// ============================================================================
// Lilolify — FileOrganizer Implementation
// ============================================================================

#include <lilolify/infra/organization/file_organizer.hpp>

#include <filesystem>
#include <system_error>

namespace lilolify::infra {

// ============================================================================
// Core Execution Pipeline
// ============================================================================

core::Result<core::FileActionRecord, core::Error> FileOrganizer::execute(
    const core::FilePath& source,
    const core::FilePath& destination_base,
    const std::string& suggested_path,
    core::FileActionType type,
    core::CollisionStrategy collision_strategy,
    const std::string& transaction_id) {

    // 1. Validate that the source file physically exists
    if (!std::filesystem::exists(source)) {
        return core::Result<core::FileActionRecord, core::Error>::failure(
            core::Error(core::ErrorCode::kFileNotFound, "Source file not found: " + source.string()));
    }

    // 2. Resolve destination path base + suggested path
    core::FilePath resolved_target = destination_base / suggested_path;

    // 3. Apply collision resolution strategy
    core::Error collision_err(core::ErrorCode::kUnknown, "");
    core::FilePath final_target = resolve_collision(resolved_target, collision_strategy, collision_err);
    if (final_target.empty()) {
        return core::Result<core::FileActionRecord, core::Error>::failure(std::move(collision_err));
    }

    std::error_code ec;

    // 4. Ensure target parent directory exists
    std::filesystem::create_directories(final_target.parent_path(), ec);
    if (ec) {
        return core::Result<core::FileActionRecord, core::Error>::failure(
            core::Error(core::ErrorCode::kDirectoryCreateError,
                        "Failed to create target directories: " + ec.message()));
    }

    // 5. Execute action based on requested type
    switch (type) {
        case core::FileActionType::kMove: {
            std::filesystem::rename(source, final_target, ec);
            if (ec) {
                // Fallback copy + delete (standard workaround for cross-volume moves)
                std::filesystem::copy_file(source, final_target,
                                           std::filesystem::copy_options::overwrite_existing, ec);
                if (ec) {
                    return core::Result<core::FileActionRecord, core::Error>::failure(
                        core::Error(core::ErrorCode::kFileMoveError, "Cross-volume move copy failed: " + ec.message()));
                }
                std::filesystem::remove(source, ec);
                if (ec) {
                    return core::Result<core::FileActionRecord, core::Error>::failure(
                        core::Error(core::ErrorCode::kFileMoveError, "Cross-volume move delete failed: " + ec.message()));
                }
            }
            break;
        }

        case core::FileActionType::kCopy: {
            std::filesystem::copy_file(source, final_target,
                                       std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                return core::Result<core::FileActionRecord, core::Error>::failure(
                    core::Error(core::ErrorCode::kFileWriteError, "Copy execution failed: " + ec.message()));
            }
            break;
        }

        case core::FileActionType::kHardLink: {
            // Remove existing if strategy was overwrite
            if (collision_strategy == core::CollisionStrategy::kOverwrite && std::filesystem::exists(final_target)) {
                std::filesystem::remove(final_target, ec);
            }
            std::filesystem::create_hard_link(source, final_target, ec);
            if (ec) {
                return core::Result<core::FileActionRecord, core::Error>::failure(
                    core::Error(core::ErrorCode::kFileWriteError, "Hardlink execution failed: " + ec.message()));
            }
            break;
        }

        case core::FileActionType::kSymLink: {
            // Remove existing if strategy was overwrite
            if (collision_strategy == core::CollisionStrategy::kOverwrite && std::filesystem::exists(final_target)) {
                std::filesystem::remove(final_target, ec);
            }
            std::filesystem::create_symlink(source, final_target, ec);
            if (ec) {
                return core::Result<core::FileActionRecord, core::Error>::failure(
                    core::Error(core::ErrorCode::kFileWriteError, "Symlink execution failed: " + ec.message()));
            }
            break;
        }
    }

    // 6. Return transaction action record ledger
    core::FileActionRecord record;
    record.transaction_id = transaction_id;
    record.original_path = source;
    record.executed_path = final_target;
    record.action_type = type;
    record.timestamp = std::chrono::system_clock::now();

    return core::Result<core::FileActionRecord, core::Error>::success(std::move(record));
}

// ============================================================================
// Action Reversion (Undo)
// ============================================================================

core::Result<void, core::Error> FileOrganizer::revert(const core::FileActionRecord& record) {
    std::error_code ec;

    // Verify file to revert still exists
    if (!std::filesystem::exists(record.executed_path)) {
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kFileNotFound,
                        "Revert source not found: " + record.executed_path.string()));
    }

    // Recreate parent directories of original path if missing
    std::filesystem::create_directories(record.original_path.parent_path(), ec);
    if (ec) {
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kDirectoryCreateError,
                        "Revert directory creation failed: " + ec.message()));
    }

    switch (record.action_type) {
        case core::FileActionType::kMove: {
            std::filesystem::rename(record.executed_path, record.original_path, ec);
            if (ec) {
                // Fallback copy + delete (cross-volume)
                std::filesystem::copy_file(record.executed_path, record.original_path,
                                           std::filesystem::copy_options::overwrite_existing, ec);
                if (ec) {
                    return core::Result<void, core::Error>::failure(
                        core::Error(core::ErrorCode::kFileMoveError, "Revert cross-volume move copy failed: " + ec.message()));
                }
                std::filesystem::remove(record.executed_path, ec);
                if (ec) {
                    return core::Result<void, core::Error>::failure(
                        core::Error(core::ErrorCode::kFileMoveError, "Revert cross-volume move delete failed: " + ec.message()));
                }
            }
            break;
        }

        case core::FileActionType::kCopy:
        case core::FileActionType::kHardLink:
        case core::FileActionType::kSymLink: {
            // Reverting copies/links simply removes the created artifact
            std::filesystem::remove(record.executed_path, ec);
            if (ec) {
                return core::Result<void, core::Error>::failure(
                    core::Error(core::ErrorCode::kFileMoveError, "Revert delete failed: " + ec.message()));
            }
            break;
        }
    }

    return core::Result<void, core::Error>::success();
}

// ============================================================================
// Collision Handler
// ============================================================================

core::FilePath FileOrganizer::resolve_collision(
    const core::FilePath& target,
    core::CollisionStrategy strategy,
    core::Error& error_out) {

    if (!std::filesystem::exists(target)) {
        return target;
    }

    if (strategy == core::CollisionStrategy::kSkip) {
        error_out = core::Error(core::ErrorCode::kFileAlreadyExists,
                                "Destination file already exists: " + target.string());
        return "";
    }

    if (strategy == core::CollisionStrategy::kOverwrite) {
        return target;
    }

    if (strategy == core::CollisionStrategy::kRename) {
        auto parent = target.parent_path();
        auto stem = target.stem().string();
        auto ext = target.extension().string();

        int suffix = 1;
        while (true) {
            core::FilePath candidate = parent / (stem + "_" + std::to_string(suffix) + ext);
            if (!std::filesystem::exists(candidate)) {
                return candidate;
            }
            suffix++;
        }
    }

    error_out = core::Error(core::ErrorCode::kInvalidArgument, "Unsupported collision strategy");
    return "";
}

}  // namespace lilolify::infra
