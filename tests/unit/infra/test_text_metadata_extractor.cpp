// ============================================================================
// Lilolify — Unit Tests: TextMetadataExtractor
// ============================================================================

#include <lilolify/infra/metadata/text_metadata_extractor.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace lilolify::infra::test {

namespace fs = std::filesystem;

class TextMetadataExtractorTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "lilolify_txt_test";
        fs::create_directories(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(test_dir_, ec);
    }

    fs::path create_file(const std::string& name, const std::string& content) {
        auto path = test_dir_ / name;
        std::ofstream file(path, std::ios::binary);
        file << content;
        return path;
    }

    fs::path create_binary_file(const std::string& name, const std::vector<std::uint8_t>& bytes) {
        auto path = test_dir_ / name;
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        return path;
    }

    TextMetadataExtractor extractor_;
    fs::path test_dir_;
};

// ============================================================================
// Text Preview Extraction Tests
// ============================================================================

TEST_F(TextMetadataExtractorTest, ParseEmptyTextFile) {
    auto path = create_file("empty.txt", "");
    auto result = extractor_.extract(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().text_preview.empty());
}

TEST_F(TextMetadataExtractorTest, ParseValidAsciiText) {
    std::string text = "Hello, this is a plain text file for unit testing metadata extraction.";
    auto path = create_file("ascii.txt", text);
    auto result = extractor_.extract(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().text_preview, text);
}

TEST_F(TextMetadataExtractorTest, ParseValidUtf8Text) {
    // Contains UTF-8 characters (e.g. smileys, non-ascii symbols)
    std::string text = "Hello 🌍! Unicode test: €¢£. Standard text here.";
    auto path = create_file("utf8.txt", text);
    auto result = extractor_.extract(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().text_preview, text);
}

TEST_F(TextMetadataExtractorTest, CleanInvalidUtf8Sequences) {
    // 0xFF and 0xC3 (unpaired lead) are invalid in UTF-8
    std::vector<std::uint8_t> bytes = {
        'H', 'e', 'l', 'l', 'o', ' ',
        0xFF,                             // Invalid byte
        'w', 'o', 'r', 'l', 'd', ' ',
        0xC3, 0x20,                       // Invalid UTF-8 sequence (lead followed by ASCII space)
        '!'
    };

    auto path = create_binary_file("invalid_utf8.txt", bytes);
    auto result = extractor_.extract(path);

    ASSERT_TRUE(result.has_value());
    // Should clean and replace invalid bytes with '?'
    EXPECT_EQ(result.value().text_preview, "Hello ?world ? !");
}

TEST_F(TextMetadataExtractorTest, LimitsSnippetTo1000Chars) {
    std::string text(1500, 'x');
    auto path = create_file("long.txt", text);
    auto result = extractor_.extract(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().text_preview.size(), 1000u);
    EXPECT_EQ(result.value().text_preview, std::string(1000, 'x'));
}

// ============================================================================
// Binary File Rejection Tests
// ============================================================================

TEST_F(TextMetadataExtractorTest, RejectsBinaryFileWithNullBytes) {
    // Looks like text, but contains a null byte at index 5
    std::vector<std::uint8_t> bytes = {'H', 'e', 'l', 'l', 'o', 0x00, 'w', 'o', 'r', 'l', 'd'};
    auto path = create_binary_file("binary_null.dat", bytes);
    auto result = extractor_.extract(path);

    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), core::ErrorCode::kOcrProcessingError);
}

TEST_F(TextMetadataExtractorTest, RejectsBinaryFileWithRawControlBytes) {
    // Contains raw control character 0x07 (BEL / Alarm), which is non-text
    std::vector<std::uint8_t> bytes = {'T', 'e', 's', 't', 0x07, 'd', 'a', 't', 'a'};
    auto path = create_binary_file("binary_ctrl.dat", bytes);
    auto result = extractor_.extract(path);

    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), core::ErrorCode::kOcrProcessingError);
}

}  // namespace lilolify::infra::test
