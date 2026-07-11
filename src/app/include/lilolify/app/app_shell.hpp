// ============================================================================
// Lilolify — AppShell Facade
// ============================================================================
// Central lifecycle manager assembling settings loaders, WAL database caches,
// HTTP networks, OCR scrapers, LLMs, and worker threads into a unified API.
// ============================================================================

#pragma once

#include <lilolify/app/app_settings.hpp>
#include <lilolify/app/pipeline_coordinator.hpp>
#include <lilolify/app/configuration_manager.hpp>
#include <lilolify/core/error.hpp>
#include <lilolify/core/result.hpp>

#include <memory>
#include <string>
#include <functional>

namespace lilolify::core {
class IHttpClient;
}

namespace lilolify::app {

/// Unified orchestrator shell presenting application interfaces to UI controllers.
class AppShell {
public:
    /// Default constructor: uses production-quality database and filesystem adapters.
    AppShell() noexcept;

    /// Dependency Injection constructor for testing mocks.
    AppShell(
        std::unique_ptr<core::IFileScanner> scanner,
        std::unique_ptr<core::IOcrEngine> ocr_engine,
        std::unique_ptr<core::IAiProvider> ai_provider,
        std::unique_ptr<core::IFileOrganizer> organizer) noexcept;

    ~AppShell();

    // Disable copy/move
    AppShell(const AppShell&) = delete;
    AppShell& operator=(const AppShell&) = delete;
    AppShell(AppShell&&) = delete;
    AppShell& operator=(AppShell&&) = delete;

    /// Load default configuration, create folders, open database connection.
    [[nodiscard]] core::Result<void, core::Error> initialize(const std::string& custom_config_dir = "");

    /// Terminate background pipeline jobs and close SQLite connection.
    [[nodiscard]] core::Result<void, core::Error> shutdown() noexcept;

    /// Launch background organization thread.
    ///
    /// @param scan_path    Directory to discover files in.
    /// @param dest_path    Base directory to reorganize categories into.
    /// @param action_type  Copy, Move, or Link directive.
    /// @param callback     State machine progress reporting callback.
    /// @return Success, or Error if configuration is missing or a job is active.
    [[nodiscard]] core::Result<void, core::Error> start_job(
        const std::string& scan_path,
        const std::string& dest_path,
        core::FileActionType action_type,
        std::function<void(const PipelineProgress&)> callback);

    /// Abort ongoing background processing.
    void cancel_job() noexcept;

    /// Blocks calling thread until active background thread completes.
    void wait_for_job() noexcept;

    /// Revert all filesystem changes group logged in a transaction session.
    [[nodiscard]] core::Result<void, core::Error> undo_transaction(
        const std::string& transaction_id);

    /// Reload settings dynamically from settings.json.
    [[nodiscard]] core::Result<void, core::Error> reload_settings();

    /// Retrieve active AppSettings.
    [[nodiscard]] AppSettings settings() const noexcept { return settings_; }

    /// Overwrite active AppSettings and save them to disk.
    [[nodiscard]] core::Result<void, core::Error> update_settings(const AppSettings& settings);

    /// Retrieve current job progress.
    [[nodiscard]] PipelineProgress progress() const noexcept;

    /// Check if a background processing thread is running.
    [[nodiscard]] bool is_job_running() const noexcept;

    /// Check if initialization sequence succeeded.
    [[nodiscard]] bool is_initialized() const noexcept { return is_initialized_; }

private:
    [[nodiscard]] core::Result<void, core::Error> initialize_ai_provider();

    // Configuration
    ConfigurationManager config_manager_;
    AppSettings settings_;
    std::string settings_file_path_;

    // Core Injected Components (Managed by AppShell or Test)
    std::unique_ptr<core::IHttpClient> http_client_;
    std::unique_ptr<core::IFileScanner> scanner_;
    std::unique_ptr<infra::SqliteDatabase> database_;
    std::unique_ptr<core::IOcrEngine> ocr_engine_;
    std::unique_ptr<core::IAiProvider> ai_provider_;
    std::unique_ptr<core::IFileOrganizer> organizer_;

    // Pipeline Coordinator
    std::unique_ptr<PipelineCoordinator> pipeline_coordinator_;

    bool is_initialized_{false};
    bool is_custom_mocked_{false};
};

}  // namespace lilolify::app
