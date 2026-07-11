// ============================================================================
// Lilolify — Unit Tests: PipelineCoordinator
// ============================================================================

#include <lilolify/app/pipeline_coordinator.hpp>
#include <lilolify/infra/database/sqlite_database.hpp>

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace lilolify::app::test {

// ============================================================================
// Mock Implementations for Isolated Pipeline Testing
// ============================================================================

class MockFileScanner : public core::IFileScanner {
public:
    core::Result<core::ScanResult, core::Error> scan(
        const core::ScanOptions& options,
        core::ScanProgressCallback on_progress = nullptr) override {
        
        if (cancelled_) {
            core::ScanResult res;
            res.was_cancelled = true;
            return core::Result<core::ScanResult, core::Error>::success(std::move(res));
        }

        core::ScanResult res;
        auto now = std::chrono::system_clock::now();
        
        // Use extension and MIME type that skip format-specific extractors
        core::FileEntry file1(options.root_directory / "document.bin", 1024, now, now);
        file1.set_mime_type("application/octet-stream");
        res.files.push_back(file1);

        if (on_progress) {
            core::ScanProgress prog;
            prog.files_found = 1;
            on_progress(prog);
        }

        if (delay_ms_ > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms_));
        }

        if (cancelled_) {
            res.was_cancelled = true;
            return core::Result<core::ScanResult, core::Error>::success(std::move(res));
        }

        core::FileEntry file2(options.root_directory / "image.bin", 2048, now, now);
        file2.set_mime_type("application/octet-stream");
        res.files.push_back(file2);

        if (on_progress) {
            core::ScanProgress prog;
            prog.files_found = 2;
            on_progress(prog);
        }

        return core::Result<core::ScanResult, core::Error>::success(std::move(res));
    }

    void cancel() noexcept override {
        cancelled_ = true;
    }

    bool is_scanning() const noexcept override {
        return false;
    }

    void set_delay(int ms) noexcept { delay_ms_ = ms; }

private:
    std::atomic<bool> cancelled_{false};
    int delay_ms_ = 0;
};

class MockAiProvider : public core::IAiProvider {
public:
    core::Result<core::AiResponse, core::Error> generate(const core::AiRequest&) override {
        core::AiResponse resp;
        resp.provider = "MockAi";
        resp.model = "v1.0";
        resp.text = R"({
            "category": "Invoices",
            "suggested_path": "Invoices/2026/invoice.pdf",
            "tags": ["invoice", "mock"],
            "confidence": 0.95,
            "reasoning": "Identified standard mock file structures."
        })";
        return core::Result<core::AiResponse, core::Error>::success(std::move(resp));
    }

    std::string provider_name() const noexcept override {
        return "MockAi";
    }
};

class MockOcrEngine : public core::IOcrEngine {
public:
    core::Result<void, core::Error> initialize(const std::string&, const std::string&) override {
        return core::Result<void, core::Error>::success();
    }
    core::Result<std::string, core::Error> extract_text(const core::FilePath&) override {
        return core::Result<std::string, core::Error>::success("Extracted mock invoice receipt text.");
    }
    std::string language() const noexcept override {
        return "eng";
    }
    bool is_initialized() const noexcept override {
        return true;
    }
};

class MockFileOrganizer : public core::IFileOrganizer {
public:
    core::Result<core::FileActionRecord, core::Error> execute(
        const core::FilePath& source,
        const core::FilePath& dest_base,
        const std::string& suggested_path,
        core::FileActionType action_type,
        core::CollisionStrategy,
        const std::string& transaction_id) override {

        core::FileActionRecord rec;
        rec.transaction_id = transaction_id;
        rec.original_path = source;
        rec.executed_path = dest_base / suggested_path;
        rec.action_type = action_type;
        rec.timestamp = std::chrono::system_clock::now();
        
        return core::Result<core::FileActionRecord, core::Error>::success(std::move(rec));
    }

    core::Result<void, core::Error> revert(const core::FileActionRecord&) override {
        return core::Result<void, core::Error>::success();
    }
};

// ============================================================================
// Test Fixture Setup
// ============================================================================

class PipelineCoordinatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto current = std::filesystem::current_path();
        
        db_path_ = current / "test_pipeline_coordinator.db";
        std::filesystem::remove(db_path_);
        
        scan_root_ = current / "test_pipeline_scan_root";
        dest_root_ = current / "test_pipeline_dest_root";
        
        std::filesystem::remove_all(scan_root_);
        std::filesystem::remove_all(dest_root_);
        std::filesystem::create_directories(scan_root_);
        std::filesystem::create_directories(dest_root_);

        // Write dummy files (no format-specific extractors will trigger)
        {
            std::ofstream f1(scan_root_ / "document.bin", std::ios::binary);
            f1 << "dummy file content 1";
        }
        {
            std::ofstream f2(scan_root_ / "image.bin", std::ios::binary);
            f2 << "dummy file content 2";
        }
        
        ASSERT_TRUE(database_.open(db_path_.string()).has_value());
        ASSERT_TRUE(database_.initialize_schema().has_value());
    }

    void TearDown() override {
        (void)database_.close();
        std::filesystem::remove(db_path_);
        std::filesystem::remove_all(scan_root_);
        std::filesystem::remove_all(dest_root_);
    }

    std::filesystem::path db_path_;
    std::filesystem::path scan_root_;
    std::filesystem::path dest_root_;
    infra::SqliteDatabase database_;
    
    MockFileScanner scanner_;
    MockOcrEngine ocr_engine_;
    MockAiProvider ai_provider_;
    MockFileOrganizer organizer_;
};

// ============================================================================
// Test Cases
// ============================================================================

TEST_F(PipelineCoordinatorTest, EndToEndPipelineSuccessFlow) {
    PipelineCoordinator coordinator(scanner_, database_, ocr_engine_, ai_provider_, organizer_);

    PipelineConfig config;
    config.scan_root = scan_root_;
    config.destination_base = dest_root_;
    config.transaction_id = "tx-12345";

    std::mutex cv_m;
    std::condition_variable cv;
    bool completed = false;
    std::vector<PipelineProgress> progress_history;

    auto callback = [&](const PipelineProgress& progress) {
        {
            std::lock_guard<std::mutex> lock(cv_m);
            progress_history.push_back(progress);
            if (progress.state == PipelineState::kCompleted || progress.state == PipelineState::kFailed) {
                completed = true;
                cv.notify_one();
            }
        }
    };

    // Start pipeline
    auto start_res = coordinator.start(config, callback);
    ASSERT_TRUE(start_res.has_value());
    EXPECT_TRUE(coordinator.is_running());

    // Wait for completion
    std::unique_lock<std::mutex> lk(cv_m);
    cv.wait_for(lk, std::chrono::seconds(5), [&] { return completed; });

    EXPECT_FALSE(coordinator.is_running());
    coordinator.wait();

    // Verify progress steps
    ASSERT_FALSE(progress_history.empty());
    EXPECT_EQ(progress_history.back().state, PipelineState::kCompleted);
    EXPECT_EQ(progress_history.back().total_files, 2u);
    EXPECT_EQ(progress_history.back().processed_files, 2u);
    EXPECT_EQ(progress_history.back().organized_files, 2u);

    // Verify data cache in database
    auto db_files = database_.find_all_files();
    ASSERT_TRUE(db_files.has_value());
    EXPECT_EQ(db_files.value().size(), 2u);
    for (const auto& file : db_files.value()) {
        EXPECT_EQ(file.status(), core::ProcessingStatus::kCompleted);
        ASSERT_TRUE(file.metadata().has_value());
        EXPECT_FALSE(file.metadata().value().sha256.empty());
    }

    // Verify ledger action database entry
    auto history = database_.get_transaction_history("tx-12345");
    ASSERT_TRUE(history.has_value());
    EXPECT_EQ(history.value().size(), 2u);
}

TEST_F(PipelineCoordinatorTest, StartFailsIfAlreadyRunning) {
    PipelineCoordinator coordinator(scanner_, database_, ocr_engine_, ai_provider_, organizer_);
    scanner_.set_delay(500); // Add delay to keep the scanning phase running

    PipelineConfig config;
    config.scan_root = scan_root_;
    config.destination_base = dest_root_;

    auto start_res1 = coordinator.start(config, [](const PipelineProgress&) {});
    ASSERT_TRUE(start_res1.has_value());

    // Second start attempt must fail immediately
    auto start_res2 = coordinator.start(config, [](const PipelineProgress&) {});
    EXPECT_TRUE(start_res2.has_error());

    // Wait and clean up
    coordinator.cancel();
    coordinator.wait();
}

TEST_F(PipelineCoordinatorTest, CancellationAbortsBackgroundThread) {
    PipelineCoordinator coordinator(scanner_, database_, ocr_engine_, ai_provider_, organizer_);
    scanner_.set_delay(500); // Add scan latency to allow test trigger cancellation

    PipelineConfig config;
    config.scan_root = scan_root_;
    config.destination_base = dest_root_;

    std::mutex cv_m;
    std::condition_variable cv;
    bool finished = false;
    PipelineState final_state = PipelineState::kIdle;

    auto callback = [&](const PipelineProgress& progress) {
        std::lock_guard<std::mutex> lock(cv_m);
        if (progress.state == PipelineState::kCancelled || progress.state == PipelineState::kCompleted || progress.state == PipelineState::kFailed) {
            finished = true;
            final_state = progress.state;
            cv.notify_one();
        }
    };

    ASSERT_TRUE(coordinator.start(config, callback).has_value());
    
    // Trigger cancellation mid-run
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    coordinator.cancel();

    std::unique_lock<std::mutex> lk(cv_m);
    cv.wait_for(lk, std::chrono::seconds(2), [&] { return finished; });

    EXPECT_EQ(final_state, PipelineState::kCancelled);
    coordinator.wait();
}

}  // namespace lilolify::app::test
