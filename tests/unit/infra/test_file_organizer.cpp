// ============================================================================
// Lilolify — Unit Tests: File Organizer
// ============================================================================

#include <lilolify/infra/organization/file_organizer.hpp>

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace lilolify::infra::test {

class FileOrganizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup sandbox directory in current work directory (inside build folder)
        sandbox_dir_ = std::filesystem::current_path() / "test_organizer_sandbox";
        std::filesystem::create_directories(sandbox_dir_);
        
        src_dir_ = sandbox_dir_ / "src";
        dst_dir_ = sandbox_dir_ / "dst";
        
        std::filesystem::create_directories(src_dir_);
        std::filesystem::create_directories(dst_dir_);
    }

    void TearDown() override {
        // Clean up the sandbox completely
        std::filesystem::remove_all(sandbox_dir_);
    }

    std::filesystem::path create_test_file(const std::filesystem::path& parent,
                                           const std::string& filename,
                                           const std::string& content = "Hello Lilolify") {
        auto path = parent / filename;
        std::ofstream ofs(path, std::ios::binary);
        ofs << content;
        ofs.close();
        return path;
    }

    std::filesystem::path sandbox_dir_;
    std::filesystem::path src_dir_;
    std::filesystem::path dst_dir_;
};

// ============================================================================
// Test Cases
// ============================================================================

TEST_F(FileOrganizerTest, MoveExecutesAndRevertsSuccessfully) {
    FileOrganizer organizer;
    auto src_file = create_test_file(src_dir_, "receipt.png", "image-content");
    
    // Execute move
    auto res = organizer.execute(
        src_file,
        dst_dir_,
        "Receipts/2026/receipt.png",
        core::FileActionType::kMove,
        core::CollisionStrategy::kSkip,
        "tx-123"
    );

    ASSERT_TRUE(res.has_value());
    auto record = res.value();
    
    EXPECT_EQ(record.transaction_id, "tx-123");
    EXPECT_EQ(record.original_path, src_file);
    EXPECT_EQ(record.action_type, core::FileActionType::kMove);
    EXPECT_FALSE(std::filesystem::exists(src_file));
    EXPECT_TRUE(std::filesystem::exists(record.executed_path));
    
    // Revert move
    auto rev_res = organizer.revert(record);
    ASSERT_TRUE(rev_res.has_value());
    
    EXPECT_TRUE(std::filesystem::exists(src_file));
    EXPECT_FALSE(std::filesystem::exists(record.executed_path));
}

TEST_F(FileOrganizerTest, CopyExecutesAndRevertsSuccessfully) {
    FileOrganizer organizer;
    auto src_file = create_test_file(src_dir_, "document.pdf", "pdf-content");
    
    // Execute copy
    auto res = organizer.execute(
        src_file,
        dst_dir_,
        "Documents/document.pdf",
        core::FileActionType::kCopy,
        core::CollisionStrategy::kSkip,
        "tx-456"
    );

    ASSERT_TRUE(res.has_value());
    auto record = res.value();
    
    EXPECT_TRUE(std::filesystem::exists(src_file)); // Original preserved
    EXPECT_TRUE(std::filesystem::exists(record.executed_path));
    
    // Revert copy
    auto rev_res = organizer.revert(record);
    ASSERT_TRUE(rev_res.has_value());
    
    EXPECT_TRUE(std::filesystem::exists(src_file));
    EXPECT_FALSE(std::filesystem::exists(record.executed_path)); // Copy deleted
}

TEST_F(FileOrganizerTest, CollisionStrategySkipFails) {
    FileOrganizer organizer;
    auto src_file = create_test_file(src_dir_, "file.txt", "new-content");
    
    // Create an existing file at destination path
    auto dst_path = dst_dir_ / "Text/file.txt";
    std::filesystem::create_directories(dst_path.parent_path());
    create_test_file(dst_path.parent_path(), "file.txt", "old-content");

    auto res = organizer.execute(
        src_file,
        dst_dir_,
        "Text/file.txt",
        core::FileActionType::kMove,
        core::CollisionStrategy::kSkip,
        "tx-789"
    );

    ASSERT_TRUE(res.has_error());
    EXPECT_EQ(res.error().code(), core::ErrorCode::kFileAlreadyExists);
    
    // Verify target content was not modified
    std::ifstream ifs(dst_path);
    std::string text((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    EXPECT_EQ(text, "old-content");
}

TEST_F(FileOrganizerTest, CollisionStrategyOverwriteSucceeds) {
    FileOrganizer organizer;
    auto src_file = create_test_file(src_dir_, "file.txt", "new-content");
    
    // Create existing file at target
    auto dst_path = dst_dir_ / "Text/file.txt";
    std::filesystem::create_directories(dst_path.parent_path());
    create_test_file(dst_path.parent_path(), "file.txt", "old-content");

    auto res = organizer.execute(
        src_file,
        dst_dir_,
        "Text/file.txt",
        core::FileActionType::kMove,
        core::CollisionStrategy::kOverwrite,
        "tx-789"
    );

    ASSERT_TRUE(res.has_value());
    EXPECT_TRUE(std::filesystem::exists(res.value().executed_path));
    
    // Verify target content was overwritten
    std::ifstream ifs(dst_path);
    std::string text((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    EXPECT_EQ(text, "new-content");
}

TEST_F(FileOrganizerTest, CollisionStrategyRenameIncrementsSuffix) {
    FileOrganizer organizer;
    
    auto src_file = create_test_file(src_dir_, "report.txt", "new-content");
    
    // Populate dst/Reports/report.txt and dst/Reports/report_1.txt
    auto target_dir = dst_dir_ / "Reports";
    std::filesystem::create_directories(target_dir);
    create_test_file(target_dir, "report.txt", "orig");
    create_test_file(target_dir, "report_1.txt", "orig-1");

    auto res = organizer.execute(
        src_file,
        dst_dir_,
        "Reports/report.txt",
        core::FileActionType::kMove,
        core::CollisionStrategy::kRename,
        "tx-999"
    );

    ASSERT_TRUE(res.has_value());
    
    // Suffix should be resolved to report_2.txt
    std::filesystem::path expected = target_dir / "report_2.txt";
    EXPECT_EQ(res.value().executed_path, expected);
    EXPECT_TRUE(std::filesystem::exists(expected));
    
    // Originals shouldn't be touched
    EXPECT_TRUE(std::filesystem::exists(target_dir / "report.txt"));
    EXPECT_TRUE(std::filesystem::exists(target_dir / "report_1.txt"));
}

TEST_F(FileOrganizerTest, MissingSourceReturnsFileNotFound) {
    FileOrganizer organizer;
    auto missing_path = src_dir_ / "ghost.txt";

    auto res = organizer.execute(
        missing_path,
        dst_dir_,
        "ghost.txt",
        core::FileActionType::kMove,
        core::CollisionStrategy::kSkip,
        "tx-999"
    );

    ASSERT_TRUE(res.has_error());
    EXPECT_EQ(res.error().code(), core::ErrorCode::kFileNotFound);
}

}  // namespace lilolify::infra::test
