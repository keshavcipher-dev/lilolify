// ============================================================================
// Lilolify — PipelineCoordinator Implementation
// ============================================================================

#include <lilolify/app/pipeline_coordinator.hpp>

#include <lilolify/infra/crypto/sha256_calculator.hpp>
#include <lilolify/infra/metadata/image_metadata_extractor.hpp>
#include <lilolify/infra/metadata/pdf_metadata_extractor.hpp>
#include <lilolify/infra/metadata/text_metadata_extractor.hpp>
#include <lilolify/core/services/metadata_engine.hpp>

#include <filesystem>
#include <system_error>

#include <lilolify/infra/filesystem/mime_detector.hpp>

namespace lilolify::app {

// ============================================================================
// Constructor / Destructor
// ============================================================================

PipelineCoordinator::PipelineCoordinator(
    core::IFileScanner& scanner,
    infra::SqliteDatabase& database,
    core::IOcrEngine& ocr_engine,
    core::IAiProvider& ai_provider,
    core::IFileOrganizer& organizer) noexcept
    : scanner_(scanner),
      database_(database),
      ocr_engine_(ocr_engine),
      ai_provider_(ai_provider),
      organizer_(organizer),
      reasoning_engine_(ai_provider, ocr_engine) {}

PipelineCoordinator::~PipelineCoordinator() {
    cancel();
    wait();
}

// ============================================================================
// API Control
// ============================================================================

core::Result<void, core::Error> PipelineCoordinator::start(
    const PipelineConfig& config,
    std::function<void(const PipelineProgress&)> callback) {

    // Ensure database is initialized
    if (!database_.handle()) {
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kNotFound, "Database connection is not open."));
    }

    // Ensure only one background thread runs at a time
    if (is_running_) {
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kFileWriteError, "Pipeline is already actively running."));
    }

    is_running_ = true;
    cancellation_requested_ = false;
    progress_callback_ = callback;

    // Reset progress state
    {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        progress_ = PipelineProgress{};
        progress_.state = PipelineState::kIdle;
    }

    // Join previous thread if it has finished execution but is still joinable
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    // Spawn background processing thread
    worker_thread_ = std::thread(&PipelineCoordinator::run_worker, this, config);

    return core::Result<void, core::Error>::success();
}

void PipelineCoordinator::cancel() noexcept {
    if (!is_running_) return;
    cancellation_requested_ = true;
    scanner_.cancel();
}

void PipelineCoordinator::wait() noexcept {
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

PipelineProgress PipelineCoordinator::progress() const noexcept {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    return progress_;
}

// ============================================================================
// Thread-Safe Progress Emitters
// ============================================================================

void PipelineCoordinator::update_progress(
    PipelineState state,
    const std::string& path,
    const std::string& msg) noexcept {

    std::lock_guard<std::mutex> lock(progress_mutex_);
    progress_.state = state;
    if (!path.empty()) {
        progress_.current_file_path = path;
    }
    if (!msg.empty()) {
        progress_.status_message = msg;
    }

    if (progress_callback_) {
        progress_callback_(progress_);
    }
}

// ============================================================================
// Background Worker Loop Thread
// ============================================================================

void PipelineCoordinator::run_worker(const PipelineConfig& config) {
    // 1. Instantiate Core MetadataEngine with all default adapters
    auto hash_calc = std::make_shared<infra::Sha256Calculator>();
    std::vector<std::shared_ptr<core::IMetadataExtractor>> extractors = {
        std::make_shared<infra::ImageMetadataExtractor>(),
        std::make_shared<infra::PdfMetadataExtractor>(),
        std::make_shared<infra::TextMetadataExtractor>()
    };
    core::MetadataEngine metadata_engine(hash_calc, std::move(extractors));

    std::vector<core::FilePath> paths_to_process;

    // STEP 1: SCANNING PHASE
    update_progress(PipelineState::kScanning, "", "Scanning filesystem for target files...");

    std::string root_str = config.scan_root.string();
    if (root_str.find('|') != std::string::npos) {
        // Direct multiselect files list: skip directory scanning
        std::stringstream ss(root_str);
        std::string single_path;
        while (std::getline(ss, single_path, '|')) {
            if (!single_path.empty()) {
                std::filesystem::path fs_path(single_path);
                if (std::filesystem::exists(fs_path)) {
                    std::error_code ec;
                    std::uint64_t file_size = std::filesystem::file_size(fs_path, ec);
                    if (!ec) {
                        auto write_time = std::filesystem::last_write_time(fs_path, ec);
                        auto created = ec ? core::Timestamp{} : std::chrono::clock_cast<std::chrono::system_clock>(write_time);
                        
                        core::FileEntry entry(fs_path, file_size, created, created);
                        
                        infra::MimeDetector detector;
                        entry.set_mime_type(detector.detect_from_file(fs_path));
                        entry.set_status(core::ProcessingStatus::kPending);
                        
                        // Save to database
                        (void)database_.save_file(entry);
                        paths_to_process.push_back(fs_path);
                    }
                }
            }
        }
    } else {
        core::ScanOptions options = config.scan_options;
        options.root_directory = config.scan_root;

        auto scan_res = scanner_.scan(
            options,
            [this](const core::ScanProgress& scan_progress) {
                if (cancellation_requested_) {
                    scanner_.cancel();
                }
                {
                    std::lock_guard<std::mutex> lock(progress_mutex_);
                    progress_.total_files = static_cast<std::uint32_t>(scan_progress.files_found);
                }
                update_progress(PipelineState::kScanning);
            }
        );

        if (cancellation_requested_) {
            update_progress(PipelineState::kCancelled, "", "Scan cancelled by user.");
            is_running_ = false;
            return;
        }

        if (scan_res.has_error()) {
            update_progress(PipelineState::kFailed, "", "Scanning failed: " + std::string(scan_res.error().message()));
            is_running_ = false;
            return;
        }

        auto scan_result = std::move(scan_res).value();
        for (const auto& entry : scan_result.files) {
            (void)database_.save_file(entry);
            paths_to_process.push_back(entry.path());
        }
    }

    {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        progress_.total_files = static_cast<std::uint32_t>(paths_to_process.size());
    }
    update_progress(PipelineState::kScanning, "", "Finished directory scan.");

    // STEP 2: PROCESSING PHASE (Metadata extraction + AI Reasoning)
    update_progress(PipelineState::kProcessing, "", "Analyzing file contents and running AI...");

    struct JobItem {
        core::FileEntry entry;
        core::OrganizationDecision decision;
    };
    std::vector<JobItem> completed_jobs;

    for (const auto& path : paths_to_process) {
        if (cancellation_requested_) {
            update_progress(PipelineState::kCancelled, "", "Processing aborted by user.");
            is_running_ = false;
            return;
        }

        // Retrieve latest FileEntry state from database
        auto find_res = database_.find_file(path);
        if (find_res.has_error() || !find_res.value().has_value()) {
            continue;
        }
        auto entry = find_res.value().value();

        update_progress(PipelineState::kProcessing, entry.path().string(), "Extracting properties...");

        // 2.1 Deep Metadata Extraction
        auto meta_res = metadata_engine.extract_metadata(entry.path(), entry.mime_type(), entry.extension());
        if (meta_res.has_value()) {
            entry.set_metadata(std::move(meta_res.value()));
        }
        entry.set_status(core::ProcessingStatus::kAnalyzing);
        (void)database_.save_file(entry);

        if (cancellation_requested_) break;

        // 2.2 AI Reasoning Classification
        entry.set_status(core::ProcessingStatus::kDeciding);
        auto reason_res = reasoning_engine_.analyze(entry);
        if (reason_res.has_error()) {
            entry.set_status(core::ProcessingStatus::kFailed);
            (void)database_.save_file(entry);
            continue;
        }

        // Cache final analysis state in database
        (void)database_.save_file(entry);

        completed_jobs.push_back(JobItem{std::move(entry), std::move(reason_res.value())});

        {
            std::lock_guard<std::mutex> lock(progress_mutex_);
            progress_.processed_files++;
        }
        update_progress(PipelineState::kProcessing);
    }

    if (cancellation_requested_) {
        update_progress(PipelineState::kCancelled, "", "Processing aborted by user.");
        is_running_ = false;
        return;
    }

    // STEP 3: REORGANIZING PHASE (File movement and journaling)
    update_progress(PipelineState::kReorganizing, "", "Organizing files on disk...");

    for (auto& job : completed_jobs) {
        if (cancellation_requested_) {
            update_progress(PipelineState::kCancelled, "", "Reorganization aborted by user.");
            is_running_ = false;
            return;
        }

        update_progress(PipelineState::kReorganizing, job.entry.path().string(), "Moving file...");

        job.entry.set_status(core::ProcessingStatus::kMoving);
        (void)database_.save_file(job.entry);

        // Execute file transaction
        auto action_res = organizer_.execute(
            job.entry.path(),
            config.destination_base / "Lilolify",
            job.decision.suggested_path,
            config.action_type,
            config.collision_strategy,
            config.transaction_id
        );

        if (action_res.has_value()) {
            // Save action in ledger
            (void)database_.save_action(action_res.value());

            // Update file details in database
            job.entry.set_status(core::ProcessingStatus::kCompleted);
            (void)database_.save_file(job.entry);
        } else {
            job.entry.set_status(core::ProcessingStatus::kFailed);
            (void)database_.save_file(job.entry);
        }

        {
            std::lock_guard<std::mutex> lock(progress_mutex_);
            progress_.organized_files++;
        }
        update_progress(PipelineState::kReorganizing);
    }

    if (cancellation_requested_) {
        update_progress(PipelineState::kCancelled, "", "Reorganization aborted by user.");
    } else {
        update_progress(PipelineState::kCompleted, "", "Job executed successfully!");
    }

    is_running_ = false;
}

}  // namespace lilolify::app
