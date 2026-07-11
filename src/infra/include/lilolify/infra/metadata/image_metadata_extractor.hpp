// ============================================================================
// Lilolify — ImageMetadataExtractor
// ============================================================================
// Concrete implementation of IMetadataExtractor for images.
// Parses binary headers (JPEG, PNG, GIF, BMP) to extract dimensions.
// ============================================================================

#pragma once

#include <lilolify/core/interfaces/i_metadata_extractor.hpp>

namespace lilolify::infra {

/// Specialized metadata extractor for image files.
/// Hand-parses header bytes for PNG, JPEG, GIF, and BMP to retrieve width/height
/// without loading raw pixels into memory or spawning dependency layers.
class ImageMetadataExtractor : public core::IMetadataExtractor {
public:
    ImageMetadataExtractor() = default;
    ~ImageMetadataExtractor() override = default;

    // Prevent copy/move
    ImageMetadataExtractor(const ImageMetadataExtractor&) = delete;
    ImageMetadataExtractor& operator=(const ImageMetadataExtractor&) = delete;
    ImageMetadataExtractor(ImageMetadataExtractor&&) = delete;
    ImageMetadataExtractor& operator=(ImageMetadataExtractor&&) = delete;

    /// Supported MIME types: image/png, image/jpeg, image/gif, image/bmp.
    [[nodiscard]] bool supports(
        const std::string& mime_type,
        const std::string& extension) const override;

    /// Extract image dimensions from the binary file header.
    [[nodiscard]] core::Result<core::FileMetadata, core::Error> extract(
        const core::FilePath& path) override;

private:
    [[nodiscard]] core::Result<core::FileMetadata, core::Error> parse_png(
        std::ifstream& file) const;

    [[nodiscard]] core::Result<core::FileMetadata, core::Error> parse_gif(
        std::ifstream& file) const;

    [[nodiscard]] core::Result<core::FileMetadata, core::Error> parse_bmp(
        std::ifstream& file) const;

    [[nodiscard]] core::Result<core::FileMetadata, core::Error> parse_jpeg(
        std::ifstream& file) const;
};

}  // namespace lilolify::infra
