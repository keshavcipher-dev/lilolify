// ============================================================================
// Lilolify — AppShell Facade Implementation
// ============================================================================

#include <lilolify/app/app_shell.hpp>

#include <lilolify/infra/filesystem/filesystem_scanner.hpp>
#include <lilolify/infra/ocr/tesseract_ocr_engine.hpp>
#include <lilolify/ai/providers/ai_provider_factory.hpp>
#include <lilolify/infra/organization/file_organizer.hpp>
#include <lilolify/infra/network/curl_http_client.hpp>

#include <filesystem>

namespace lilolify::app {

// ============================================================================
// Constructors / Destructors
// ============================================================================

AppShell::AppShell() noexcept
    : is_initialized_(false), is_custom_mocked_(false) {}

AppShell::AppShell(
    std::unique_ptr<core::IFileScanner> scanner,
    std::unique_ptr<core::IOcrEngine> ocr_engine,
    std::unique_ptr<core::IAiProvider> ai_provider,
    std::unique_ptr<core::IFileOrganizer> organizer) noexcept
    : scanner_(std::move(scanner)),
      ocr_engine_(std::move(ocr_engine)),
      ai_provider_(std::move(ai_provider)),
      organizer_(std::move(organizer)),
      is_initialized_(false),
      is_custom_mocked_(true) {}

AppShell::~AppShell() {
    (void)shutdown();
}

// ============================================================================
// Lifecycle Management
// ============================================================================

core::Result<void, core::Error> AppShell::initialize(const std::string& custom_config_dir) {
    if (is_initialized_) {
        return core::Result<void, core::Error>::success();
    }

    // 1. Resolve localized config and settings files paths
    if (!custom_config_dir.empty()) {
        settings_file_path_ = (std::filesystem::path(custom_config_dir) / "settings.json").string();
    } else {
        settings_file_path_ = config_manager_.get_default_settings_path();
    }

    // 2. Load configurations from settings.json (fallback to defaults if missing)
    auto load_res = config_manager_.load(settings_file_path_);
    if (load_res.has_error()) {
        return core::Result<void, core::Error>::failure(std::move(load_res).error());
    }
    settings_ = std::move(load_res).value();

    if (!custom_config_dir.empty() && 
        (settings_.database_path.empty() || settings_.database_path == config_manager_.get_default_database_path())) {
        settings_.database_path = (std::filesystem::path(custom_config_dir) / "lilolify.db").string();
    }

    if (!is_custom_mocked_) {
        // 3. Initialize SQLite database Cache
        database_ = std::make_unique<infra::SqliteDatabase>();
        auto db_open_res = database_->open(settings_.database_path);
        if (db_open_res.has_error()) {
            return core::Result<void, core::Error>::failure(std::move(db_open_res).error());
        }

        auto db_init_res = database_->initialize_schema();
        if (db_init_res.has_error()) {
            return core::Result<void, core::Error>::failure(std::move(db_init_res).error());
        }

        // 4. Initialize production-grade ports
        http_client_ = std::make_unique<infra::CurlHttpClient>();
        scanner_ = std::make_unique<infra::FilesystemScanner>();
        
        auto tess = std::make_unique<infra::TesseractOcrEngine>();
        (void)tess->initialize("", "eng");
        ocr_engine_ = std::move(tess);
        
        organizer_ = std::make_unique<infra::FileOrganizer>();

        // 5. Build selected AI Provider
        auto ai_init_res = initialize_ai_provider();
        if (ai_init_res.has_error()) {
            // Keep provider null, but do not fail initialization completely.
            // This allows the app to load and open the settings screen for API configuration.
        }
    } else {
        // In dependency injection mock modes, we use the database wrapper
        database_ = std::make_unique<infra::SqliteDatabase>();
        auto db_open_res = database_->open(settings_.database_path);
        if (db_open_res.has_error()) {
            return core::Result<void, core::Error>::failure(std::move(db_open_res).error());
        }
        auto db_init_res = database_->initialize_schema();
        if (db_init_res.has_error()) {
            return core::Result<void, core::Error>::failure(std::move(db_init_res).error());
        }
    }

    // 6. Build Pipeline Coordinator Orchestrator
    pipeline_coordinator_ = std::make_unique<PipelineCoordinator>(
        *scanner_,
        *database_,
        *ocr_engine_,
        *ai_provider_,
        *organizer_
    );

    is_initialized_ = true;
    return core::Result<void, core::Error>::success();
}

core::Result<void, core::Error> AppShell::shutdown() noexcept {
    if (!is_initialized_) {
        return core::Result<void, core::Error>::success();
    }

    cancel_job();
    wait_for_job();

    if (database_) {
        (void)database_->close();
    }

    is_initialized_ = false;
    return core::Result<void, core::Error>::success();
}

// ============================================================================
// Job Orchestrator
// ============================================================================

core::Result<void, core::Error> AppShell::start_job(
    const std::string& scan_path,
    const std::string& dest_path,
    core::FileActionType action_type,
    std::function<void(const PipelineProgress&)> callback) {

    if (!is_initialized_) {
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kNotFound, "AppShell is not initialized."));
    }

    // Assemble Config parameters from loaded structures
    PipelineConfig config;
    config.scan_root = scan_path;
    config.destination_base = dest_path;
    config.action_type = action_type;
    config.collision_strategy = settings_.organization.collision_strategy;
    
    // Copy scan filters
    config.scan_options.recursive = settings_.scan.recursive;
    config.scan_options.max_depth = settings_.scan.max_depth;
    config.scan_options.max_file_size = settings_.scan.max_file_size;
    config.scan_options.min_file_size = settings_.scan.min_file_size;
    
    for (const auto& ext : settings_.scan.include_extensions) {
        config.scan_options.include_extensions.push_back(ext);
    }
    for (const auto& ext : settings_.scan.exclude_extensions) {
        config.scan_options.exclude_extensions.push_back(ext);
    }
    for (const auto& dir : settings_.scan.exclude_directories) {
        config.scan_options.exclude_directories.push_back(dir);
    }
    
    config.scan_options.follow_symlinks = settings_.scan.follow_symlinks;
    config.scan_options.detect_mime_type = settings_.scan.detect_mime_type;

    // Generate unique transaction group ID
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    config.transaction_id = "tx-" + std::to_string(ms);

    return pipeline_coordinator_->start(config, callback);
}

void AppShell::cancel_job() noexcept {
    if (pipeline_coordinator_) {
        pipeline_coordinator_->cancel();
    }
}

void AppShell::wait_for_job() noexcept {
    if (pipeline_coordinator_) {
        pipeline_coordinator_->wait();
    }
}

core::Result<void, core::Error> AppShell::undo_transaction(const std::string& transaction_id) {
    if (!is_initialized_) {
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kNotFound, "AppShell is not initialized."));
    }

    // Retrieve ledger history group logs
    auto history_res = database_->get_transaction_history(transaction_id);
    if (history_res.has_error()) {
        return core::Result<void, core::Error>::failure(std::move(history_res).error());
    }

    const auto& actions = history_res.value();
    if (actions.empty()) {
        return core::Result<void, core::Error>::success(); // Nothing to revert
    }

    // Rollback changes safely inside SQL transaction
    auto trans_res = database_->begin_transaction();
    if (trans_res.has_error()) return trans_res;

    // Revert files in reverse order (LIFO)
    for (auto it = actions.rbegin(); it != actions.rend(); ++it) {
        auto revert_res = organizer_->revert(*it);
        if (revert_res.has_error()) {
            (void)database_->rollback_transaction();
            return revert_res;
        }
    }

    auto commit_res = database_->commit_transaction();
    if (commit_res.has_error()) return commit_res;

    return core::Result<void, core::Error>::success();
}

// ============================================================================
// Settings Management
// ============================================================================

core::Result<void, core::Error> AppShell::reload_settings() {
    auto load_res = config_manager_.load(settings_file_path_);
    if (load_res.has_error()) {
        return core::Result<void, core::Error>::failure(std::move(load_res).error());
    }

    settings_ = std::move(load_res).value();
    
    // Hot-reload AI provider bindings
    (void)initialize_ai_provider();
    return core::Result<void, core::Error>::success();
}

core::Result<void, core::Error> AppShell::update_settings(const AppSettings& settings) {
    auto save_res = config_manager_.save(settings_file_path_, settings);
    if (save_res.has_error()) {
        return save_res;
    }

    settings_ = settings;
    
    // Hot-reload AI provider configurations
    (void)initialize_ai_provider();
    return core::Result<void, core::Error>::success();
}

// ============================================================================
// Accessors / Progress Checks
// ============================================================================

PipelineProgress AppShell::progress() const noexcept {
    if (pipeline_coordinator_) {
        return pipeline_coordinator_->progress();
    }
    return PipelineProgress{};
}

bool AppShell::is_job_running() const noexcept {
    if (pipeline_coordinator_) {
        return pipeline_coordinator_->is_running();
    }
    return false;
}

core::Result<void, core::Error> AppShell::initialize_ai_provider() {
    if (is_custom_mocked_ || settings_.ai.active_provider.empty() || !http_client_) {
        return core::Result<void, core::Error>::success();
    }

    std::string decrypted_key = config_manager_.deobfuscate_key(settings_.ai.api_key);

    auto provider_res = ai::AiProviderFactory::create(
        settings_.ai.active_provider,
        decrypted_key,
        *http_client_,
        settings_.ai.model,
        settings_.ai.base_url
    );

    if (provider_res.has_error()) {
        return core::Result<void, core::Error>::failure(std::move(provider_res).error());
    }

    ai_provider_ = std::move(provider_res).value();
    
    // Re-instantiate coordinator with new provider bindings
    if (is_initialized_) {
        pipeline_coordinator_ = std::make_unique<PipelineCoordinator>(
            *scanner_,
            *database_,
            *ocr_engine_,
            *ai_provider_,
            *organizer_
        );
    }

    return core::Result<void, core::Error>::success();
}

}  // namespace lilolify::app
