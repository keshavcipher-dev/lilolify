// ============================================================================
// Lilolify — PdfMetadataExtractor
// ============================================================================
// Concrete implementation of IMetadataExtractor for PDF documents.
// Extracts basic info (page count, etc.) from PDF binary trailers.
// ============================================================================

#pragma once

#include <lilolify/core/interfaces/i_metadata_extractor.hpp>

namespace lilolify::infra {

/// Specialized metadata extractor for PDF files.
/// Scans object structures inside the PDF to retrieve the page count
/// without pulling in full heavy PDF engines.
class PdfMetadataExtractor : public core::IMetadataExtractor {
public:
    PdfMetadataExtractor() = default;
    ~PdfMetadataExtractor() override = default;

    // Prevent copy/move
    PdfMetadataExtractor(const PdfMetadataExtractor&) = delete;
    PdfMetadataExtractor& operator=(const PdfMetadataExtractor&) = delete;
    PdfMetadataExtractor(PdfMetadataExtractor&&) = delete;
    PdfMetadataExtractor& operator=(PdfMetadataExtractor&&) = delete;

    /// Supported MIME types: application/pdf, extension: .pdf.
    [[nodiscard]] bool supports(
        const std::string& mime_type,
        const std::string& extension) const override;

    /// Extract page count from PDF document structure.
    [[nodiscard]] core::Result<core::FileMetadata, core::Error> extract(
        const core::FilePath& path) override;
};

}  // namespace lilolify::infra
