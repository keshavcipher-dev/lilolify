#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

// ============================================================================
// Lilolify — ConfigurationManager Implementation
// ============================================================================

#include <lilolify/app/configuration_manager.hpp>

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <sstream>
#include <iomanip>

using json = nlohmann::json;

namespace lilolify::app {

namespace {

// XOR key for obfuscation cipher
constexpr std::uint8_t kXorCipherMask = 0x5C;

// Helper to convert hex char to byte value
std::uint8_t hex_to_byte(char ch) noexcept {
    if (ch >= '0' && ch <= '9') return static_cast<std::uint8_t>(ch - '0');
    if (ch >= 'a' && ch <= 'f') return static_cast<std::uint8_t>(ch - 'a' + 10);
    if (ch >= 'A' && ch <= 'F') return static_cast<std::uint8_t>(ch - 'A' + 10);
    return 0;
}

}  // namespace

// ============================================================================
// JSON Serialization / Deserialization Mappings
// ============================================================================

static void to_json(json& j, const AiSettings& s) {
    j = json{
        {"active_provider", s.active_provider},
        {"api_key", s.api_key},
        {"model", s.model},
        {"base_url", s.base_url},
        {"temperature", s.temperature},
        {"max_tokens", s.max_tokens}
    };
}

static void from_json(const json& j, AiSettings& s) {
    s.active_provider = j.value("active_provider", "openai");
    s.api_key = j.value("api_key", "");
    s.model = j.value("model", "gpt-4o");
    s.base_url = j.value("base_url", "");
    s.temperature = j.value("temperature", 0.1);
    s.max_tokens = j.value("max_tokens", 1000u);
}

static void to_json(json& j, const ScanSettings& s) {
    j = json{
        {"recursive", s.recursive},
        {"max_depth", s.max_depth},
        {"max_file_size", s.max_file_size},
        {"min_file_size", s.min_file_size},
        {"include_extensions", s.include_extensions},
        {"exclude_extensions", s.exclude_extensions},
        {"exclude_directories", s.exclude_directories},
        {"follow_symlinks", s.follow_symlinks},
        {"detect_mime_type", s.detect_mime_type}
    };
}

static void from_json(const json& j, ScanSettings& s) {
    s.recursive = j.value("recursive", true);
    s.max_depth = j.value("max_depth", 50u);
    s.max_file_size = j.value("max_file_size", 100ULL * 1024 * 1024);
    s.min_file_size = j.value("min_file_size", 0ULL);
    
    if (j.contains("include_extensions")) {
        j.at("include_extensions").get_to(s.include_extensions);
    }
    if (j.contains("exclude_extensions")) {
        j.at("exclude_extensions").get_to(s.exclude_extensions);
    }
    if (j.contains("exclude_directories")) {
        j.at("exclude_directories").get_to(s.exclude_directories);
    }
    
    s.follow_symlinks = j.value("follow_symlinks", false);
    s.detect_mime_type = j.value("detect_mime_type", true);
}

static void to_json(json& j, const OrganizationSettings& s) {
    j = json{
        {"destination_base", s.destination_base},
        {"action_type", static_cast<int>(s.action_type)},
        {"collision_strategy", static_cast<int>(s.collision_strategy)},
        {"category_folders", s.category_folders}
    };
}

static void from_json(const json& j, OrganizationSettings& s) {
    s.destination_base = j.value("destination_base", "");
    s.action_type = static_cast<core::FileActionType>(j.value("action_type", 0));
    s.collision_strategy = static_cast<core::CollisionStrategy>(j.value("collision_strategy", 1));
    
    if (j.contains("category_folders")) {
        j.at("category_folders").get_to(s.category_folders);
    }
}

static void to_json(json& j, const AppSettings& s) {
    j = json{
        {"ai", s.ai},
        {"scan", s.scan},
        {"organization", s.organization},
        {"database_path", s.database_path}
    };
}

static void from_json(const json& j, AppSettings& s) {
    if (j.contains("ai")) j.at("ai").get_to(s.ai);
    if (j.contains("scan")) j.at("scan").get_to(s.scan);
    if (j.contains("organization")) j.at("organization").get_to(s.organization);
    s.database_path = j.value("database_path", "");
}

// ============================================================================
// Configuration File IO Loader
// ============================================================================

core::Result<AppSettings, core::Error> ConfigurationManager::load(const std::string& file_path) {
    std::filesystem::path path(file_path);
    AppSettings settings;

    // Resolve default database and organization targets if path does not exist
    settings.database_path = get_default_database_path();
    
    if (!std::filesystem::exists(path)) {
        // Return default initialized settings if file is missing
        return core::Result<AppSettings, core::Error>::success(settings);
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return core::Result<AppSettings, core::Error>::failure(
                core::Error(core::ErrorCode::kFileAccessDenied, "Failed to open settings file: " + file_path));
        }

        json j;
        file >> j;
        
        // Parse settings structure from json
        AppSettings parsed = j.get<AppSettings>();
        
        // Ensure database path fallback is mapped
        if (parsed.database_path.empty()) {
            parsed.database_path = settings.database_path;
        }

        return core::Result<AppSettings, core::Error>::success(std::move(parsed));

    } catch (const std::exception& ex) {
        return core::Result<AppSettings, core::Error>::failure(
            core::Error(core::ErrorCode::kFileReadError, "JSON parsing failed on config: " + std::string(ex.what())));
    }
}

core::Result<void, core::Error> ConfigurationManager::save(
    const std::string& file_path,
    const AppSettings& settings) {

    std::filesystem::path path(file_path);

    try {
        // Create nested folders if missing
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream file(path);
        if (!file.is_open()) {
            return core::Result<void, core::Error>::failure(
                core::Error(core::ErrorCode::kFileAccessDenied, "Failed to open settings file for writing: " + file_path));
        }

        json j = settings;
        file << j.dump(4); // Format with pretty-print 4 spaces indent

        return core::Result<void, core::Error>::success();

    } catch (const std::exception& ex) {
        return core::Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kFileWriteError, "JSON serialization writing failed: " + std::string(ex.what())));
    }
}

// ============================================================================
// Secure XOR Key Obfuscation
// ============================================================================

std::string ConfigurationManager::obfuscate_key(const std::string& plain_key) const {
    if (plain_key.empty()) return "";

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    for (char ch : plain_key) {
        std::uint8_t cipher = static_cast<std::uint8_t>(ch) ^ kXorCipherMask;
        ss << std::setw(2) << static_cast<int>(cipher);
    }
    
    return ss.str();
}

std::string ConfigurationManager::deobfuscate_key(const std::string& secure_key) const {
    if (secure_key.empty() || secure_key.length() % 2 != 0) return "";

    std::string plain;
    plain.reserve(secure_key.length() / 2);

    for (std::size_t i = 0; i < secure_key.length(); i += 2) {
        std::uint8_t byte = (hex_to_byte(secure_key[i]) << 4) | hex_to_byte(secure_key[i + 1]);
        plain.push_back(static_cast<char>(byte ^ kXorCipherMask));
    }

    return plain;
}

// ============================================================================
// Platform-Specific Path Resolvers
// ============================================================================

std::string ConfigurationManager::get_default_config_dir() const {
    std::filesystem::path config_path;

#if defined(_WIN32) || defined(_WIN64)
    const char* appdata = std::getenv("APPDATA");
    if (appdata) {
        config_path = std::filesystem::path(appdata) / "Lilolify";
    } else {
        config_path = std::filesystem::current_path() / ".config" / "lilolify";
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        config_path = std::filesystem::path(home) / ".config" / "lilolify";
    } else {
        config_path = std::filesystem::current_path() / ".config" / "lilolify";
    }
#endif

    return config_path.string();
}

std::string ConfigurationManager::get_default_settings_path() const {
    return (std::filesystem::path(get_default_config_dir()) / "settings.json").string();
}

std::string ConfigurationManager::get_default_database_path() const {
    return (std::filesystem::path(get_default_config_dir()) / "lilolify.db").string();
}

}  // namespace lilolify::app
