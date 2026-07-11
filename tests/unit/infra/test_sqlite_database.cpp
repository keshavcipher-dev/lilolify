// ============================================================================
// Lilolify — Unit Tests: SqliteDatabase
// ============================================================================

#include <lilolify/infra/database/sqlite_database.hpp>

#include <gtest/gtest.h>
#include <filesystem>
#include <chrono>

namespace lilolify::infra::test {

class SqliteDatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Construct unique sandboxed database path
        db_path_ = std::filesystem::current_path() / "test_lilolify_persistence.db";
        std::filesystem::remove(db_path_); // Clean any leftover file
    }

    void TearDown() override {
        // Ensure database connection is closed to release handle lock, then delete file
        (void)db_.close();
        std::filesystem::remove(db_path_);
    }

    std::filesystem::path db_path_;
    SqliteDatabase db_;
};

// ============================================================================
// Test Cases
// ============================================================================

TEST_F(SqliteDatabaseTest, DatabaseLifecycleAndSchemaInitialization) {
    // Open connection
    auto open_res = db_.open(db_path_.string());
    ASSERT_TRUE(open_res.has_value());
    EXPECT_TRUE(std::filesystem::exists(db_path_));

    // Run schema
    auto schema_res = db_.initialize_schema();
    EXPECT_TRUE(schema_res.has_value());

    // Close connection
    auto close_res = db_.close();
    EXPECT_TRUE(close_res.has_value());
}

TEST_F(SqliteDatabaseTest, FileEntryCRUDOperationsAndMetadataSerialization) {
    ASSERT_TRUE(db_.open(db_path_.string()).has_value());
    ASSERT_TRUE(db_.initialize_schema().has_value());

    auto now = std::chrono::system_clock::now();
    core::FileEntry entry("C:/workspace/report.pdf", 40960, now, now);
    entry.set_mime_type("application/pdf");
    entry.set_status(core::ProcessingStatus::kCompleted);

    // Attach deep metadata
    core::FileMetadata meta;
    meta.sha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    meta.page_count = 12;
    meta.text_preview = "Financial report summary...";
    entry.set_metadata(meta);

    // 1. Save entry
    auto save_res = db_.save_file(entry);
    ASSERT_TRUE(save_res.has_value());

    // 2. Find file (Verify CRUD insertion)
    auto find_res = db_.find_file(entry.path());
    ASSERT_TRUE(find_res.has_value());
    ASSERT_TRUE(find_res.value().has_value());
    
    auto retrieved = find_res.value().value();
    EXPECT_EQ(retrieved.path(), entry.path());
    EXPECT_EQ(retrieved.filename(), "report.pdf");
    EXPECT_EQ(retrieved.extension(), ".pdf");
    EXPECT_EQ(retrieved.size_bytes(), 40960u);
    EXPECT_EQ(retrieved.mime_type(), "application/pdf");
    EXPECT_EQ(retrieved.status(), core::ProcessingStatus::kCompleted);
    
    ASSERT_TRUE(retrieved.metadata().has_value());
    EXPECT_EQ(retrieved.metadata().value().sha256, meta.sha256);
    EXPECT_EQ(retrieved.metadata().value().page_count, 12u);
    EXPECT_EQ(retrieved.metadata().value().text_preview, "Financial report summary...");

    // 3. Upsert update (Save changes on conflict)
    retrieved.set_status(core::ProcessingStatus::kFailed);
    core::FileMetadata updated_meta = retrieved.metadata().value();
    updated_meta.page_count = 15;
    retrieved.set_metadata(updated_meta);
    
    ASSERT_TRUE(db_.save_file(retrieved).has_value());
    
    auto find_updated = db_.find_file(entry.path());
    ASSERT_TRUE(find_updated.value().has_value());
    EXPECT_EQ(find_updated.value().value().status(), core::ProcessingStatus::kFailed);
    EXPECT_EQ(find_updated.value().value().metadata().value().page_count, 15u);

    // 4. Remove file
    auto rm_res = db_.remove_file(entry.path());
    EXPECT_TRUE(rm_res.has_value());
    
    auto find_deleted = db_.find_file(entry.path());
    EXPECT_FALSE(find_deleted.value().has_value());
}

TEST_F(SqliteDatabaseTest, FileActionRecordOperationsAndClearHistory) {
    ASSERT_TRUE(db_.open(db_path_.string()).has_value());
    ASSERT_TRUE(db_.initialize_schema().has_value());

    core::FileActionRecord action;
    action.transaction_id = "batch-tx-99";
    action.original_path = "C:/org/old.txt";
    action.executed_path = "C:/org/NewDir/new.txt";
    action.action_type = core::FileActionType::kMove;
    action.timestamp = std::chrono::system_clock::now();

    // Save action
    auto save_res = db_.save_action(action);
    ASSERT_TRUE(save_res.has_value());

    // Query transaction history
    auto hist_res = db_.get_transaction_history("batch-tx-99");
    ASSERT_TRUE(hist_res.has_value());
    ASSERT_EQ(hist_res.value().size(), 1u);
    
    auto retrieved = hist_res.value()[0];
    EXPECT_EQ(retrieved.transaction_id, "batch-tx-99");
    EXPECT_EQ(retrieved.original_path, "C:/org/old.txt");
    EXPECT_EQ(retrieved.executed_path, "C:/org/NewDir/new.txt");
    EXPECT_EQ(retrieved.action_type, core::FileActionType::kMove);

    // Clear history
    ASSERT_TRUE(db_.clear_history().has_value());
    auto list_empty = db_.get_all_history();
    EXPECT_EQ(list_empty.value().size(), 0u);
}

TEST_F(SqliteDatabaseTest, TransactionRollbackSafety) {
    ASSERT_TRUE(db_.open(db_path_.string()).has_value());
    ASSERT_TRUE(db_.initialize_schema().has_value());

    auto now = std::chrono::system_clock::now();
    core::FileEntry entry1("C:/file1.txt", 100, now, now);
    core::FileEntry entry2("C:/file2.txt", 200, now, now);

    // 1. Successful commit transaction
    ASSERT_TRUE(db_.begin_transaction().has_value());
    ASSERT_TRUE(db_.save_file(entry1).has_value());
    ASSERT_TRUE(db_.commit_transaction().has_value());

    auto check1 = db_.find_file("C:/file1.txt");
    EXPECT_TRUE(check1.value().has_value()); // Committed entry exists

    // 2. Rollback transaction
    ASSERT_TRUE(db_.begin_transaction().has_value());
    ASSERT_TRUE(db_.save_file(entry2).has_value());
    ASSERT_TRUE(db_.rollback_transaction().has_value()); // Revert save

    auto check2 = db_.find_file("C:/file2.txt");
    EXPECT_FALSE(check2.value().has_value()); // Rolled back entry does NOT exist
}

}  // namespace lilolify::infra::test
