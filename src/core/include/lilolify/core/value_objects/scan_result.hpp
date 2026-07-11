// ============================================================================
// Lilolify — ScanResult Value Object
// ============================================================================
// The output of a completed (or cancelled) directory scan.
// Contains all discovered files, any errors encountered, and final stats.
// ============================================================================

#pragma once

#include <lilolify/core/entities/file_entry.hpp>
#include <lilolify/core/error.hpp>
#include <lilolify/core/value_objects/scan_progress.hpp>

#include <string>
#include <vector>

namespace lilolify::core {

/// Output of a completed directory scan operation.
///
/// A ScanResult is always returned — even if the scan was cancelled or
/// encountered errors. The `was_cancelled` flag and `errors` vector tell
/// the caller what happened.
///
/// This design avoids losing partial results: if 8,000 out of 10,000 files
/// were scanned before cancellation, all 8,000 are still available.
struct ScanResult {
    /// All files discovered during the scan (matching filters).
    std::vector<FileEntry> files;

    /// Errors encountered during scanning (one per problematic file/directory).
    /// These are non-fatal — the scan continued past them.
    std::vector<std::string> errors;

    /// Final progress snapshot at scan completion.
    ScanProgress final_progress;

    /// True if the scan was cancelled by the user before completion.
    bool was_cancelled = false;

    // ====================================================================
    // Convenience Accessors
    // ====================================================================

    /// Total number of files discovered.
    [[nodiscard]] std::size_t file_count() const noexcept { return files.size(); }

    /// Total number of errors encountered.
    [[nodiscard]] std::size_t error_count() const noexcept { return errors.size(); }

    /// True if the scan completed without any errors.
    [[nodiscard]] bool is_clean() const noexcept {
        return errors.empty() && !was_cancelled;
    }

    /// Total size of all discovered files in bytes.
    [[nodiscard]] FileSize total_size_bytes() const noexcept {
        FileSize total = 0;
        for (const auto& file : files) {
            total += file.size_bytes();
        }
        return total;
    }
};

}  // namespace lilolify::core
