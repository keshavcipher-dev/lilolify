// ============================================================================
// Lilolify — FilesystemScanner Implementation
// ============================================================================

#include <lilolify/infra/filesystem/filesystem_scanner.hpp>

#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

namespace lilolify::infra {

// ============================================================================
// Construction
// ============================================================================

FilesystemScanner::FilesystemScanner() = default;

// ============================================================================
// IFileScanner: scan()
// ============================================================================

core::Result<core::ScanResult, core::Error> FilesystemScanner::scan(
    const core::ScanOptions& options,
    core::ScanProgressCallback on_progress) {

    // --- Validate options ---
    auto validation = options.validate();
    if (validation.has_error()) {
        return core::Result<core::ScanResult, core::Error>::failure(
            std::move(validation).error());
    }

    // --- Check root directory exists ---
    std::error_code ec;
    if (!fs::exists(options.root_directory, ec) || ec) {
        return core::Result<core::ScanResult, core::Error>::failure(
            core::Error(core::ErrorCode::kDirectoryNotFound,
                        "Path does not exist: " +
                            options.root_directory.string()));
    }

    // --- Single File Support ---
    if (fs::is_regular_file(options.root_directory, ec) && !ec) {
        core::ScanResult result;
        core::ScanProgress progress;
        auto start_time = std::chrono::steady_clock::now();

        auto file_size = fs::file_size(options.root_directory, ec);
        if (!ec) {
            std::string ext = options.root_directory.extension().string();
            for (auto& ch : ext) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }

            auto write_time = fs::last_write_time(options.root_directory, ec);
            auto created = ec ? core::Timestamp{} : std::chrono::clock_cast<std::chrono::system_clock>(write_time);

            core::FileEntry file_entry(options.root_directory, file_size, created, created);
            
            if (options.detect_mime_type) {
                auto mime = mime_detector_.detect_from_file(options.root_directory);
                file_entry.set_mime_type(std::move(mime));
            } else {
                auto mime = mime_detector_.detect_from_extension(ext);
                file_entry.set_mime_type(std::move(mime));
            }

            file_entry.set_status(core::ProcessingStatus::kPending);
            result.files.push_back(std::move(file_entry));
            
            progress.files_found = 1;
            progress.files_processed = 1;
        }

        auto end_time = std::chrono::steady_clock::now();
        progress.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        result.final_progress = progress;

        return core::Result<core::ScanResult, core::Error>::success(std::move(result));
    }

    if (!fs::is_directory(options.root_directory, ec) || ec) {
        return core::Result<core::ScanResult, core::Error>::failure(
            core::Error(core::ErrorCode::kInvalidPath,
                        "Path is not a directory or a file: " +
                            options.root_directory.string()));
    }

    // --- Initialize scan state ---
    cancel_requested_.store(false, std::memory_order_relaxed);
    scanning_.store(true, std::memory_order_relaxed);

    core::ScanResult result;
    core::ScanProgress progress;

    auto start_time = std::chrono::steady_clock::now();

    // --- Configure directory iteration options ---
    auto dir_options = fs::directory_options::skip_permission_denied;
    if (options.follow_symlinks) {
        dir_options |= fs::directory_options::follow_directory_symlink;
    }

    // --- Choose iteration strategy based on recursive flag ---
    if (options.recursive) {
        auto iter = fs::recursive_directory_iterator(
            options.root_directory, dir_options, ec);

        if (ec) {
            scanning_.store(false, std::memory_order_relaxed);
            return core::Result<core::ScanResult, core::Error>::failure(
                core::Error(core::ErrorCode::kFileAccessDenied,
                            "Cannot open directory: " + ec.message()));
        }

        auto end = fs::end(iter);

        while (iter != end) {
            // --- Check cancellation ---
            if (cancel_requested_.load(std::memory_order_relaxed)) {
                result.was_cancelled = true;
                break;
            }

            const auto& entry = *iter;

            // --- Check depth limit ---
            if (static_cast<std::uint32_t>(iter.depth()) > options.max_depth) {
                iter.disable_recursion_pending();
                iter.increment(ec);
                continue;
            }

            // --- Handle directories ---
            if (entry.is_directory(ec) && !ec) {
                progress.directories_scanned++;
                progress.current_directory = entry.path().string();

                // Check if directory should be excluded
                if (is_directory_excluded(entry.path(), options)) {
                    iter.disable_recursion_pending();
                    iter.increment(ec);
                    continue;
                }

                iter.increment(ec);
                if (ec) {
                    result.errors.push_back(
                        "Error iterating in: " + entry.path().string() +
                        " (" + ec.message() + ")");
                    progress.errors_count++;
                    ec.clear();
                }
                continue;
            }

            // --- Handle files ---
            if (entry.is_regular_file(ec) && !ec) {
                auto file_size = entry.file_size(ec);
                if (ec) {
                    result.errors.push_back(
                        "Cannot read file size: " + entry.path().string() +
                        " (" + ec.message() + ")");
                    progress.errors_count++;
                    ec.clear();
                    iter.increment(ec);
                    continue;
                }

                // --- Apply size filter ---
                if (!options.is_size_allowed(file_size)) {
                    progress.files_skipped++;
                    iter.increment(ec);
                    continue;
                }

                // --- Apply extension filter ---
                std::string ext = entry.path().extension().string();
                for (auto& ch : ext) {
                    ch = static_cast<char>(
                        std::tolower(static_cast<unsigned char>(ch)));
                }

                if (!options.is_extension_allowed(ext)) {
                    progress.files_skipped++;
                    iter.increment(ec);
                    continue;
                }

                // --- Create FileEntry ---
                auto created = get_creation_time(entry);
                auto modified = get_last_write_time(entry);

                core::FileEntry file_entry(
                    entry.path(), file_size, created, modified);

                // --- Detect MIME type ---
                if (options.detect_mime_type) {
                    auto mime = mime_detector_.detect_from_file(entry.path());
                    file_entry.set_mime_type(std::move(mime));
                } else {
                    auto mime = mime_detector_.detect_from_extension(ext);
                    file_entry.set_mime_type(std::move(mime));
                }

                file_entry.set_status(core::ProcessingStatus::kPending);
                result.files.push_back(std::move(file_entry));

                progress.files_found++;
                progress.files_processed++;

                // --- Report progress ---
                if (on_progress &&
                    progress.files_processed % options.progress_interval == 0) {
                    auto now = std::chrono::steady_clock::now();
                    progress.elapsed =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - start_time);
                    on_progress(progress);
                }
            }

            // --- Advance iterator ---
            iter.increment(ec);
            if (ec) {
                result.errors.push_back(
                    "Error advancing past: " + entry.path().string() +
                    " (" + ec.message() + ")");
                progress.errors_count++;
                ec.clear();
            }
        }
    } else {
        // --- Non-recursive: only scan the root directory ---
        auto iter = fs::directory_iterator(
            options.root_directory, dir_options, ec);

        if (ec) {
            scanning_.store(false, std::memory_order_relaxed);
            return core::Result<core::ScanResult, core::Error>::failure(
                core::Error(core::ErrorCode::kFileAccessDenied,
                            "Cannot open directory: " + ec.message()));
        }

        auto end = fs::end(iter);
        progress.directories_scanned = 1;
        progress.current_directory = options.root_directory.string();

        while (iter != end) {
            if (cancel_requested_.load(std::memory_order_relaxed)) {
                result.was_cancelled = true;
                break;
            }

            const auto& entry = *iter;

            if (entry.is_regular_file(ec) && !ec) {
                auto file_size = entry.file_size(ec);
                if (ec) {
                    result.errors.push_back(
                        "Cannot read file size: " + entry.path().string() +
                        " (" + ec.message() + ")");
                    progress.errors_count++;
                    ec.clear();
                    iter.increment(ec);
                    continue;
                }

                if (!options.is_size_allowed(file_size)) {
                    progress.files_skipped++;
                    iter.increment(ec);
                    continue;
                }

                std::string ext = entry.path().extension().string();
                for (auto& ch : ext) {
                    ch = static_cast<char>(
                        std::tolower(static_cast<unsigned char>(ch)));
                }

                if (!options.is_extension_allowed(ext)) {
                    progress.files_skipped++;
                    iter.increment(ec);
                    continue;
                }

                auto created = get_creation_time(entry);
                auto modified = get_last_write_time(entry);

                core::FileEntry file_entry(
                    entry.path(), file_size, created, modified);

                if (options.detect_mime_type) {
                    auto mime = mime_detector_.detect_from_file(entry.path());
                    file_entry.set_mime_type(std::move(mime));
                } else {
                    auto mime = mime_detector_.detect_from_extension(ext);
                    file_entry.set_mime_type(std::move(mime));
                }

                file_entry.set_status(core::ProcessingStatus::kPending);
                result.files.push_back(std::move(file_entry));

                progress.files_found++;
                progress.files_processed++;

                if (on_progress &&
                    progress.files_processed % options.progress_interval == 0) {
                    auto now = std::chrono::steady_clock::now();
                    progress.elapsed =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - start_time);
                    on_progress(progress);
                }
            }

            iter.increment(ec);
            if (ec) {
                result.errors.push_back(
                    "Error advancing past: " + entry.path().string() +
                    " (" + ec.message() + ")");
                progress.errors_count++;
                ec.clear();
            }
        }
    }

    // --- Finalize ---
    auto end_time = std::chrono::steady_clock::now();
    progress.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    result.final_progress = progress;

    scanning_.store(false, std::memory_order_relaxed);

    return core::Result<core::ScanResult, core::Error>::success(std::move(result));
}

// ============================================================================
// IFileScanner: cancel() / is_scanning()
// ============================================================================

void FilesystemScanner::cancel() {
    cancel_requested_.store(true, std::memory_order_relaxed);
}

bool FilesystemScanner::is_scanning() const noexcept {
    return scanning_.load(std::memory_order_relaxed);
}

// ============================================================================
// Private: Directory Exclusion Check
// ============================================================================

bool FilesystemScanner::is_directory_excluded(
    const core::FilePath& dir,
    const core::ScanOptions& options) const {

    for (const auto& excluded : options.exclude_directories) {
        std::error_code ec;
        if (fs::equivalent(dir, excluded, ec) && !ec) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Private: Timestamp Extraction
// ============================================================================

core::Timestamp FilesystemScanner::get_last_write_time(
    const fs::directory_entry& entry) noexcept {

    std::error_code ec;
    auto ftime = entry.last_write_time(ec);
    if (ec) {
        return core::Timestamp{};  // epoch on failure
    }

    // Convert file_time to system_clock::time_point
    // C++20: use clock_cast if available, otherwise manual conversion
    auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
    return sctp;
}

core::Timestamp FilesystemScanner::get_creation_time(
    const fs::directory_entry& entry) noexcept {

    // std::filesystem doesn't provide a cross-platform creation time API.
    // On Windows, last_write_time is the closest portable alternative.
    // Future: use platform-specific APIs (GetFileTime on Windows).
    return get_last_write_time(entry);
}

}  // namespace lilolify::infra
