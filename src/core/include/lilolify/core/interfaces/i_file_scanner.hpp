// ============================================================================
// Lilolify — IFileScanner Interface (Port)
// ============================================================================
// Abstract interface defining the contract for directory scanning.
// This is a PORT in Clean Architecture — the domain defines WHAT scanning
// means; the infrastructure provides HOW it's done.
//
// Design Decision:
//   Pure virtual interface (no implementation) ensures:
//   1. The domain layer has zero knowledge of std::filesystem
//   2. Swapping scanner implementations (local, cloud, mock) requires
//      no changes to the domain or application layers
//   3. Unit tests can inject a mock scanner without touching real files
// ============================================================================

#pragma once

#include <lilolify/core/error.hpp>
#include <lilolify/core/result.hpp>
#include <lilolify/core/value_objects/scan_options.hpp>
#include <lilolify/core/value_objects/scan_progress.hpp>
#include <lilolify/core/value_objects/scan_result.hpp>

namespace lilolify::core {

/// Abstract interface for directory scanning operations.
///
/// Implementations:
///   - FilesystemScanner: scans local filesystem via std::filesystem
///   - (future) CloudScanner: scans cloud storage (Google Drive, OneDrive)
///   - MockScanner: returns predefined results for testing
///
/// Thread Safety:
///   scan() runs on a worker thread. cancel() may be called from any thread.
///   Implementations must ensure cancel() is thread-safe.
class IFileScanner {
public:
    virtual ~IFileScanner() = default;

    // Prevent copying and moving of interface pointers
    IFileScanner() = default;
    IFileScanner(const IFileScanner&) = delete;
    IFileScanner& operator=(const IFileScanner&) = delete;
    IFileScanner(IFileScanner&&) = delete;
    IFileScanner& operator=(IFileScanner&&) = delete;

    /// Scan a directory according to the given options.
    ///
    /// @param options      Scan configuration (root path, filters, etc.)
    /// @param on_progress  Optional callback invoked at regular intervals.
    ///                     May be nullptr if progress reporting is not needed.
    /// @return ScanResult on success (may contain partial results + errors),
    ///         or Error if the scan could not even start (e.g., invalid path).
    [[nodiscard]] virtual Result<ScanResult, Error> scan(
        const ScanOptions& options,
        ScanProgressCallback on_progress = nullptr) = 0;

    /// Request cancellation of an ongoing scan.
    /// This is thread-safe and may be called from any thread.
    /// The scan will stop at the next file boundary and return partial results.
    virtual void cancel() = 0;

    /// Returns true if a scan is currently in progress.
    [[nodiscard]] virtual bool is_scanning() const noexcept = 0;
};

}  // namespace lilolify::core
