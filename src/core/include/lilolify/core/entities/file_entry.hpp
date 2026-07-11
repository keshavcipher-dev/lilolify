// ============================================================================
// Lilolify — FileEntry Entity
// ============================================================================
// The central data structure representing a discovered file in the system.
// Every file that Lilolify processes — from initial scan through AI analysis
// to final organization — is tracked as a FileEntry.
//
// Design Decision:
//   FileEntry is a MUTABLE entity (not an immutable value object) because:
//   1. It has identity (its file path uniquely identifies it)
//   2. It has lifecycle (status changes as it moves through the pipeline)
//   3. It accumulates data over time (MIME type, analysis results, etc.)
//
//   This contrasts with value objects like ScanOptions or Confidence, which
//   are immutable and compared by value.
//
// Alternative Considered:
//   Immutable FileEntry + separate FileState tracker. Rejected because it
//   adds unnecessary indirection for a desktop app where the entity lifecycle
//   is straightforward and doesn't require event sourcing.
// ============================================================================

#pragma once

#include <lilolify/core/types.hpp>
#include <lilolify/core/value_objects/file_metadata.hpp>

#include <string>
#include <utility>

namespace lilolify::core {

/// Represents a discovered file with all metadata needed by the AI pipeline.
///
/// Lifecycle:
///   1. Created by the scanner with path, size, and timestamps
///   2. Enriched with MIME type during scanning
///   3. Status updated as the file progresses through the pipeline
///   4. Persisted to the database for history and undo support
///
/// Identity:
///   Two FileEntry objects are considered the same if they refer to the same
///   absolute file path. The `id` field is a database-assigned surrogate key.
class FileEntry {
public:
    // ====================================================================
    // Construction
    // ====================================================================

    /// Create a FileEntry from scan data.
    /// This is the primary construction path — called by FilesystemScanner.
    ///
    /// @param path        Absolute path to the file
    /// @param size_bytes  File size in bytes
    /// @param created_at  File creation timestamp
    /// @param modified_at File last-modification timestamp
    FileEntry(FilePath path,
              FileSize size_bytes,
              Timestamp created_at,
              Timestamp modified_at) noexcept
        : id_(kInvalidEntityId),
          path_(std::move(path)),
          size_bytes_(size_bytes),
          created_at_(created_at),
          modified_at_(modified_at),
          scanned_at_(std::chrono::system_clock::now()),
          status_(ProcessingStatus::kPending),
          metadata_(std::nullopt) {
        // Cache filename and extension to avoid repeated path decomposition
        filename_ = path_.filename().string();
        extension_ = path_.extension().string();
        // Normalize extension to lowercase for consistent matching
        for (auto& ch : extension_) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
    }

    // -- Rule of Five: defaulted (movable, copyable) --
    FileEntry(const FileEntry&) = default;
    FileEntry& operator=(const FileEntry&) = default;
    FileEntry(FileEntry&&) noexcept = default;
    FileEntry& operator=(FileEntry&&) noexcept = default;
    ~FileEntry() = default;

    // ====================================================================
    // Accessors (const)
    // ====================================================================

    /// Database surrogate key. Returns kInvalidEntityId if not yet persisted.
    [[nodiscard]] EntityId id() const noexcept { return id_; }

    /// Absolute path to the file on disk.
    [[nodiscard]] const FilePath& path() const noexcept { return path_; }

    /// Cached filename (e.g., "report.pdf").
    [[nodiscard]] const std::string& filename() const noexcept { return filename_; }

    /// Normalized lowercase extension including the dot (e.g., ".pdf").
    /// Empty string if the file has no extension.
    [[nodiscard]] const std::string& extension() const noexcept { return extension_; }

    /// File size in bytes.
    [[nodiscard]] FileSize size_bytes() const noexcept { return size_bytes_; }

    /// Detected MIME type (e.g., "image/jpeg"). Empty until MIME detection runs.
    [[nodiscard]] const std::string& mime_type() const noexcept { return mime_type_; }

    /// File creation timestamp (from the OS).
    [[nodiscard]] Timestamp created_at() const noexcept { return created_at_; }

    /// File last-modification timestamp (from the OS).
    [[nodiscard]] Timestamp modified_at() const noexcept { return modified_at_; }

    /// Timestamp when Lilolify scanned this file.
    [[nodiscard]] Timestamp scanned_at() const noexcept { return scanned_at_; }

    /// Current processing status in the AI pipeline.
    [[nodiscard]] ProcessingStatus status() const noexcept { return status_; }

    /// Deep file metadata. std::nullopt if not yet extracted.
    [[nodiscard]] const std::optional<FileMetadata>& metadata() const noexcept { return metadata_; }

    // ====================================================================
    // Queries
    // ====================================================================

    /// True if this entity has been persisted to the database.
    [[nodiscard]] bool is_persisted() const noexcept { return id_ != kInvalidEntityId; }

    /// True if the file is an image based on MIME type.
    [[nodiscard]] bool is_image() const noexcept {
        return mime_type_.starts_with("image/");
    }

    /// True if the file is a document (PDF, Office, etc.).
    [[nodiscard]] bool is_document() const noexcept {
        return mime_type_.starts_with("application/pdf") ||
               mime_type_.starts_with("application/msword") ||
               mime_type_.starts_with("application/vnd.");
    }

    /// Human-readable file size (e.g., "2.3 MB").
    [[nodiscard]] std::string human_readable_size() const {
        constexpr FileSize kKB = 1024;
        constexpr FileSize kMB = 1024 * kKB;
        constexpr FileSize kGB = 1024 * kMB;

        if (size_bytes_ >= kGB) {
            return std::to_string(size_bytes_ / kGB) + "." +
                   std::to_string((size_bytes_ % kGB) * 10 / kGB) + " GB";
        }
        if (size_bytes_ >= kMB) {
            return std::to_string(size_bytes_ / kMB) + "." +
                   std::to_string((size_bytes_ % kMB) * 10 / kMB) + " MB";
        }
        if (size_bytes_ >= kKB) {
            return std::to_string(size_bytes_ / kKB) + "." +
                   std::to_string((size_bytes_ % kKB) * 10 / kKB) + " KB";
        }
        return std::to_string(size_bytes_) + " B";
    }

    // ====================================================================
    // Mutators (controlled state changes)
    // ====================================================================

    /// Set the database ID after persistence. May only be called once.
    void set_id(EntityId id) noexcept { id_ = id; }

    /// Set the detected MIME type (called by MimeDetector during scanning).
    void set_mime_type(std::string mime_type) noexcept {
        mime_type_ = std::move(mime_type);
    }

    /// Update the processing status (called as the file moves through pipeline).
    void set_status(ProcessingStatus status) noexcept { status_ = status; }

    /// Set extracted deep file metadata.
    void set_metadata(FileMetadata metadata) noexcept {
        metadata_ = std::move(metadata);
    }

private:
    EntityId id_;
    FilePath path_;
    std::string filename_;
    std::string extension_;
    FileSize size_bytes_;
    std::string mime_type_;
    Timestamp created_at_;
    Timestamp modified_at_;
    Timestamp scanned_at_;
    ProcessingStatus status_;
    std::optional<FileMetadata> metadata_;
};

}  // namespace lilolify::core
