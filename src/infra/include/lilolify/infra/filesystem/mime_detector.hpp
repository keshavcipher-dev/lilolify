// ============================================================================
// Lilolify — MIME Type Detector
// ============================================================================
// Two-layer MIME type detection: fast extension lookup + accurate magic byte
// verification for file content validation.
//
// Design Decision:
//   We use a custom implementation instead of libmagic because:
//   1. libmagic is Unix-only and adds a heavy C dependency
//   2. We only need ~20 file types relevant to Lilolify (images, docs, PDFs)
//   3. A custom magic byte table is lighter, portable, and faster
//   4. Zero external dependencies — pure C++20
//
// Security Consideration:
//   Extensions can be spoofed (virus.exe → photo.jpg). Magic byte detection
//   validates actual file content, providing an essential security layer.
// ============================================================================

#pragma once

#include <lilolify/core/types.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lilolify::infra {

/// Detects MIME types using a two-layer strategy:
/// 1. Extension-to-MIME map (O(1) hash lookup, fast but unreliable)
/// 2. Magic byte signatures (reads first 16 bytes, accurate but needs file I/O)
///
/// Usage:
///   MimeDetector detector;
///   auto mime = detector.detect_from_extension(".jpg");  // "image/jpeg"
///   auto verified = detector.detect_from_file(path);     // Reads magic bytes
class MimeDetector {
public:
    MimeDetector();

    /// Detect MIME type from file extension only (fast, no I/O).
    /// Extension should include the dot and be lowercase (e.g., ".jpg").
    /// Returns "application/octet-stream" for unknown extensions.
    [[nodiscard]] std::string detect_from_extension(std::string_view extension) const;

    /// Detect MIME type by reading the file's magic bytes (accurate, requires I/O).
    /// Falls back to extension-based detection if magic bytes don't match.
    /// Returns "application/octet-stream" if both methods fail.
    [[nodiscard]] std::string detect_from_file(const core::FilePath& path) const;

    /// Check if a MIME type represents an image.
    [[nodiscard]] static bool is_image_mime(std::string_view mime) noexcept;

    /// Check if a MIME type represents a document.
    [[nodiscard]] static bool is_document_mime(std::string_view mime) noexcept;

    /// The default MIME type for unrecognized files.
    static constexpr std::string_view kUnknownMime = "application/octet-stream";

private:
    /// A magic byte signature entry.
    struct MagicSignature {
        std::string mime_type;               // The MIME type this signature identifies
        std::vector<std::uint8_t> bytes;     // The expected byte sequence
        std::size_t offset;                  // Byte offset where the signature starts
    };

    /// Initialize the extension-to-MIME map.
    void init_extension_map();

    /// Initialize the magic byte signature database.
    void init_magic_signatures();

    /// Read the first N bytes of a file.
    [[nodiscard]] std::vector<std::uint8_t> read_header(
        const core::FilePath& path,
        std::size_t max_bytes) const;

    /// Match file header against the magic signature database.
    [[nodiscard]] std::string match_magic_bytes(
        const std::vector<std::uint8_t>& header) const;

    std::unordered_map<std::string, std::string> extension_map_;
    std::vector<MagicSignature> magic_signatures_;
};

}  // namespace lilolify::infra
