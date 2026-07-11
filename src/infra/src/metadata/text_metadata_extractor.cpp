// ============================================================================
// Lilolify — TextMetadataExtractor Implementation
// ============================================================================

#include <lilolify/infra/metadata/text_metadata_extractor.hpp>

#include <fstream>
#include <vector>

namespace lilolify::infra {

namespace {

/// Checks if a byte is a valid standard ASCII print character or text spacing.
bool is_text_char(std::uint8_t byte) noexcept {
    if (byte >= 32 && byte <= 126) return true; // Standard printable ASCII
    if (byte == '\t' || byte == '\n' || byte == '\r') return true; // Whitespace
    if (byte >= 128) return true; // Keep UTF-8 / high-byte characters (validated later)
    return false;
}

/// Simple UTF-8 validation and replacement filter.
/// Replaces invalid UTF-8 byte sequences with '?' to ensure safe text.
std::string clean_utf8(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    std::size_t i = 0;
    while (i < input.size()) {
        auto c = static_cast<std::uint8_t>(input[i]);

        if (c <= 0x7F) {
            // 1-byte ASCII
            result.push_back(input[i]);
            i += 1;
        } else if (c >= 0xC2 && c <= 0xDF) {
            // 2-byte sequence
            if (i + 1 < input.size() &&
                static_cast<std::uint8_t>(input[i + 1]) >= 0x80 &&
                static_cast<std::uint8_t>(input[i + 1]) <= 0xBF) {
                result.push_back(input[i]);
                result.push_back(input[i + 1]);
                i += 2;
            } else {
                result.push_back('?');
                i += 1;
            }
        } else if (c >= 0xE0 && c <= 0xEF) {
            // 3-byte sequence
            if (i + 2 < input.size() &&
                static_cast<std::uint8_t>(input[i + 1]) >= 0x80 &&
                static_cast<std::uint8_t>(input[i + 1]) <= 0xBF &&
                static_cast<std::uint8_t>(input[i + 2]) >= 0x80 &&
                static_cast<std::uint8_t>(input[i + 2]) <= 0xBF) {
                result.push_back(input[i]);
                result.push_back(input[i + 1]);
                result.push_back(input[i + 2]);
                i += 3;
            } else {
                result.push_back('?');
                i += 1;
            }
        } else if (c >= 0xF0 && c <= 0xF4) {
            // 4-byte sequence
            if (i + 3 < input.size() &&
                static_cast<std::uint8_t>(input[i + 1]) >= 0x80 &&
                static_cast<std::uint8_t>(input[i + 1]) <= 0xBF &&
                static_cast<std::uint8_t>(input[i + 2]) >= 0x80 &&
                static_cast<std::uint8_t>(input[i + 2]) <= 0xBF &&
                static_cast<std::uint8_t>(input[i + 3]) >= 0x80 &&
                static_cast<std::uint8_t>(input[i + 3]) <= 0xBF) {
                result.push_back(input[i]);
                result.push_back(input[i + 1]);
                result.push_back(input[i + 2]);
                result.push_back(input[i + 3]);
                i += 4;
            } else {
                result.push_back('?');
                i += 1;
            }
        } else {
            // Invalid lead byte
            result.push_back('?');
            i += 1;
        }
    }

    return result;
}

}  // namespace

// ============================================================================
// TextMetadataExtractor Implementation
// ============================================================================

bool TextMetadataExtractor::supports(
    const std::string& mime_type,
    const std::string& extension) const {

    return mime_type == "text/plain" ||
           mime_type == "text/csv" ||
           mime_type == "text/markdown" ||
           extension == ".txt" ||
           extension == ".csv" ||
           extension == ".md" ||
           extension == ".json" ||
           extension == ".xml" ||
           extension == ".cpp" ||
           extension == ".hpp" ||
           extension == ".c" ||
           extension == ".h" ||
           extension == ".py" ||
           extension == ".rs";
}

core::Result<core::FileMetadata, core::Error> TextMetadataExtractor::extract(
    const core::FilePath& path) {

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return core::Result<core::FileMetadata, core::Error>::failure(
            core::Error(core::ErrorCode::kFileNotFound,
                        "Could not open text file: " + path.string()));
    }

    // Read first 2 KB for snippet and binary checking
    constexpr std::size_t kReadSize = 2048;
    std::vector<char> buffer(kReadSize);
    file.read(buffer.data(), kReadSize);
    auto bytes_read = static_cast<std::size_t>(file.gcount());

    if (bytes_read == 0) {
        // Empty text file
        core::FileMetadata metadata;
        metadata.text_preview = "";
        return core::Result<core::FileMetadata, core::Error>::success(std::move(metadata));
    }

    // 1. Binary check: inspect read buffer for null bytes or invalid control bytes
    for (std::size_t i = 0; i < bytes_read; ++i) {
        auto byte = static_cast<std::uint8_t>(buffer[i]);
        if (byte == 0x00) {
            return core::Result<core::FileMetadata, core::Error>::failure(
                core::Error(core::ErrorCode::kOcrProcessingError,
                            "File contains binary null characters (rejected as text): " + path.string()));
        }
        if (!is_text_char(byte)) {
            // If more than 2% of the file contains raw non-text control characters, reject
            // For simple strictness, we check if we hit control codes in first few bytes.
            // Let's count them or return a failure for safety.
            // We'll allow a small threshold or fail on any invalid control code.
            return core::Result<core::FileMetadata, core::Error>::failure(
                core::Error(core::ErrorCode::kOcrProcessingError,
                            "File contains non-text control characters (rejected as text): " + path.string()));
        }
    }

    // 2. Build snippet (up to 1000 characters)
    std::string raw_snippet(buffer.data(), std::min(bytes_read, static_cast<std::size_t>(1000)));

    // 3. Clean invalid UTF-8 sequences
    std::string clean_snippet = clean_utf8(raw_snippet);

    core::FileMetadata metadata;
    metadata.text_preview = std::move(clean_snippet);

    return core::Result<core::FileMetadata, core::Error>::success(std::move(metadata));
}

}  // namespace lilolify::infra
