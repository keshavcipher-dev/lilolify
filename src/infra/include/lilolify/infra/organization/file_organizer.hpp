// ============================================================================
// Lilolify — FileOrganizer
// ============================================================================
// Concrete implementation of IFileOrganizer using C++20 standard filesystem.
// ============================================================================

#pragma once

#include <lilolify/core/interfaces/i_file_organizer.hpp>

namespace lilolify::infra {

/// Implements physical file moves, copies, and symlinks with rollback safety.
class FileOrganizer : public core::IFileOrganizer {
public:
    FileOrganizer() noexcept = default;
    ~FileOrganizer() override = default;

    // Disable copy/move
    FileOrganizer(const FileOrganizer&) = delete;
    FileOrganizer& operator=(const FileOrganizer&) = delete;
    FileOrganizer(FileOrganizer&&) = delete;
    FileOrganizer& operator=(FileOrganizer&&) = delete;

    [[nodiscard]] core::Result<core::FileActionRecord, core::Error> execute(
        const core::FilePath& source,
        const core::FilePath& destination_base,
        const std::string& suggested_path,
        core::FileActionType type,
        core::CollisionStrategy collision_strategy,
        const std::string& transaction_id) override;

    [[nodiscard]] core::Result<void, core::Error> revert(
        const core::FileActionRecord& record) override;

private:
    [[nodiscard]] core::FilePath resolve_collision(
        const core::FilePath& target,
        core::CollisionStrategy strategy,
        core::Error& error_out);
};

}  // namespace lilolify::infra
