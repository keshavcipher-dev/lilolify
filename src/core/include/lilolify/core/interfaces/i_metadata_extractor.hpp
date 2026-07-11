// ============================================================================
// Lilolify — IMetadataExtractor Interface (Port)
// ============================================================================
// Abstract port interface for file-format-specific metadata extraction.
// ============================================================================

#pragma once

#include <lilolify/core/error.hpp>
#include <lilolify/core/result.hpp>
#include <lilolify/core/types.hpp>
#include <lilolify/core/value_objects/file_metadata.hpp>

#include <string>

namespace lilolify::core {

/// Abstract interface for extracting type-specific metadata from a file.
/// Implementations handle specific classes of formats (e.g. images, PDFs, text).
class IMetadataExtractor {
public:
    virtual ~IMetadataExtractor() = default;

    IMetadataExtractor() = default;
    IMetadataExtractor(const IMetadataExtractor&) = delete;
    IMetadataExtractor& operator=(const IMetadataExtractor&) = delete;
    IMetadataExtractor(IMetadataExtractor&&) = delete;
    IMetadataExtractor& operator=(IMetadataExtractor&&) = delete;

    /// Returns true if this extractor supports the given file properties.
    [[nodiscard]] virtual bool supports(
        const std::string& mime_type,
        const std::string& extension) const = 0;

    /// Extract type-specific metadata.
    ///
    /// @param path  Absolute path to the file.
    /// @return FileMetadata object containing populated optional fields on success,
    ///         or Error if extraction fails due to format corruption or I/O.
    [[nodiscard]] virtual Result<FileMetadata, Error> extract(const FilePath& path) = 0;
};

}  // namespace lilolify::core
