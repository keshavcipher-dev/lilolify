// ============================================================================
// Lilolify — TextMetadataExtractor
// ============================================================================
// Concrete implementation of IMetadataExtractor for plain text documents.
// Extracts character snippets (first 1000 characters) and checks encoding.
// ============================================================================

#pragma once

#include <lilolify/core/interfaces/i_metadata_extractor.hpp>

namespace lilolify::infra {

/// Specialized metadata extractor for text files.
/// Validates UTF-8 encoding/binary markers and extracts content previews.
class TextMetadataExtractor : public core::IMetadataExtractor {
public:
    TextMetadataExtractor() = default;
    ~TextMetadataExtractor() override = default;

    // Prevent copy/move
    TextMetadataExtractor(const TextMetadataExtractor&) = delete;
    TextMetadataExtractor& operator=(const TextMetadataExtractor&) = delete;
    TextMetadataExtractor(TextMetadataExtractor&&) = delete;
    TextMetadataExtractor& operator=(TextMetadataExtractor&&) = delete;

    /// Supported MIME types: text/plain, text/csv, text/markdown, etc.
    [[nodiscard]] bool supports(
        const std::string& mime_type,
        const std::string& extension) const override;

    /// Validate text characteristics and extract content preview snippet.
    [[nodiscard]] core::Result<core::FileMetadata, core::Error> extract(
        const core::FilePath& path) override;
};

}  // namespace lilolify::infra
