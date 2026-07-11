// ============================================================================
// Lilolify — Unit Tests: AppShell Facade
// ============================================================================

#include <lilolify/app/app_shell.hpp>
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
// Mocks for AppShell Facade Test
// ============================================================================

class MockScanner : public core::IFileScanner {
public:
    core::Result<core::ScanResult, core::Error> scan(
        const core::ScanOptions& options,
        core::ScanProgressCallback) override {
        
        core::ScanResult res;
        auto now = std::chrono::system_clock::now();
        
        core::FileEntry file(options.root_directory / "receipt.bin", 512, now, now);
        file.set_mime_type("application/octet-stream");
        res.files.push_back(file);
        
        return core::Result<core::ScanResult, core::Error>::success(std::move(res));
    }

    void cancel() noexcept override {}
    bool is_scanning() const noexcept override { return false; }
};

class MockOcr : public core::IOcrEngine {
public:
    core::Result<void, core::Error> initialize(const std::string&, const std::string&) override {
        return core::Result<void, core::Error>::success();
    }
    core::Result<std::string, core::Error> extract_text(const core::FilePath&) override {
        return core::Result<std::string, core::Error>::success("Mock OCR");
    }
    std::string language() const noexcept override { return "eng"; }
    bool is_initialized() const noexcept override { return true; }
};

class MockProvider : public core::IAiProvider {
public:
    core::Result<core::AiResponse, core::Error> generate(const core::AiRequest&) override {
        core::AiResponse resp;
        resp.provider = "MockProvider";
        resp.model = "v1.0";
        resp.text = R"({
            "category": "Receipts",
            "suggested_path": "Receipts/2026/receipt.pdf",
            "tags": ["receipt"],
            "confidence": 0.99,
            "reasoning": "Standard mockup."
        })";
        return core::Result<core::AiResponse, core::Error>::success(std::move(resp));
    }
    std::string provider_name() const noexcept override { return "MockProvider"; }
};

class MockOrganizer : public core::IFileOrganizer {
public:
    core::Result<core::FileActionRecord, core::Error> execute(
        const core::FilePath& source,
        const core::FilePath& dest_base,
        const std::string& suggested_path,
        core::FileActionType action_type,
        core::CollisionStrategy,
        const std::string& transaction_id) override {

        last_transaction_id_ = transaction_id;

        core::FileActionRecord rec;
        rec.transaction_id = transaction_id;
        rec.original_path = source;
        rec.executed_path = dest_base / suggested_path;
        rec.action_type = action_type;
        rec.timestamp = std::chrono::system_clock::now();
        
        return core::Result<core::FileActionRecord, core::Error>::success(std::move(rec));
    }

    core::Result<void, core::Error> revert(const core::FileActionRecord& record) override {
        reverted_record_ = record;
        revert_called_ = true;
        return core::Result<void, core::Error>::success();
    }

    bool revert_called_ = false;
    std::string last_transaction_id_;
    core::FileActionRecord reverted_record_;
};

// ============================================================================
// Test Fixture
// ============================================================================

class AppShellTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto current = std::filesystem::current_path();
        scan_root_ = current / "test_app_shell_scan";
        dest_root_ = current / "test_app_shell_dest";
        config_root_ = current / "test_app_shell_config";

        std::filesystem::remove_all(scan_root_);
        std::filesystem::remove_all(dest_root_);
        std::filesystem::remove_all(config_root_);
        
        std::filesystem::create_directories(scan_root_);
        std::filesystem::create_directories(dest_root_);
        std::filesystem::create_directories(config_root_);

        // Write dummy file for scanner
        {
            std::ofstream f(scan_root_ / "receipt.bin");
            f << "mock receipt data";
        }
    }

    void TearDown() override {
        std::filesystem::remove_all(scan_root_);
        std::filesystem::remove_all(dest_root_);
        std::filesystem::remove_all(config_root_);
    }

    std::filesystem::path scan_root_;
    std::filesystem::path dest_root_;
    std::filesystem::path config_root_;
};

// ============================================================================
// Test Cases
// ============================================================================

TEST_F(AppShellTest, InitializationAndShutdownLifecycle) {
    auto scanner = std::make_unique<MockScanner>();
    auto ocr = std::make_unique<MockOcr>();
    auto provider = std::make_unique<MockProvider>();
    auto organizer = std::make_unique<MockOrganizer>();

    AppShell shell(std::move(scanner), std::move(ocr), std::move(provider), std::move(organizer));
    
    // Test initialization in sandbox
    auto init_res = shell.initialize(config_root_.string());
    ASSERT_TRUE(init_res.has_value());
    EXPECT_TRUE(shell.is_initialized());

    // Test settings access
    AppSettings settings = shell.settings();
    EXPECT_FALSE(settings.database_path.empty());

    // Test shutdown
    auto shutdown_res = shell.shutdown();
    EXPECT_TRUE(shutdown_res.has_value());
    EXPECT_FALSE(shell.is_initialized());
}

TEST_F(AppShellTest, SettingsManagementFacadeUpdatesConfig) {
    auto scanner = std::make_unique<MockScanner>();
    auto ocr = std::make_unique<MockOcr>();
    auto provider = std::make_unique<MockProvider>();
    auto organizer = std::make_unique<MockOrganizer>();

    AppShell shell(std::move(scanner), std::move(ocr), std::move(provider), std::move(organizer));
    ASSERT_TRUE(shell.initialize(config_root_.string()).has_value());

    AppSettings settings = shell.settings();
    settings.scan.recursive = false;
    settings.scan.max_depth = 5;

    // Save updated settings
    auto update_res = shell.update_settings(settings);
    ASSERT_TRUE(update_res.has_value());

    // Verify settings got updated
    AppSettings loaded = shell.settings();
    EXPECT_FALSE(loaded.scan.recursive);
    EXPECT_EQ(loaded.scan.max_depth, 5u);
}

TEST_F(AppShellTest, EndToEndJobRunAndUndoRollback) {
    auto scanner = std::make_unique<MockScanner>();
    auto ocr = std::make_unique<MockOcr>();
    auto provider = std::make_unique<MockProvider>();
    
    auto org_ptr = std::make_unique<MockOrganizer>();
    auto* mock_org_raw = org_ptr.get();

    AppShell shell(std::move(scanner), std::move(ocr), std::move(provider), std::move(org_ptr));
    ASSERT_TRUE(shell.initialize(config_root_.string()).has_value());

    std::mutex cv_m;
    std::condition_variable cv;
    bool completed = false;

    auto callback = [&](const PipelineProgress& progress) {
        std::lock_guard<std::mutex> lock(cv_m);
        if (progress.state == PipelineState::kCompleted || progress.state == PipelineState::kFailed) {
            completed = true;
            cv.notify_one();
        }
    };

    // Run job
    auto job_res = shell.start_job(scan_root_.string(), dest_root_.string(), core::FileActionType::kCopy, callback);
    ASSERT_TRUE(job_res.has_value());
    EXPECT_TRUE(shell.is_job_running());

    // Wait for completion callback
    std::unique_lock<std::mutex> lk(cv_m);
    cv.wait_for(lk, std::chrono::seconds(5), [&] { return completed; });

    EXPECT_FALSE(shell.is_job_running());
    shell.wait_for_job();

    EXPECT_EQ(shell.progress().state, PipelineState::kCompleted);
    EXPECT_EQ(shell.progress().total_files, 1u);
    EXPECT_EQ(shell.progress().organized_files, 1u);

    // Revert the action history using the transaction ID from MockOrganizer
    ASSERT_FALSE(mock_org_raw->last_transaction_id_.empty());
    auto undo_res = shell.undo_transaction(mock_org_raw->last_transaction_id_);
    ASSERT_TRUE(undo_res.has_value());
    EXPECT_TRUE(mock_org_raw->revert_called_);
    EXPECT_EQ(mock_org_raw->reverted_record_.transaction_id, mock_org_raw->last_transaction_id_);
}

}  // namespace lilolify::app::test
