// ============================================================================
// Lilolify — Unit Tests: ConfigurationManager
// ============================================================================

#include <lilolify/app/configuration_manager.hpp>

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

namespace lilolify::app::test {

class ConfigurationManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Construct unique sandboxed config test path
        test_path_ = std::filesystem::current_path() / "test_lilolify_settings.json";
        std::filesystem::remove(test_path_);
    }

    void TearDown() override {
        std::filesystem::remove(test_path_);
    }

    std::filesystem::path test_path_;
    ConfigurationManager manager_;
};

// ============================================================================
// Test Cases
// ============================================================================

TEST_F(ConfigurationManagerTest, LoadReturnsDefaultsIfFileMissing) {
    auto load_res = manager_.load(test_path_.string());
    ASSERT_TRUE(load_res.has_value());

    const auto& settings = load_res.value();
    EXPECT_EQ(settings.ai.active_provider, "openai");
    EXPECT_EQ(settings.ai.model, "gpt-4o");
    EXPECT_EQ(settings.ai.temperature, 0.1);
    EXPECT_TRUE(settings.scan.recursive);
    EXPECT_EQ(settings.scan.exclude_extensions.size(), 4u);
    EXPECT_FALSE(settings.database_path.empty());
}

TEST_F(ConfigurationManagerTest, SaveAndLoadSerializationRoundtrip) {
    AppSettings settings;
    settings.ai.active_provider = "gemini";
    settings.ai.model = "gemini-1.5-pro";
    settings.ai.temperature = 0.5;
    settings.ai.max_tokens = 500;
    settings.ai.api_key = manager_.obfuscate_key("gemini-test-secret-key-12345");

    settings.scan.recursive = false;
    settings.scan.max_depth = 12;
    settings.scan.include_extensions = {".jpg", ".png"};
    settings.scan.exclude_extensions = {".tmp"};

    settings.organization.destination_base = "C:/user/organize_dest";
    settings.organization.action_type = core::FileActionType::kCopy;
    settings.organization.collision_strategy = core::CollisionStrategy::kOverwrite;
    settings.organization.category_folders["Invoices"] = "Billing/Receipts";

    settings.database_path = "C:/user/custom_lilolify.db";

    // 1. Save settings to disk
    auto save_res = manager_.save(test_path_.string(), settings);
    ASSERT_TRUE(save_res.has_value());
    EXPECT_TRUE(std::filesystem::exists(test_path_));

    // 2. Load settings back and assert equality
    auto load_res = manager_.load(test_path_.string());
    ASSERT_TRUE(load_res.has_value());

    const auto& loaded = load_res.value();
    EXPECT_EQ(loaded.ai.active_provider, "gemini");
    EXPECT_EQ(loaded.ai.model, "gemini-1.5-pro");
    EXPECT_EQ(loaded.ai.temperature, 0.5);
    EXPECT_EQ(loaded.ai.max_tokens, 500u);
    
    // Test key de-obfuscation matches original plain text key
    std::string decrypted_key = manager_.deobfuscate_key(loaded.ai.api_key);
    EXPECT_EQ(decrypted_key, "gemini-test-secret-key-12345");

    EXPECT_FALSE(loaded.scan.recursive);
    EXPECT_EQ(loaded.scan.max_depth, 12u);
    EXPECT_EQ(loaded.scan.include_extensions, std::vector<std::string>({".jpg", ".png"}));
    EXPECT_EQ(loaded.scan.exclude_extensions, std::vector<std::string>({".tmp"}));

    EXPECT_EQ(loaded.organization.destination_base, "C:/user/organize_dest");
    EXPECT_EQ(loaded.organization.action_type, core::FileActionType::kCopy);
    EXPECT_EQ(loaded.organization.collision_strategy, core::CollisionStrategy::kOverwrite);
    
    ASSERT_TRUE(loaded.organization.category_folders.contains("Invoices"));
    EXPECT_EQ(loaded.organization.category_folders.at("Invoices"), "Billing/Receipts");

    EXPECT_EQ(loaded.database_path, "C:/user/custom_lilolify.db");
}

TEST_F(ConfigurationManagerTest, KeyObfuscationReversibility) {
    std::string plain = "sk-proj-xyz_1234567890abcdefghijklmnopqrstuvwxyz";
    
    std::string secured = manager_.obfuscate_key(plain);
    EXPECT_NE(plain, secured);
    EXPECT_FALSE(secured.empty());

    std::string restored = manager_.deobfuscate_key(secured);
    EXPECT_EQ(plain, restored);
}

TEST_F(ConfigurationManagerTest, LoadFailsOnCorruptJsonFile) {
    // Write invalid corrupt json formatting to settings file path
    {
        std::ofstream file(test_path_);
        file << "{ \"ai\": { \"active_provider\": \"openai\", "; // Missing trailing closing brackets
    }

    auto load_res = manager_.load(test_path_.string());
    EXPECT_TRUE(load_res.has_error());
}

}  // namespace lilolify::app::test
