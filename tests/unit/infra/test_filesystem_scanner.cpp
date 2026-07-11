// ============================================================================
// Lilolify — Unit Tests: FilesystemScanner
// ============================================================================
// Integration-style tests that create real temp directories and files to
// verify the scanner works correctly end-to-end.
// ============================================================================

#include <lilolify/infra/filesystem/filesystem_scanner.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace lilolify::infra::test {

namespace fs = std::filesystem;

/// Test fixture that creates and cleans up a temporary directory tree.
class FilesystemScannerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a unique temp directory for each test
        test_dir_ = fs::temp_directory_path() / "lilolify_scanner_test";
        fs::create_directories(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(test_dir_, ec);
    }

    /// Create a file with given content in the test directory.
    void create_file(const fs::path& relative_path,
                     const std::string& content = "test content") {
        auto full_path = test_dir_ / relative_path;
        fs::create_directories(full_path.parent_path());
        std::ofstream file(full_path);
        file << content;
    }

    /// Create a file with specific binary content.
    void create_binary_file(const fs::path& relative_path,
                            const std::vector<std::uint8_t>& bytes) {
        auto full_path = test_dir_ / relative_path;
        fs::create_directories(full_path.parent_path());
        std::ofstream file(full_path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
    }

    /// Create default scan options pointing to the test directory.
    core::ScanOptions make_options() {
        core::ScanOptions opts;
        opts.root_directory = test_dir_;
        opts.detect_mime_type = false;  // Faster for tests without real files
        opts.progress_interval = 1;     // Report every file for testing
        return opts;
    }

    FilesystemScanner scanner_;
    fs::path test_dir_;
};

// ============================================================================
// Basic Scanning Tests
// ============================================================================

TEST_F(FilesystemScannerTest, ScanEmptyDirectory) {
    auto opts = make_options();
    auto result = scanner_.scan(opts);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().file_count(), 0u);
    EXPECT_TRUE(result.value().is_clean());
    EXPECT_FALSE(result.value().was_cancelled);
}

TEST_F(FilesystemScannerTest, ScanFlatDirectoryFindsAllFiles) {
    create_file("file1.txt", "hello");
    create_file("file2.txt", "world");
    create_file("file3.jpg", "image data");

    auto opts = make_options();
    auto result = scanner_.scan(opts);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().file_count(), 3u);
}

TEST_F(FilesystemScannerTest, ScanNestedDirectoriesRecursively) {
    create_file("root.txt");
    create_file("sub1/a.txt");
    create_file("sub1/sub2/b.txt");
    create_file("sub1/sub2/sub3/c.txt");

    auto opts = make_options();
    auto result = scanner_.scan(opts);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().file_count(), 4u);
}

TEST_F(FilesystemScannerTest, NonRecursiveScanOnlyFindsRootFiles) {
    create_file("root.txt");
    create_file("sub/nested.txt");

    auto opts = make_options();
    opts.recursive = false;
    auto result = scanner_.scan(opts);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().file_count(), 1u);
    EXPECT_EQ(result.value().files[0].filename(), "root.txt");
}

// ============================================================================
// Filtering Tests
// ============================================================================

TEST_F(FilesystemScannerTest, ExtensionIncludeFilter) {
    create_file("photo.jpg", "jpeg data");
    create_file("doc.pdf", "pdf data");
    create_file("notes.txt", "text");

    auto opts = make_options();
    opts.include_extensions = {".jpg", ".png"};
    auto result = scanner_.scan(opts);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().file_count(), 1u);
    EXPECT_EQ(result.value().files[0].extension(), ".jpg");
}

TEST_F(FilesystemScannerTest, ExtensionExcludeFilter) {
    create_file("keep.txt", "keep me");
    create_file("temp.tmp", "delete me");
    create_file("backup.bak", "also delete");

    auto opts = make_options();
    opts.exclude_extensions = {".tmp", ".bak"};
    auto result = scanner_.scan(opts);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().file_count(), 1u);
    EXPECT_EQ(result.value().files[0].extension(), ".txt");
}

TEST_F(FilesystemScannerTest, SizeFilterMinimum) {
    create_file("tiny.txt", "hi");           // ~2 bytes
    create_file("bigger.txt", std::string(1000, 'x'));  // 1000 bytes

    auto opts = make_options();
    opts.min_file_size = 100;
    auto result = scanner_.scan(opts);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().file_count(), 1u);
}

TEST_F(FilesystemScannerTest, SizeFilterMaximum) {
    create_file("small.txt", "hello");                   // ~5 bytes
    create_file("large.txt", std::string(10000, 'x'));   // 10000 bytes

    auto opts = make_options();
    opts.max_file_size = 100;
    auto result = scanner_.scan(opts);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().file_count(), 1u);
}

TEST_F(FilesystemScannerTest, DepthLimit) {
    create_file("level0.txt");
    create_file("a/level1.txt");
    create_file("a/b/level2.txt");
    create_file("a/b/c/level3.txt");

    auto opts = make_options();
    opts.max_depth = 1;  // Only root + 1 level deep
    auto result = scanner_.scan(opts);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().file_count(), 2u);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(FilesystemScannerTest, ScanNonexistentDirectoryReturnsError) {
    auto opts = make_options();
    opts.root_directory = "C:/this/path/does/not/exist/at/all";
    auto result = scanner_.scan(opts);

    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), core::ErrorCode::kDirectoryNotFound);
}

TEST_F(FilesystemScannerTest, ScanSingleFileSucceeds) {
    create_file("just_a_file.txt");

    auto opts = make_options();
    opts.root_directory = test_dir_ / "just_a_file.txt";
    auto result = scanner_.scan(opts);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().file_count(), 1u);
    EXPECT_EQ(result.value().files[0].filename(), "just_a_file.txt");
}

TEST_F(FilesystemScannerTest, InvalidOptionsReturnsError) {
    core::ScanOptions opts;  // Empty root_directory
    auto result = scanner_.scan(opts);

    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), core::ErrorCode::kInvalidArgument);
}

// ============================================================================
// Progress Reporting Tests
// ============================================================================

TEST_F(FilesystemScannerTest, ProgressCallbackInvoked) {
    for (int i = 0; i < 5; i++) {
        create_file("file" + std::to_string(i) + ".txt", "content");
    }

    auto opts = make_options();
    opts.progress_interval = 2;  // Report every 2 files

    int callback_count = 0;
    core::ScanProgress last_progress;

    auto result = scanner_.scan(opts, [&](const core::ScanProgress& p) {
        callback_count++;
        last_progress = p;
    });

    ASSERT_TRUE(result.has_value());
    EXPECT_GT(callback_count, 0);
    EXPECT_GT(last_progress.files_found, 0u);
}

TEST_F(FilesystemScannerTest, NullProgressCallbackIsAllowed) {
    create_file("file.txt");

    auto opts = make_options();
    auto result = scanner_.scan(opts, nullptr);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().file_count(), 1u);
}

// ============================================================================
// Cancellation Tests
// ============================================================================

TEST_F(FilesystemScannerTest, CancelStopsScan) {
    // Create many files to ensure cancellation has time to trigger
    for (int i = 0; i < 50; i++) {
        create_file("dir" + std::to_string(i / 10) +
                        "/file" + std::to_string(i) + ".txt",
                    "content");
    }

    auto opts = make_options();
    opts.progress_interval = 1;

    // Cancel after first progress report
    auto result = scanner_.scan(opts, [&](const core::ScanProgress&) {
        scanner_.cancel();
    });

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().was_cancelled);
    // Should have partial results (not all 50 files)
    EXPECT_LT(result.value().file_count(), 50u);
}

TEST_F(FilesystemScannerTest, IsScanningReturnsFalseWhenNotScanning) {
    EXPECT_FALSE(scanner_.is_scanning());
}

// ============================================================================
// MIME Detection Integration Tests
// ============================================================================

TEST_F(FilesystemScannerTest, MimeDetectionByMagicBytes) {
    // Create a file with JPEG magic bytes
    create_binary_file("photo.jpg",
        {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46,
         0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01});

    auto opts = make_options();
    opts.detect_mime_type = true;  // Enable magic byte detection
    auto result = scanner_.scan(opts);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().file_count(), 1u);
    EXPECT_EQ(result.value().files[0].mime_type(), "image/jpeg");
}

TEST_F(FilesystemScannerTest, MimeDetectionByExtensionOnly) {
    create_file("document.pdf", "not a real PDF");

    auto opts = make_options();
    opts.detect_mime_type = false;  // Extension-only mode
    auto result = scanner_.scan(opts);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result.value().file_count(), 1u);
    EXPECT_EQ(result.value().files[0].mime_type(), "application/pdf");
}

// ============================================================================
// ScanResult Properties Tests
// ============================================================================

TEST_F(FilesystemScannerTest, ScanResultTotalSizeBytes) {
    create_file("a.txt", std::string(100, 'a'));
    create_file("b.txt", std::string(200, 'b'));

    auto opts = make_options();
    auto result = scanner_.scan(opts);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().file_count(), 2u);
    EXPECT_GE(result.value().total_size_bytes(), 300u);
}

TEST_F(FilesystemScannerTest, ScanResultFinalProgress) {
    create_file("file1.txt");
    create_file("file2.txt");
    create_file("sub/file3.txt");

    auto opts = make_options();
    auto result = scanner_.scan(opts);

    ASSERT_TRUE(result.has_value());
    auto& progress = result.value().final_progress;
    EXPECT_EQ(progress.files_found, 3u);
    EXPECT_EQ(progress.files_processed, 3u);
    EXPECT_GT(progress.directories_scanned, 0u);
}

}  // namespace lilolify::infra::test
