// ============================================================================
// Lilolify — ScanOptions Value Object
// ============================================================================
// Configuration parameters for a directory scan operation.
//
// Design Decision:
//   ScanOptions is a simple aggregate with sensible defaults. We chose NOT
//   to use a Builder pattern because:
//   1. C++20 designated initializers make construction readable:
//      auto opts = ScanOptions{.root_directory = path, .recursive = true};
//   2. All fields have safe defaults — you can construct with just a path
//   3. The Builder pattern adds boilerplate without real benefit here
//
//   Validation is done via a separate validate() method that returns
//   Result<void, Error>, keeping the constructor trivial and testable.
// ============================================================================

#pragma once

#include <lilolify/core/error.hpp>
#include <lilolify/core/result.hpp>
#include <lilolify/core/types.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace lilolify::core {

/// Configuration for a directory scan operation.
/// All fields have sensible defaults — only root_directory is required.
struct ScanOptions {
    /// Root directory to scan. REQUIRED — validation fails if empty.
    FilePath root_directory;

    /// Whether to recurse into subdirectories.
    bool recursive = true;

    /// Maximum recursion depth (0 = root only, 1 = root + one level, etc.).
    /// Default 50 is generous but prevents infinite symlink loops.
    std::uint32_t max_depth = 50;

    /// Skip files larger than this (bytes). Default 100 MB.
    /// Large files (videos, disk images) are usually not what Lilolify organizes.
    FileSize max_file_size = 100ULL * 1024 * 1024;

    /// Skip files smaller than this (bytes). Default 0 (no minimum).
    /// Can be used to skip empty or near-empty files.
    FileSize min_file_size = 0;

    /// Only include files with these extensions (e.g., {".jpg", ".png"}).
    /// Empty means include ALL extensions. Extensions should include the dot.
    std::vector<std::string> include_extensions;

    /// Exclude files with these extensions (e.g., {".tmp", ".bak"}).
    /// Applied after include_extensions filter.
    std::vector<std::string> exclude_extensions;

    /// Directories to skip entirely (absolute paths).
    std::vector<FilePath> exclude_directories;

    /// Report progress every N files discovered.
    /// Lower values = more responsive UI but slightly more overhead.
    std::uint32_t progress_interval = 100;

    /// Whether to follow symbolic links during traversal.
    /// Default false to prevent infinite loops and unexpected behavior.
    bool follow_symlinks = false;

    /// Whether to detect MIME type via magic bytes (slightly slower but accurate).
    /// If false, MIME type is guessed from extension only.
    bool detect_mime_type = true;

    // ====================================================================
    // Validation
    // ====================================================================

    /// Validate the scan options, returning an error if any field is invalid.
    [[nodiscard]] Result<void, Error> validate() const {
        if (root_directory.empty()) {
            return Result<void, Error>::failure(
                Error(ErrorCode::kInvalidArgument, "Root directory path is empty"));
        }

        if (min_file_size > max_file_size) {
            return Result<void, Error>::failure(
                Error(ErrorCode::kInvalidArgument,
                      "min_file_size (" + std::to_string(min_file_size) +
                          ") > max_file_size (" + std::to_string(max_file_size) + ")"));
        }

        if (progress_interval == 0) {
            return Result<void, Error>::failure(
                Error(ErrorCode::kInvalidArgument, "progress_interval must be > 0"));
        }

        return Result<void, Error>::success();
    }

    // ====================================================================
    // Helpers
    // ====================================================================

    /// Normalize all extensions to lowercase for case-insensitive matching.
    /// Call this after setting include/exclude extensions.
    void normalize_extensions() {
        auto to_lower = [](std::string& ext) {
            for (auto& ch : ext) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            // Ensure extension starts with a dot
            if (!ext.empty() && ext[0] != '.') {
                ext = "." + ext;
            }
        };

        for (auto& ext : include_extensions) {
            to_lower(ext);
        }
        for (auto& ext : exclude_extensions) {
            to_lower(ext);
        }
    }

    /// Check if a file extension passes the include/exclude filters.
    /// Extension should already be normalized to lowercase.
    [[nodiscard]] bool is_extension_allowed(const std::string& ext) const {
        // If include list is non-empty, extension must be in it
        if (!include_extensions.empty()) {
            bool found = std::find(include_extensions.begin(),
                                   include_extensions.end(),
                                   ext) != include_extensions.end();
            if (!found) return false;
        }

        // Check exclude list
        if (!exclude_extensions.empty()) {
            bool excluded = std::find(exclude_extensions.begin(),
                                      exclude_extensions.end(),
                                      ext) != exclude_extensions.end();
            if (excluded) return false;
        }

        return true;
    }

    /// Check if a file size is within the allowed range.
    [[nodiscard]] bool is_size_allowed(FileSize size) const noexcept {
        return size >= min_file_size && size <= max_file_size;
    }
};

}  // namespace lilolify::core
