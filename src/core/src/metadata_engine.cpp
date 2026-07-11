// ============================================================================
// Lilolify — MetadataEngine Orchestrator Implementation
// ============================================================================

#include <lilolify/core/services/metadata_engine.hpp>

namespace lilolify::core {

MetadataEngine::MetadataEngine(
    std::shared_ptr<IHashCalculator> hash_calc,
    std::vector<std::shared_ptr<IMetadataExtractor>> extractors) noexcept
    : hash_calc_(std::move(hash_calc)), extractors_(std::move(extractors)) {}

Result<FileMetadata, Error> MetadataEngine::extract_metadata(
    const FilePath& path,
    const std::string& mime_type,
    const std::string& extension) {

    if (!hash_calc_) {
        return Result<FileMetadata, Error>::failure(
            Error(ErrorCode::kInvalidArgument, "Hash calculator is null"));
    }

    // 1. Calculate SHA-256 (mandatory for all files)
    auto hash_result = hash_calc_->calculate_sha256(path);
    if (hash_result.has_error()) {
        // Wrap error with file path context
        return Result<FileMetadata, Error>::failure(
            Error(ErrorCode::kFileReadError,
                  "Failed to hash file: " + path.string(),
                  std::move(hash_result).error()));
    }

    FileMetadata metadata;
    metadata.sha256 = std::move(hash_result).value();

    // 2. Dispatch to the first supporting extractor (if any)
    for (const auto& extractor : extractors_) {
        if (extractor && extractor->supports(mime_type, extension)) {
            auto extract_result = extractor->extract(path);
            if (extract_result.has_error()) {
                // Return partial metadata containing at least the SHA-256 hash,
                // but log/collect the error or bubble it up depending on strictness.
                // For production robustness, we fail if a registered extractor fails,
                // indicating file corruption or access denial.
                return Result<FileMetadata, Error>::failure(
                    Error(ErrorCode::kOcrProcessingError, // placeholder or general extractor error
                          "Failed to extract metadata for: " + path.string(),
                          std::move(extract_result).error()));
            }

            // Merge extracted fields with our core SHA-256 hash
            auto ext_meta = std::move(extract_result).value();
            metadata.image_width = ext_meta.image_width;
            metadata.image_height = ext_meta.image_height;
            metadata.page_count = ext_meta.page_count;
            metadata.text_preview = std::move(ext_meta.text_preview);
            break; // First match wins
        }
    }

    return Result<FileMetadata, Error>::success(std::move(metadata));
}

}  // namespace lilolify::core
