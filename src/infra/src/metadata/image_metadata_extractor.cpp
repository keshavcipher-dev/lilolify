// ============================================================================
// Lilolify — ImageMetadataExtractor Implementation
// ============================================================================

#include <lilolify/infra/metadata/image_metadata_extractor.hpp>

#include <array>
#include <cmath>
#include <fstream>

namespace lilolify::infra {

bool ImageMetadataExtractor::supports(
    const std::string& mime_type,
    const std::string& extension) const {

    return mime_type == "image/png" ||
           mime_type == "image/jpeg" ||
           mime_type == "image/gif" ||
           mime_type == "image/bmp" ||
           extension == ".png" ||
           extension == ".jpg" ||
           extension == ".jpeg" ||
           extension == ".gif" ||
           extension == ".bmp";
}

core::Result<core::FileMetadata, core::Error> ImageMetadataExtractor::extract(
    const core::FilePath& path) {

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return core::Result<core::FileMetadata, core::Error>::failure(
            core::Error(core::ErrorCode::kFileNotFound,
                        "Could not open image file: " + path.string()));
    }

    // Read the first 4 bytes to check signature
    std::array<std::uint8_t, 4> signature{};
    file.read(reinterpret_cast<char*>(signature.data()), 4);
    if (file.gcount() < 4) {
        return core::Result<core::FileMetadata, core::Error>::failure(
            core::Error(core::ErrorCode::kFileReadError,
                        "File too short to match image signature: " + path.string()));
    }

    // Reset stream position
    file.seekg(0);

    // PNG: 89 50 4E 47
    if (signature[0] == 0x89 && signature[1] == 0x50 &&
        signature[2] == 0x4E && signature[3] == 0x47) {
        return parse_png(file);
    }

    // JPEG: FF D8
    if (signature[0] == 0xFF && signature[1] == 0xD8) {
        return parse_jpeg(file);
    }

    // GIF: 47 49 46 38 ("GIF8")
    if (signature[0] == 0x47 && signature[1] == 0x49 &&
        signature[2] == 0x46 && signature[3] == 0x38) {
        return parse_gif(file);
    }

    // BMP: 42 4D ("BM")
    if (signature[0] == 0x42 && signature[1] == 0x4D) {
        return parse_bmp(file);
    }

    return core::Result<core::FileMetadata, core::Error>::failure(
        core::Error(core::ErrorCode::kOcrProcessingError,
                    "Unsupported or unrecognized image signature: " + path.string()));
}

// ============================================================================
// Format Parsers
// ============================================================================

core::Result<core::FileMetadata, core::Error> ImageMetadataExtractor::parse_png(
    std::ifstream& file) const {

    // IHDR block starts after 8-byte signature.
    // Chunk length: 4B, Chunk type (IHDR): 4B, Width: 4B, Height: 4B.
    // Total skip offset = 8 (sig) + 4 (length) + 4 (type) = 16.
    file.seekg(16);

    std::array<std::uint8_t, 8> dim_bytes{};
    file.read(reinterpret_cast<char*>(dim_bytes.data()), 8);
    if (file.gcount() < 8) {
        return core::Result<core::FileMetadata, core::Error>::failure(
            core::Error(core::ErrorCode::kFileReadError, "Corrupt PNG: incomplete IHDR chunk"));
    }

    // Big-endian dimensions
    std::uint32_t width = (static_cast<std::uint32_t>(dim_bytes[0]) << 24) |
                          (static_cast<std::uint32_t>(dim_bytes[1]) << 16) |
                          (static_cast<std::uint32_t>(dim_bytes[2]) << 8) |
                          (static_cast<std::uint32_t>(dim_bytes[3]));

    std::uint32_t height = (static_cast<std::uint32_t>(dim_bytes[4]) << 24) |
                           (static_cast<std::uint32_t>(dim_bytes[5]) << 16) |
                           (static_cast<std::uint32_t>(dim_bytes[6]) << 8) |
                           (static_cast<std::uint32_t>(dim_bytes[7]));

    core::FileMetadata metadata;
    metadata.image_width = width;
    metadata.image_height = height;

    return core::Result<core::FileMetadata, core::Error>::success(std::move(metadata));
}

core::Result<core::FileMetadata, core::Error> ImageMetadataExtractor::parse_gif(
    std::ifstream& file) const {

    // Signature: "GIF87a" or "GIF89a" (6 bytes)
    // Width: 2B (little-endian) at offset 6
    // Height: 2B (little-endian) at offset 8
    file.seekg(6);

    std::array<std::uint8_t, 4> dim_bytes{};
    file.read(reinterpret_cast<char*>(dim_bytes.data()), 4);
    if (file.gcount() < 4) {
        return core::Result<core::FileMetadata, core::Error>::failure(
            core::Error(core::ErrorCode::kFileReadError, "Corrupt GIF: incomplete header"));
    }

    std::uint32_t width = dim_bytes[0] | (static_cast<std::uint32_t>(dim_bytes[1]) << 8);
    std::uint32_t height = dim_bytes[2] | (static_cast<std::uint32_t>(dim_bytes[3]) << 8);

    core::FileMetadata metadata;
    metadata.image_width = width;
    metadata.image_height = height;

    return core::Result<core::FileMetadata, core::Error>::success(std::move(metadata));
}

core::Result<core::FileMetadata, core::Error> ImageMetadataExtractor::parse_bmp(
    std::ifstream& file) const {

    // File header is 14 bytes. DIB header starts at offset 14.
    // BITMAPINFOHEADER width is at offset 18 (4B), height is at offset 22 (4B) - little-endian.
    file.seekg(18);

    std::array<std::uint8_t, 8> dim_bytes{};
    file.read(reinterpret_cast<char*>(dim_bytes.data()), 8);
    if (file.gcount() < 8) {
        return core::Result<core::FileMetadata, core::Error>::failure(
            core::Error(core::ErrorCode::kFileReadError, "Corrupt BMP: incomplete DIB header"));
    }

    std::int32_t w = static_cast<std::int32_t>(
        dim_bytes[0] |
        (static_cast<std::uint32_t>(dim_bytes[1]) << 8) |
        (static_cast<std::uint32_t>(dim_bytes[2]) << 16) |
        (static_cast<std::uint32_t>(dim_bytes[3]) << 24));

    std::int32_t h = static_cast<std::int32_t>(
        dim_bytes[4] |
        (static_cast<std::uint32_t>(dim_bytes[5]) << 8) |
        (static_cast<std::uint32_t>(dim_bytes[6]) << 16) |
        (static_cast<std::uint32_t>(dim_bytes[7]) << 24));

    core::FileMetadata metadata;
    metadata.image_width = static_cast<std::uint32_t>(std::abs(w));
    metadata.image_height = static_cast<std::uint32_t>(std::abs(h));

    return core::Result<core::FileMetadata, core::Error>::success(std::move(metadata));
}

core::Result<core::FileMetadata, core::Error> ImageMetadataExtractor::parse_jpeg(
    std::ifstream& file) const {

    // Skip JPEG SOI (FF D8)
    file.seekg(2);

    while (file.good()) {
        std::uint8_t marker_prefix = 0;
        file.read(reinterpret_cast<char*>(&marker_prefix), 1);
        if (file.gcount() < 1) break;

        if (marker_prefix != 0xFF) {
            // Re-sync byte if we get garbage
            continue;
        }

        std::uint8_t marker_type = 0;
        file.read(reinterpret_cast<char*>(&marker_type), 1);
        if (file.gcount() < 1) break;

        // Skip stuffing bytes (multiple FFs in a row)
        while (marker_type == 0xFF) {
            file.read(reinterpret_cast<char*>(&marker_type), 1);
            if (file.gcount() < 1) break;
        }

        // SOS (Start of Scan) or EOI (End of Image) indicates main image data start;
        // metadata markers are always before this, so we stop if we hit these.
        if (marker_type == 0xDA || marker_type == 0xD9) {
            break;
        }

        // Read segment length (2 bytes, big-endian)
        std::array<std::uint8_t, 2> length_bytes{};
        file.read(reinterpret_cast<char*>(length_bytes.data()), 2);
        if (file.gcount() < 2) break;

        std::uint16_t length = (static_cast<std::uint16_t>(length_bytes[0]) << 8) |
                               length_bytes[1];

        // SOF markers: FF C0 through FF CF (excluding DHT FF C4 and DAC FF C8)
        if (marker_type >= 0xC0 && marker_type <= 0xCF &&
            marker_type != 0xC4 && marker_type != 0xC8) {

            // SOF layout:
            // - Precision: 1 byte
            // - Height: 2 bytes (big-endian)
            // - Width: 2 bytes (big-endian)
            // We skip precision (1 byte)
            file.seekg(file.tellg() + std::streamoff(1));

            std::array<std::uint8_t, 4> dim_bytes{};
            file.read(reinterpret_cast<char*>(dim_bytes.data()), 4);
            if (file.gcount() < 4) {
                return core::Result<core::FileMetadata, core::Error>::failure(
                    core::Error(core::ErrorCode::kFileReadError, "Corrupt JPEG: incomplete SOF segment"));
            }

            std::uint32_t height = (static_cast<std::uint32_t>(dim_bytes[0]) << 8) | dim_bytes[1];
            std::uint32_t width = (static_cast<std::uint32_t>(dim_bytes[2]) << 8) | dim_bytes[3];

            core::FileMetadata metadata;
            metadata.image_width = width;
            metadata.image_height = height;

            return core::Result<core::FileMetadata, core::Error>::success(std::move(metadata));
        } else {
            // Skip the rest of this segment
            // Subtract 2 because length field itself is included in length value
            if (length < 2) {
                return core::Result<core::FileMetadata, core::Error>::failure(
                    core::Error(core::ErrorCode::kFileReadError, "Corrupt JPEG: invalid segment length"));
            }
            file.seekg(file.tellg() + std::streamoff(length - 2));
        }
    }

    return core::Result<core::FileMetadata, core::Error>::failure(
        core::Error(core::ErrorCode::kOcrProcessingError,
                    "Failed to find SOF marker in JPEG file"));
}

}  // namespace lilolify::infra
