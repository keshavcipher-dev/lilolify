// ============================================================================
// Lilolify — SqliteDatabase Implementation
// ============================================================================

#include <lilolify/infra/database/sqlite_database.hpp>

#include <sqlite3.h>
#include <filesystem>
#include <chrono>

namespace lilolify::infra {

namespace {

// ============================================================================
// RAII Prepared Statement Helper
// ============================================================================

class SqliteStatement {
public:
    SqliteStatement(sqlite3* db, const std::string& sql, core::Result<void, core::Error>& res_out) noexcept {
        int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr);
        if (rc != SQLITE_OK) {
            res_out = core::Result<void, core::Error>::failure(
                core::Error(core::ErrorCode::kFileWriteError,
                            "Failed to prepare SQL statement: " + std::string(sqlite3_errmsg(db))));
        } else {
            res_out = core::Result<void, core::Error>::success();
        }
    }

    ~SqliteStatement() {
        if (stmt_) {
            sqlite3_finalize(stmt_);
        }
    }

    // Disable copy/move
    SqliteStatement(const SqliteStatement&) = delete;
    SqliteStatement& operator=(const SqliteStatement&) = delete;

    int step() noexcept {
        return sqlite3_step(stmt_);
    }

    void reset() noexcept {
        sqlite3_reset(stmt_);
        sqlite3_clear_bindings(stmt_);
    }

    void bind_int(int index, int val) noexcept {
        sqlite3_bind_int(stmt_, index, val);
    }

    void bind_int64(int index, std::int64_t val) noexcept {
        sqlite3_bind_int64(stmt_, index, val);
    }

    void bind_text(int index, const std::string& val) noexcept {
        sqlite3_bind_text(stmt_, index, val.c_str(), -1, SQLITE_TRANSIENT);
    }

    void bind_null(int index) noexcept {
        sqlite3_bind_null(stmt_, index);
    }

    void bind_optional_int(int index, const std::optional<std::uint32_t>& val) noexcept {
        if (val.has_value()) {
            sqlite3_bind_int64(stmt_, index, static_cast<std::int64_t>(val.value()));
        } else {
            sqlite3_bind_null(stmt_, index);
        }
    }

    bool column_is_null(int col) const noexcept {
        return sqlite3_column_type(stmt_, col) == SQLITE_NULL;
    }

    int column_int(int col) const noexcept {
        return sqlite3_column_int(stmt_, col);
    }

    std::int64_t column_int64(int col) const noexcept {
        return sqlite3_column_int64(stmt_, col);
    }

    std::string column_text(int col) const noexcept {
        const auto* txt = sqlite3_column_text(stmt_, col);
        return txt ? std::string(reinterpret_cast<const char*>(txt)) : "";
    }

    std::optional<std::uint32_t> column_optional_int(int col) const noexcept {
        if (column_is_null(col)) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(sqlite3_column_int64(stmt_, col));
    }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

// Timestamp conversion helpers
std::int64_t to_ms(const core::Timestamp& tp) noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

core::Timestamp to_timestamp(std::int64_t ms) noexcept {
    return core::Timestamp(std::chrono::milliseconds(ms));
}

}  // namespace

// ============================================================================
// Constructor / Destructor
// ============================================================================

SqliteDatabase::SqliteDatabase() noexcept : db_(nullptr) {}

SqliteDatabase::~SqliteDatabase() {
    (void)close();
}

// ============================================================================
// Connection Lifecycle
// ============================================================================

core::Result<void, core::Error> SqliteDatabase::open(const std::string& db_path) {
    if (db_) {
        return core::Result<void, core::Error>::success();
    }

    std::filesystem::path path(db_path);
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err_msg = db_ ? sqlite3_errmsg(db_) : "Failed to allocate sqlite3 database object";
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kFileAccessDenied, "SQLite open failed: " + err_msg));
    }

    // Enable WAL mode for high concurrency
    char* zErrMsg = nullptr;
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &zErrMsg);
    if (zErrMsg) {
        sqlite3_free(zErrMsg);
    }

    return core::Result<void, core::Error>::success();
}

core::Result<void, core::Error> SqliteDatabase::close() noexcept {
    if (!db_) {
        return core::Result<void, core::Error>::success();
    }

    int rc = sqlite3_close(db_);
    if (rc != SQLITE_OK) {
        // SQLITE_BUSY if unfinalized statements exist.
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kFileAccessDenied, "SQLite close failed: connection busy"));
    }

    db_ = nullptr;
    return core::Result<void, core::Error>::success();
}

// ============================================================================
// Schema Initialization
// ============================================================================

core::Result<void, core::Error> SqliteDatabase::initialize_schema() {
    if (!db_) {
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kNotFound, "Database not open"));
    }

    const std::string sql_files = 
        "CREATE TABLE IF NOT EXISTS files (\n"
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
        "    path TEXT UNIQUE NOT NULL,\n"
        "    filename TEXT NOT NULL,\n"
        "    extension TEXT NOT NULL,\n"
        "    size_bytes INTEGER NOT NULL,\n"
        "    mime_type TEXT NOT NULL,\n"
        "    created_at INTEGER NOT NULL,\n"
        "    modified_at INTEGER NOT NULL,\n"
        "    scanned_at INTEGER NOT NULL,\n"
        "    status INTEGER NOT NULL,\n"
        "    sha256 TEXT,\n"
        "    image_width INTEGER,\n"
        "    image_height INTEGER,\n"
        "    page_count INTEGER,\n"
        "    text_preview TEXT\n"
        ");";

    const std::string sql_history = 
        "CREATE TABLE IF NOT EXISTS action_history (\n"
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
        "    transaction_id TEXT NOT NULL,\n"
        "    original_path TEXT NOT NULL,\n"
        "    executed_path TEXT NOT NULL,\n"
        "    action_type INTEGER NOT NULL,\n"
        "    timestamp INTEGER NOT NULL\n"
        ");";

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql_files.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string msg = err_msg ? err_msg : "Unknown SQLite error";
        sqlite3_free(err_msg);
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kFileWriteError, "Schema initialization failed: " + msg));
    }

    rc = sqlite3_exec(db_, sql_history.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string msg = err_msg ? err_msg : "Unknown SQLite error";
        sqlite3_free(err_msg);
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kFileWriteError, "Schema history initialization failed: " + msg));
    }

    return core::Result<void, core::Error>::success();
}

// ============================================================================
// Transactions
// ============================================================================

core::Result<void, core::Error> SqliteDatabase::begin_transaction() {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "";
        sqlite3_free(err);
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kFileWriteError, "Failed to begin transaction: " + msg));
    }
    return core::Result<void, core::Error>::success();
}

core::Result<void, core::Error> SqliteDatabase::commit_transaction() {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "";
        sqlite3_free(err);
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kFileWriteError, "Failed to commit transaction: " + msg));
    }
    return core::Result<void, core::Error>::success();
}

core::Result<void, core::Error> SqliteDatabase::rollback_transaction() {
    char* err = nullptr;
    int rc = sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string msg = err ? err : "";
        sqlite3_free(err);
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kFileWriteError, "Failed to rollback transaction: " + msg));
    }
    return core::Result<void, core::Error>::success();
}

// ============================================================================
// File Persistence
// ============================================================================

core::Result<void, core::Error> SqliteDatabase::save_file(const core::FileEntry& entry) {
    if (!db_) {
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kNotFound, "Database not open"));
    }

    const std::string sql = 
        "INSERT INTO files (path, filename, extension, size_bytes, mime_type, "
        "created_at, modified_at, scanned_at, status, sha256, image_width, "
        "image_height, page_count, text_preview) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14) "
        "ON CONFLICT(path) DO UPDATE SET "
        "filename=excluded.filename, extension=excluded.extension, size_bytes=excluded.size_bytes, "
        "mime_type=excluded.mime_type, created_at=excluded.created_at, modified_at=excluded.modified_at, "
        "scanned_at=excluded.scanned_at, status=excluded.status, sha256=excluded.sha256, "
        "image_width=excluded.image_width, image_height=excluded.image_height, "
        "page_count=excluded.page_count, text_preview=excluded.text_preview;";

    core::Result<void, core::Error> prep_res = core::Result<void, core::Error>::success();
    SqliteStatement stmt(db_, sql, prep_res);
    if (prep_res.has_error()) {
        return prep_res;
    }

    stmt.bind_text(1, entry.path().string());
    stmt.bind_text(2, entry.filename());
    stmt.bind_text(3, entry.extension());
    stmt.bind_int64(4, static_cast<std::int64_t>(entry.size_bytes()));
    stmt.bind_text(5, entry.mime_type());
    stmt.bind_int64(6, to_ms(entry.created_at()));
    stmt.bind_int64(7, to_ms(entry.modified_at()));
    stmt.bind_int64(8, to_ms(entry.scanned_at()));
    stmt.bind_int(9, static_cast<int>(entry.status()));

    if (entry.metadata().has_value()) {
        const auto& meta = entry.metadata().value();
        stmt.bind_text(10, meta.sha256);
        stmt.bind_optional_int(11, meta.image_width);
        stmt.bind_optional_int(12, meta.image_height);
        stmt.bind_optional_int(13, meta.page_count);
        stmt.bind_text(14, meta.text_preview);
    } else {
        stmt.bind_null(10);
        stmt.bind_null(11);
        stmt.bind_null(12);
        stmt.bind_null(13);
        stmt.bind_null(14);
    }

    int rc = stmt.step();
    if (rc != SQLITE_DONE) {
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kFileWriteError,
                        "Failed to save file: " + std::string(sqlite3_errmsg(db_))));
    }

    return core::Result<void, core::Error>::success();
}

core::Result<std::optional<core::FileEntry>, core::Error> SqliteDatabase::find_file(const core::FilePath& path) {
    if (!db_) {
        return core::Result<std::optional<core::FileEntry>, core::Error>::failure(
            core::Error(core::ErrorCode::kNotFound, "Database not open"));
    }

    const std::string sql = 
        "SELECT id, path, filename, extension, size_bytes, mime_type, "
        "created_at, modified_at, scanned_at, status, sha256, image_width, "
        "image_height, page_count, text_preview "
        "FROM files WHERE path = ?1;";

    core::Result<void, core::Error> prep_res = core::Result<void, core::Error>::success();
    SqliteStatement stmt(db_, sql, prep_res);
    if (prep_res.has_error()) {
        return core::Result<std::optional<core::FileEntry>, core::Error>::failure(std::move(prep_res).error());
    }

    stmt.bind_text(1, path.string());

    int rc = stmt.step();
    if (rc == SQLITE_ROW) {
        core::EntityId db_id = static_cast<core::EntityId>(stmt.column_int64(0));
        core::FilePath fpath(stmt.column_text(1));
        core::FileSize size = static_cast<core::FileSize>(stmt.column_int64(4));
        core::Timestamp created = to_timestamp(stmt.column_int64(6));
        core::Timestamp modified = to_timestamp(stmt.column_int64(7));
        core::Timestamp scanned = to_timestamp(stmt.column_int64(8));
        
        core::FileEntry entry(fpath, size, created, modified);
        entry.set_id(db_id);
        entry.set_mime_type(stmt.column_text(5));
        entry.set_status(static_cast<core::ProcessingStatus>(stmt.column_int(9)));

        // If metadata columns exist, rebuild metadata value object
        if (!stmt.column_is_null(10)) {
            core::FileMetadata meta;
            meta.sha256 = stmt.column_text(10);
            meta.image_width = stmt.column_optional_int(11);
            meta.image_height = stmt.column_optional_int(12);
            meta.page_count = stmt.column_optional_int(13);
            meta.text_preview = stmt.column_text(14);
            entry.set_metadata(std::move(meta));
        }

        return core::Result<std::optional<core::FileEntry>, core::Error>::success(std::move(entry));
    }

    return core::Result<std::optional<core::FileEntry>, core::Error>::success(std::nullopt);
}

core::Result<std::vector<core::FileEntry>, core::Error> SqliteDatabase::find_all_files() {
    if (!db_) {
        return core::Result<std::vector<core::FileEntry>, core::Error>::failure(
            core::Error(core::ErrorCode::kNotFound, "Database not open"));
    }

    const std::string sql = 
        "SELECT id, path, filename, extension, size_bytes, mime_type, "
        "created_at, modified_at, scanned_at, status, sha256, image_width, "
        "image_height, page_count, text_preview FROM files;";

    core::Result<void, core::Error> prep_res = core::Result<void, core::Error>::success();
    SqliteStatement stmt(db_, sql, prep_res);
    if (prep_res.has_error()) {
        return core::Result<std::vector<core::FileEntry>, core::Error>::failure(std::move(prep_res).error());
    }

    std::vector<core::FileEntry> results;
    while (stmt.step() == SQLITE_ROW) {
        core::EntityId db_id = static_cast<core::EntityId>(stmt.column_int64(0));
        core::FilePath fpath(stmt.column_text(1));
        core::FileSize size = static_cast<core::FileSize>(stmt.column_int64(4));
        core::Timestamp created = to_timestamp(stmt.column_int64(6));
        core::Timestamp modified = to_timestamp(stmt.column_int64(7));
        
        core::FileEntry entry(fpath, size, created, modified);
        entry.set_id(db_id);
        entry.set_mime_type(stmt.column_text(5));
        entry.set_status(static_cast<core::ProcessingStatus>(stmt.column_int(9)));

        if (!stmt.column_is_null(10)) {
            core::FileMetadata meta;
            meta.sha256 = stmt.column_text(10);
            meta.image_width = stmt.column_optional_int(11);
            meta.image_height = stmt.column_optional_int(12);
            meta.page_count = stmt.column_optional_int(13);
            meta.text_preview = stmt.column_text(14);
            entry.set_metadata(std::move(meta));
        }
        results.push_back(std::move(entry));
    }

    return core::Result<std::vector<core::FileEntry>, core::Error>::success(std::move(results));
}

core::Result<void, core::Error> SqliteDatabase::remove_file(const core::FilePath& path) {
    if (!db_) {
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kNotFound, "Database not open"));
    }

    const std::string sql = "DELETE FROM files WHERE path = ?1;";

    core::Result<void, core::Error> prep_res = core::Result<void, core::Error>::success();
    SqliteStatement stmt(db_, sql, prep_res);
    if (prep_res.has_error()) {
        return prep_res;
    }

    stmt.bind_text(1, path.string());

    int rc = stmt.step();
    if (rc != SQLITE_DONE) {
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kFileWriteError,
                        "Failed to delete file: " + std::string(sqlite3_errmsg(db_))));
    }

    return core::Result<void, core::Error>::success();
}

// ============================================================================
// Action Logs Persistence
// ============================================================================

core::Result<void, core::Error> SqliteDatabase::save_action(const core::FileActionRecord& record) {
    if (!db_) {
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kNotFound, "Database not open"));
    }

    const std::string sql = 
        "INSERT INTO action_history (transaction_id, original_path, executed_path, action_type, timestamp) "
        "VALUES (?1, ?2, ?3, ?4, ?5);";

    core::Result<void, core::Error> prep_res = core::Result<void, core::Error>::success();
    SqliteStatement stmt(db_, sql, prep_res);
    if (prep_res.has_error()) {
        return prep_res;
    }

    stmt.bind_text(1, record.transaction_id);
    stmt.bind_text(2, record.original_path.string());
    stmt.bind_text(3, record.executed_path.string());
    stmt.bind_int(4, static_cast<int>(record.action_type));
    
    std::int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        record.timestamp.time_since_epoch()).count();
    stmt.bind_int64(5, ms);

    int rc = stmt.step();
    if (rc != SQLITE_DONE) {
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kFileWriteError,
                        "Failed to save action: " + std::string(sqlite3_errmsg(db_))));
    }

    return core::Result<void, core::Error>::success();
}

core::Result<std::vector<core::FileActionRecord>, core::Error> SqliteDatabase::get_transaction_history(
    const std::string& transaction_id) {

    if (!db_) {
        return core::Result<std::vector<core::FileActionRecord>, core::Error>::failure(
            core::Error(core::ErrorCode::kNotFound, "Database not open"));
    }

    const std::string sql = 
        "SELECT transaction_id, original_path, executed_path, action_type, timestamp "
        "FROM action_history WHERE transaction_id = ?1 ORDER BY timestamp ASC;";

    core::Result<void, core::Error> prep_res = core::Result<void, core::Error>::success();
    SqliteStatement stmt(db_, sql, prep_res);
    if (prep_res.has_error()) {
        return core::Result<std::vector<core::FileActionRecord>, core::Error>::failure(std::move(prep_res).error());
    }

    stmt.bind_text(1, transaction_id);

    std::vector<core::FileActionRecord> history;
    while (stmt.step() == SQLITE_ROW) {
        core::FileActionRecord rec;
        rec.transaction_id = stmt.column_text(0);
        rec.original_path = core::FilePath(stmt.column_text(1));
        rec.executed_path = core::FilePath(stmt.column_text(2));
        rec.action_type = static_cast<core::FileActionType>(stmt.column_int(3));
        
        std::int64_t ms = stmt.column_int64(4);
        rec.timestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
        
        history.push_back(std::move(rec));
    }

    return core::Result<std::vector<core::FileActionRecord>, core::Error>::success(std::move(history));
}

core::Result<std::vector<core::FileActionRecord>, core::Error> SqliteDatabase::get_all_history() {
    if (!db_) {
        return core::Result<std::vector<core::FileActionRecord>, core::Error>::failure(
            core::Error(core::ErrorCode::kNotFound, "Database not open"));
    }

    const std::string sql = 
        "SELECT transaction_id, original_path, executed_path, action_type, timestamp "
        "FROM action_history ORDER BY timestamp ASC;";

    core::Result<void, core::Error> prep_res = core::Result<void, core::Error>::success();
    SqliteStatement stmt(db_, sql, prep_res);
    if (prep_res.has_error()) {
        return core::Result<std::vector<core::FileActionRecord>, core::Error>::failure(std::move(prep_res).error());
    }

    std::vector<core::FileActionRecord> history;
    while (stmt.step() == SQLITE_ROW) {
        core::FileActionRecord rec;
        rec.transaction_id = stmt.column_text(0);
        rec.original_path = core::FilePath(stmt.column_text(1));
        rec.executed_path = core::FilePath(stmt.column_text(2));
        rec.action_type = static_cast<core::FileActionType>(stmt.column_int(3));
        
        std::int64_t ms = stmt.column_int64(4);
        rec.timestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
        
        history.push_back(std::move(rec));
    }

    return core::Result<std::vector<core::FileActionRecord>, core::Error>::success(std::move(history));
}

core::Result<void, core::Error> SqliteDatabase::clear_history() {
    if (!db_) {
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kNotFound, "Database not open"));
    }

    const std::string sql = "DELETE FROM action_history;";

    core::Result<void, core::Error> prep_res = core::Result<void, core::Error>::success();
    SqliteStatement stmt(db_, sql, prep_res);
    if (prep_res.has_error()) {
        return prep_res;
    }

    int rc = stmt.step();
    if (rc != SQLITE_DONE) {
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kFileWriteError,
                        "Failed to clear history: " + std::string(sqlite3_errmsg(db_))));
    }

    return core::Result<void, core::Error>::success();
}

}  // namespace lilolify::infra
