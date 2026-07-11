// ============================================================================
// Lilolify — SqliteDatabase
// ============================================================================
// RAII local persistence database wrapper using SQLite.
// ============================================================================

#pragma once

#include <lilolify/core/entities/file_entry.hpp>
#include <lilolify/core/value_objects/file_action_record.hpp>
#include <lilolify/core/error.hpp>
#include <lilolify/core/result.hpp>

#include <memory>
#include <string>
#include <vector>
#include <optional>

// Forward declaration of raw SQLite handle to avoid exposing C API headers
struct sqlite3;

namespace lilolify::infra {

/// Manages SQL schema setup, transaction scopes, and serialization
/// of file entities and history logs to/from local disk db file.
class SqliteDatabase {
public:
    SqliteDatabase() noexcept;
    ~SqliteDatabase();

    // Disable copy/move
    SqliteDatabase(const SqliteDatabase&) = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;
    SqliteDatabase(SqliteDatabase&&) = delete;
    SqliteDatabase& operator=(SqliteDatabase&&) = delete;

    /// Open local database file. Creates the file if it does not exist.
    [[nodiscard]] core::Result<void, core::Error> open(const std::string& db_path);

    /// Close database connection.
    [[nodiscard]] core::Result<void, core::Error> close() noexcept;

    /// Creates files and action_history tables if missing.
    [[nodiscard]] core::Result<void, core::Error> initialize_schema();

    // ========================================================================
    // Transactions
    // ========================================================================
    [[nodiscard]] core::Result<void, core::Error> begin_transaction();
    [[nodiscard]] core::Result<void, core::Error> commit_transaction();
    [[nodiscard]] core::Result<void, core::Error> rollback_transaction();

    // ========================================================================
    // File Persistence (FileRepository API)
    // ========================================================================
    
    /// Insert or update a file entry.
    [[nodiscard]] core::Result<void, core::Error> save_file(const core::FileEntry& entry);

    /// Search for a file entry by absolute file path.
    [[nodiscard]] core::Result<std::optional<core::FileEntry>, core::Error> find_file(
        const core::FilePath& path);

    /// Get all tracked files.
    [[nodiscard]] core::Result<std::vector<core::FileEntry>, core::Error> find_all_files();

    /// Remove a file entry from database tracking.
    [[nodiscard]] core::Result<void, core::Error> remove_file(const core::FilePath& path);

    // ========================================================================
    // History Persistence (HistoryRepository API)
    // ========================================================================

    /// Log an executed filesystem action record.
    [[nodiscard]] core::Result<void, core::Error> save_action(
        const core::FileActionRecord& record);

    /// Retrieve actions belonging to a specific transaction session.
    [[nodiscard]] core::Result<std::vector<core::FileActionRecord>, core::Error> get_transaction_history(
        const std::string& transaction_id);

    /// Get all action history logs ordered by timestamp ascending.
    [[nodiscard]] core::Result<std::vector<core::FileActionRecord>, core::Error> get_all_history();

    /// Clear all action records from the ledger database.
    [[nodiscard]] core::Result<void, core::Error> clear_history();

    /// Get SQLite raw pointer (useful for statement binding internally).
    [[nodiscard]] sqlite3* handle() const noexcept { return db_; }

private:
    sqlite3* db_;
};

}  // namespace lilolify::infra
