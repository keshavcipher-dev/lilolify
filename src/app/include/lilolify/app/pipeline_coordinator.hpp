// ============================================================================
// Lilolify — PipelineCoordinator
// ============================================================================
// Asynchronous background coordinator executing scanning, metadata, OCR, AI,
// caching, and organization actions in thread-safe worker loops.
// ============================================================================

#pragma once

#include <lilolify/core/interfaces/i_file_scanner.hpp>
#include <lilolify/core/interfaces/i_ocr_engine.hpp>
#include <lilolify/core/interfaces/i_ai_provider.hpp>
#include <lilolify/core/interfaces/i_file_organizer.hpp>
#include <lilolify/core/services/ai_reasoning_engine.hpp>
#include <lilolify/core/value_objects/scan_options.hpp>
#include <lilolify/infra/database/sqlite_database.hpp>

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace lilolify::app {

/// Execution states of the background processing thread.
enum class PipelineState {
    kIdle,          /// Pipeline is not running.
    kScanning,      /// FilesystemScanner is discovering files.
    kProcessing,    /// MIME detection, metadata extraction, OCR, and AI reasoning are running.
    kReorganizing,  /// FileOrganizer is moving/copying files to their destinations.
    kCompleted,     /// Pipeline finished successfully.
    kCancelled,     /// Pipeline was aborted by user request.
    kFailed         /// Pipeline failed due to database or core engine errors.
};

/// Thread-safe progress metrics emitted to the client UI/controller.
struct PipelineProgress {
    PipelineState state = PipelineState::kIdle;
    std::uint32_t total_files = 0;
    std::uint32_t processed_files = 0;
    std::uint32_t organized_files = 0;
    std::string current_file_path;
    std::string status_message;
};

/// Configuration parameters for launching an organization job.
struct PipelineConfig {
    core::FilePath scan_root;
    core::FilePath destination_base;
    core::ScanOptions scan_options;
    core::FileActionType action_type = core::FileActionType::kMove;
    core::CollisionStrategy collision_strategy = core::CollisionStrategy::kRename;
    std::string transaction_id;
};

/// Master pipeline coordinator running end-to-end processing.
class PipelineCoordinator {
public:
    /// Construct coordinator by injecting all ports and services.
    PipelineCoordinator(
        core::IFileScanner& scanner,
        infra::SqliteDatabase& database,
        core::IOcrEngine& ocr_engine,
        core::IAiProvider& ai_provider,
        core::IFileOrganizer& organizer) noexcept;

    ~PipelineCoordinator();

    // Disable copy/move
    PipelineCoordinator(const PipelineCoordinator&) = delete;
    PipelineCoordinator& operator=(const PipelineCoordinator&) = delete;
    PipelineCoordinator(PipelineCoordinator&&) = delete;
    PipelineCoordinator& operator=(PipelineCoordinator&&) = delete;

    /// Launch background processing loop asynchronously.
    ///
    /// @param config    Organization directives and directories.
    /// @param callback  Callback to report thread-safe progress updates.
    /// @return Success, or Error if the database fails to initialize or a job is running.
    [[nodiscard]] core::Result<void, core::Error> start(
        const PipelineConfig& config,
        std::function<void(const PipelineProgress&)> callback);

    /// Request active pipeline thread to abort processing.
    /// This returns immediately; check progress callback state to confirm cancellation.
    void cancel() noexcept;

    /// Wait blocks the calling thread until the background thread joins.
    void wait() noexcept;

    /// Check if background execution thread is actively running.
    [[nodiscard]] bool is_running() const noexcept { return is_running_; }

    /// Read the current thread-safe pipeline progress state.
    [[nodiscard]] PipelineProgress progress() const noexcept;

private:
    void run_worker(const PipelineConfig& config);
    void update_progress(PipelineState state, const std::string& path = "", const std::string& msg = "") noexcept;

    // Injected dependencies
    core::IFileScanner& scanner_;
    infra::SqliteDatabase& database_;
    core::IOcrEngine& ocr_engine_;
    core::IAiProvider& ai_provider_;
    core::IFileOrganizer& organizer_;
    core::AiReasoningEngine reasoning_engine_;

    // Threading & Synchronization
    std::thread worker_thread_;
    std::function<void(const PipelineProgress&)> progress_callback_;
    mutable std::mutex progress_mutex_;
    PipelineProgress progress_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> cancellation_requested_{false};
};

}  // namespace lilolify::app
