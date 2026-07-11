// ============================================================================
// Lilolify — FilesystemScanner
// ============================================================================
// Concrete implementation of IFileScanner using std::filesystem.
// Handles recursive directory traversal, filtering, MIME detection,
// progress reporting, and cancellation.
// ============================================================================

#pragma once

#include <lilolify/core/interfaces/i_file_scanner.hpp>
#include <lilolify/infra/filesystem/mime_detector.hpp>

#include <atomic>

namespace lilolify::infra {

/// Production implementation of IFileScanner using std::filesystem.
///
/// Performance characteristics:
///   - Uses directory_entry cached metadata (avoids redundant stat() calls)
///   - Error-code overloads for all filesystem operations (no exceptions in hot loop)
///   - Extension filtering via direct string comparison (O(n) but n is tiny)
///   - Progress callback throttled to every N files (configurable)
///   - Cancellation checked at each file boundary (single atomic load)
///
/// Thread Safety:
///   - scan() is NOT thread-safe — do not call from multiple threads
///   - cancel() IS thread-safe — may be called from any thread
///   - is_scanning() IS thread-safe — atomic read
class FilesystemScanner : public core::IFileScanner {
public:
    FilesystemScanner();
    ~FilesystemScanner() override = default;

    // -- IFileScanner interface --

    [[nodiscard]] core::Result<core::ScanResult, core::Error> scan(
        const core::ScanOptions& options,
        core::ScanProgressCallback on_progress = nullptr) override;

    void cancel() override;

    [[nodiscard]] bool is_scanning() const noexcept override;

private:
    /// Check if a directory should be excluded from scanning.
    [[nodiscard]] bool is_directory_excluded(
        const core::FilePath& dir,
        const core::ScanOptions& options) const;

    /// Extract file timestamps safely (returns epoch on failure).
    [[nodiscard]] static core::Timestamp get_last_write_time(
        const std::filesystem::directory_entry& entry) noexcept;

    [[nodiscard]] static core::Timestamp get_creation_time(
        const std::filesystem::directory_entry& entry) noexcept;

    std::atomic<bool> cancel_requested_{false};
    std::atomic<bool> scanning_{false};
    MimeDetector mime_detector_;
};

}  // namespace lilolify::infra
