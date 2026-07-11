// ============================================================================
// Lilolify — Common Types
// ============================================================================
// Central type definitions, aliases, and enumerations used across the entire
// domain layer. This header establishes the vocabulary of the system.
//
// Design Decision:
//   Strong type aliases and enums live here so that every module shares a
//   single, unambiguous vocabulary. This prevents the "stringly typed" anti-
//   pattern where file paths, IDs, and categories are all just std::string.
// ============================================================================

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lilolify::core {

// ============================================================================
// Type Aliases
// ============================================================================

/// Unique identifier for entities in the system.
/// Using int64_t for SQLite compatibility (INTEGER PRIMARY KEY).
using EntityId = std::int64_t;

/// Sentinel value indicating an entity has not been persisted yet.
inline constexpr EntityId kInvalidEntityId = -1;

/// File system path type — std::filesystem::path throughout the project.
using FilePath = std::filesystem::path;

/// Timestamp type — steady_clock for durations, system_clock for wall time.
using Timestamp = std::chrono::system_clock::time_point;

/// File size in bytes.
using FileSize = std::uintmax_t;

/// Confidence score from AI analysis (0–100).
/// Wrapped in a value object in confidence.hpp for validation;
/// this alias is for raw storage/transport.
using ConfidenceScore = std::uint8_t;

// ============================================================================
// Enumerations
// ============================================================================

/// Supported AI providers.
/// New providers are added here AND in the ProviderFactory (Phase 5).
enum class AiProvider : std::uint8_t {
    kOpenAI,   ///< OpenAI GPT-4 Vision
    kGemini,   ///< Google Gemini Vision
    kClaude,   ///< Anthropic Claude Vision
    kLocalAI,  ///< Future local model support
    kMock,     ///< Mock provider for testing
};

/// File processing status through the AI pipeline.
enum class ProcessingStatus : std::uint8_t {
    kPending,     ///< Queued for processing
    kScanning,    ///< Metadata extraction in progress
    kAnalyzing,   ///< AI vision analysis in progress
    kDeciding,    ///< Decision engine determining folder
    kMoving,      ///< File move in progress
    kCompleted,   ///< Successfully organized
    kFailed,      ///< Processing failed (see error)
    kSkipped,     ///< Skipped by user or filter
    kCancelled,   ///< Cancelled by user
};

/// Categories for organized folders (top-level).
/// These map to the root-level folders in the organization hierarchy.
enum class FileCategory : std::uint8_t {
    kDocuments,
    kFinance,
    kHealth,
    kEducation,
    kProgramming,
    kPersonal,
    kTravel,
    kFood,
    kShopping,
    kNature,
    kBusinessCards,
    kQRCodes,
    kWallpapers,
    kMemes,
    kDownloads,
    kTemporary,
    kUnknown,
};

/// Severity levels for operations and logging within the domain.
enum class Severity : std::uint8_t {
    kInfo,
    kWarning,
    kError,
    kCritical,
};

/// Type of operation for the undo/redo system.
enum class OperationType : std::uint8_t {
    kFileMove,     ///< File was moved to a new location
    kFileRename,   ///< File was renamed
    kFolderCreate, ///< New folder was created
    kFolderDelete, ///< Folder was deleted
    kFileCopy,     ///< File was copied
    kFileDelete,   ///< File was deleted
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Convert AiProvider enum to human-readable string.
[[nodiscard]] constexpr std::string_view to_string(AiProvider provider) noexcept {
    switch (provider) {
        case AiProvider::kOpenAI:  return "OpenAI";
        case AiProvider::kGemini:  return "Google Gemini";
        case AiProvider::kClaude:  return "Anthropic Claude";
        case AiProvider::kLocalAI: return "Local AI";
        case AiProvider::kMock:    return "Mock (Testing)";
    }
    return "Unknown";
}

/// Convert ProcessingStatus enum to human-readable string.
[[nodiscard]] constexpr std::string_view to_string(ProcessingStatus status) noexcept {
    switch (status) {
        case ProcessingStatus::kPending:   return "Pending";
        case ProcessingStatus::kScanning:  return "Scanning";
        case ProcessingStatus::kAnalyzing: return "Analyzing";
        case ProcessingStatus::kDeciding:  return "Deciding";
        case ProcessingStatus::kMoving:    return "Moving";
        case ProcessingStatus::kCompleted: return "Completed";
        case ProcessingStatus::kFailed:    return "Failed";
        case ProcessingStatus::kSkipped:   return "Skipped";
        case ProcessingStatus::kCancelled: return "Cancelled";
    }
    return "Unknown";
}

/// Convert FileCategory enum to human-readable string.
[[nodiscard]] constexpr std::string_view to_string(FileCategory category) noexcept {
    switch (category) {
        case FileCategory::kDocuments:     return "Documents";
        case FileCategory::kFinance:       return "Finance";
        case FileCategory::kHealth:        return "Health";
        case FileCategory::kEducation:     return "Education";
        case FileCategory::kProgramming:   return "Programming";
        case FileCategory::kPersonal:      return "Personal";
        case FileCategory::kTravel:        return "Travel";
        case FileCategory::kFood:          return "Food";
        case FileCategory::kShopping:      return "Shopping";
        case FileCategory::kNature:        return "Nature";
        case FileCategory::kBusinessCards: return "Business Cards";
        case FileCategory::kQRCodes:       return "QR Codes";
        case FileCategory::kWallpapers:    return "Wallpapers";
        case FileCategory::kMemes:         return "Memes";
        case FileCategory::kDownloads:     return "Downloads";
        case FileCategory::kTemporary:     return "Temporary";
        case FileCategory::kUnknown:       return "Unknown";
    }
    return "Unknown";
}

/// Convert OperationType enum to human-readable string.
[[nodiscard]] constexpr std::string_view to_string(OperationType op) noexcept {
    switch (op) {
        case OperationType::kFileMove:     return "File Move";
        case OperationType::kFileRename:   return "File Rename";
        case OperationType::kFolderCreate: return "Folder Create";
        case OperationType::kFolderDelete: return "Folder Delete";
        case OperationType::kFileCopy:     return "File Copy";
        case OperationType::kFileDelete:   return "File Delete";
    }
    return "Unknown";
}

}  // namespace lilolify::core
