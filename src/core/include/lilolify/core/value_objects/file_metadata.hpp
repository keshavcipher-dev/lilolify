// ============================================================================
// Lilolify — FileMetadata Value Object
// ============================================================================
// Stores deep metadata about a file that has been scanned.
// This includes cryptographic hashes, image dimensions, document stats, etc.
// ============================================================================

#pragma once

#include <optional>
#include <string>

namespace lilolify::core {

/// Holds deep file properties extracted during the secondary metadata stage.
///
/// This is a value object containing optional fields which are populated
/// depending on the file's MIME type (e.g., width/height for images, page count
/// for PDFs, text snippet for text documents).
struct FileMetadata {
    /// SHA-256 hash of the file content (used for deduplication).
    std::string sha256;

    /// Image width in pixels (only populated for supported images).
    std::optional<std::uint32_t> image_width;

    /// Image height in pixels (only populated for supported images).
    std::optional<std::uint32_t> image_height;

    /// Total number of pages (only populated for document formats like PDF).
    std::optional<std::uint32_t> page_count;

    /// Safe text snippet (e.g. first 1000 characters) for text files.
    std::string text_preview;

    // ====================================================================
    // Value Object comparison
    // ====================================================================

    [[nodiscard]] bool operator==(const FileMetadata& other) const noexcept {
        return sha256 == other.sha256 &&
               image_width == other.image_width &&
               image_height == other.image_height &&
               page_count == other.page_count &&
               text_preview == other.text_preview;
    }

    [[nodiscard]] bool operator!=(const FileMetadata& other) const noexcept {
        return !(*this == other);
    }
};

}  // namespace lilolify::core
