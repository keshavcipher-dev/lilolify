// ============================================================================
// Lilolify — IFileOrganizer Interface
// ============================================================================
// Abstract port contract for filesystem reorganization and undo ledger reversion.
// ============================================================================

#pragma once

#include <lilolify/core/types.hpp>
#include <lilolify/core/error.hpp>
#include <lilolify/core/result.hpp>
#include <lilolify/core/value_objects/file_action_record.hpp>

namespace lilolify::core {

/// Interface for executing and reverting filesystem actions.
class IFileOrganizer {
public:
    virtual ~IFileOrganizer() = default;

    /// Execute a file organization directive on disk.
    ///
    /// @param source             The original file path.
    /// @param destination_base   The base root path to organize files into.
    /// @param suggested_path     The target relative path suggested (e.g. "Invoices/file.pdf").
    /// @param type               Type of action (Move, Copy, Link).
    /// @param collision_strategy Strategy to apply if destination exists.
    /// @param transaction_id     Session transaction ID for batch grouping.
    /// @return The executed action record ledger on success, or Error.
    [[nodiscard]] virtual Result<FileActionRecord, Error> execute(
        const FilePath& source,
        const FilePath& destination_base,
        const std::string& suggested_path,
        FileActionType type,
        CollisionStrategy collision_strategy,
        const std::string& transaction_id) = 0;

    /// Revert a previously executed action record.
    ///
    /// @param record The action record to roll back.
    /// @return Success or Error.
    [[nodiscard]] virtual Result<void, Error> revert(
        const FileActionRecord& record) = 0;
};

}  // namespace lilolify::core
