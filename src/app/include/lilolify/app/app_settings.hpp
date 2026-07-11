// ============================================================================
// Lilolify — AppSettings
// ============================================================================
// Strongly-typed settings structs representing configurations for AI reasoning,
// scanning parameters, organization targets, and caching databases.
// ============================================================================

#pragma once

#include <lilolify/core/types.hpp>
#include <lilolify/core/value_objects/file_action_record.hpp>

#include <string>
#include <vector>
#include <map>

namespace lilolify::app {

/// Configuration for AI Providers and query limits.
struct AiSettings {
    std::string active_provider = "openai"; // "openai", "gemini", "claude", "custom"
    std::string api_key;                    // API Key (encoded/obfuscated)
    std::string model = "gpt-4o";           // Model identifier
    std::string base_url;                   // Custom endpoint base URL
    double temperature = 0.1;
    std::uint32_t max_tokens = 1000;
};

/// Configuration for directory scanning filters.
struct ScanSettings {
    bool recursive = true;
    std::uint32_t max_depth = 50;
    core::FileSize max_file_size = 100ULL * 1024 * 1024; // 100 MB
    core::FileSize min_file_size = 0;
    std::vector<std::string> include_extensions;
    std::vector<std::string> exclude_extensions = {".tmp", ".bak", ".log", ".lnk"};
    std::vector<std::string> exclude_directories;
    bool follow_symlinks = false;
    bool detect_mime_type = true;
};

/// Configuration for file reorganization behaviors.
struct OrganizationSettings {
    std::string destination_base;
    core::FileActionType action_type = core::FileActionType::kMove;
    core::CollisionStrategy collision_strategy = core::CollisionStrategy::kRename;
    
    // Mapping of category categories to custom relative subfolders.
    // e.g. "Invoices" -> "Finance/Invoices"
    std::map<std::string, std::string> category_folders;
};

/// Master application settings object.
struct AppSettings {
    AiSettings ai;
    ScanSettings scan;
    OrganizationSettings organization;
    std::string database_path;
};

}  // namespace lilolify::app
