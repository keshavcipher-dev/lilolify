// ============================================================================
// Lilolify — ScanProgress Value Object
// ============================================================================
// Lightweight progress snapshot reported during directory scanning.
// Designed to be cheaply copied and passed to UI callbacks.
// ============================================================================

#pragma once

#include <lilolify/core/types.hpp>

#include <chrono>
#include <string>

namespace lilolify::core {

/// Real-time progress snapshot during a directory scan.
/// Passed to the ScanProgressCallback at regular intervals.
///
/// This is a pure data struct — no behavior, no invariants.
/// The scanner creates and populates it; the UI consumes it.
struct ScanProgress {
    /// Total number of files discovered so far (matching filters).
    std::uint64_t files_found = 0;

    /// Total number of files processed (metadata + MIME detection complete).
    std::uint64_t files_processed = 0;

    /// Number of directories traversed so far.
    std::uint64_t directories_scanned = 0;

    /// Number of files skipped (filtered by extension, size, etc.).
    std::uint64_t files_skipped = 0;

    /// Number of errors encountered (permission denied, etc.).
    std::uint64_t errors_count = 0;

    /// Directory currently being scanned (for display in the UI).
    std::string current_directory;

    /// Wall-clock time elapsed since scan started.
    std::chrono::milliseconds elapsed{0};

    /// Estimated scanning rate (files per second).
    [[nodiscard]] double files_per_second() const noexcept {
        if (elapsed.count() == 0) return 0.0;
        return static_cast<double>(files_processed) * 1000.0 /
               static_cast<double>(elapsed.count());
    }

    /// True if the scan has encountered any errors.
    [[nodiscard]] bool has_errors() const noexcept { return errors_count > 0; }
};

/// Callback type for scan progress reporting.
/// The scanner invokes this at regular intervals (configured by ScanOptions::progress_interval).
/// The callback should be lightweight — heavy work here slows the scan.
using ScanProgressCallback = std::function<void(const ScanProgress&)>;

}  // namespace lilolify::core
