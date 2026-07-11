// ============================================================================
// Lilolify — ConfigurationManager
// ============================================================================
// Resolves platform-specific configuration storage paths, performs
// JSON serialization, and secures API keys via obfuscation ciphers.
// ============================================================================

#pragma once

#include <lilolify/app/app_settings.hpp>
#include <lilolify/core/error.hpp>
#include <lilolify/core/result.hpp>

#include <string>

namespace lilolify::app {

/// Manages loading, saving, and securing application-wide configuration settings.
class ConfigurationManager {
public:
    ConfigurationManager() noexcept = default;
    ~ConfigurationManager() = default;

    // Disable copy/move
    ConfigurationManager(const ConfigurationManager&) = delete;
    ConfigurationManager& operator=(const ConfigurationManager&) = delete;
    ConfigurationManager(ConfigurationManager&&) = delete;
    ConfigurationManager& operator=(ConfigurationManager&&) = delete;

    /// Load settings from the specified file path.
    /// If the file does not exist, it initializes settings with default values.
    ///
    /// @param file_path  Absolute path to the settings JSON file.
    /// @return AppSettings on success, or Error on read/parse failure.
    [[nodiscard]] core::Result<AppSettings, core::Error> load(const std::string& file_path);

    /// Save settings to the specified file path.
    /// Creates parent folders automatically if they do not exist.
    ///
    /// @param file_path  Absolute path to the settings JSON file.
    /// @param settings   AppSettings structure to serialize.
    /// @return Success, or Error on write failure.
    [[nodiscard]] core::Result<void, core::Error> save(
        const std::string& file_path,
        const AppSettings& settings);

    /// Securely obfuscates an API key so it is not stored in plain text.
    [[nodiscard]] std::string obfuscate_key(const std::string& plain_key) const;

    /// Reverts an obfuscated key back to its plain text format.
    [[nodiscard]] std::string deobfuscate_key(const std::string& secure_key) const;

    /// Resolve default standard storage directory for Lilolify files.
    /// e.g. %APPDATA%\Lilolify on Windows, ~/.config/lilolify on Linux/macOS.
    [[nodiscard]] std::string get_default_config_dir() const;

    /// Get default settings.json path.
    [[nodiscard]] std::string get_default_settings_path() const;

    /// Get default database.db path.
    [[nodiscard]] std::string get_default_database_path() const;
};

}  // namespace lilolify::app
