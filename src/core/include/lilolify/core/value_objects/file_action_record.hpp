// ============================================================================
// Lilolify — FileActionRecord
// ============================================================================
// Data structures representing filesystem execution directives and journals.
// ============================================================================

#pragma once

#include <lilolify/core/types.hpp>

#include <string>
#include <chrono>

namespace lilolify::core {

/// Types of filesystem operations supported by the organization engine.
enum class FileActionType {
    kMove,      /// Copy the file to destination, then delete original.
    kCopy,      /// Copy the file to destination, keeping original.
    kHardLink,  /// Create a hard link pointing to original file.
    kSymLink    /// Create a symbolic link pointing to original file.
};

/// Resolution strategy when a target destination file already exists.
enum class CollisionStrategy {
    kSkip,      /// Abort execution of this file (raise skip status).
    kRename,    /// Generate a numbered suffix (e.g. file_1.ext, file_2.ext).
    kOverwrite  /// Overwrite the existing file at destination.
};

/// Recorded ledger transaction of an executed file operation.
/// Used for batch auditing, failure recovery, and undo/redo operations.
struct FileActionRecord {
    /// Unique identifier for this transaction session/batch.
    std::string transaction_id;

    /// Original source location of the file before organizing.
    FilePath original_path;

    /// Final target location where the file was executed.
    /// This may differ from standard recommendations due to collision renames.
    FilePath executed_path;

    /// The type of action performed.
    FileActionType action_type = FileActionType::kMove;

    /// When the transaction was executed.
    std::chrono::system_clock::time_point timestamp;
};

}  // namespace lilolify::core
