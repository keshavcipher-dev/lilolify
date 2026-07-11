// ============================================================================
// Lilolify — MetadataEngine Orchestrator
// ============================================================================
// Core service coordinating hashing and format-specific metadata extraction.
// ============================================================================

#pragma once

#include <lilolify/core/error.hpp>
#include <lilolify/core/interfaces/i_hash_calculator.hpp>
#include <lilolify/core/interfaces/i_metadata_extractor.hpp>
#include <lilolify/core/result.hpp>
#include <lilolify/core/types.hpp>
#include <lilolify/core/value_objects/file_metadata.hpp>

#include <memory>
#include <vector>

namespace lilolify::core {

/// Coordinates metadata extraction by combining hashing with type-specific extractors.
class MetadataEngine {
public:
    /// Construct the engine with required hash calculator and list of extractors.
    ///
    /// @param hash_calc   Concrete hash calculator implementation
    /// @param extractors  List of concrete extractors (ordered by priority)
    MetadataEngine(
        std::shared_ptr<IHashCalculator> hash_calc,
        std::vector<std::shared_ptr<IMetadataExtractor>> extractors) noexcept;

    ~MetadataEngine() = default;

    // Prevent copying, allow move
    MetadataEngine(const MetadataEngine&) = delete;
    MetadataEngine& operator=(const MetadataEngine&) = delete;
    MetadataEngine(MetadataEngine&&) noexcept = default;
    MetadataEngine& operator=(MetadataEngine&&) noexcept = default;

    /// Process a file: compute SHA-256 and run supporting extractors to enrich metadata.
    ///
    /// @param path       Absolute path to the file
    /// @param mime_type  Pre-detected MIME type of the file
    /// @param extension  Normalized lowercase file extension
    /// @return Populated FileMetadata object on success, or Error on failure.
    [[nodiscard]] Result<FileMetadata, Error> extract_metadata(
        const FilePath& path,
        const std::string& mime_type,
        const std::string& extension);

private:
    std::shared_ptr<IHashCalculator> hash_calc_;
    std::vector<std::shared_ptr<IMetadataExtractor>> extractors_;
};

}  // namespace lilolify::core
