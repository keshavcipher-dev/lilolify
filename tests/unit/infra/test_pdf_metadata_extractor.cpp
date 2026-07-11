// ============================================================================
// Lilolify — Unit Tests: PdfMetadataExtractor
// ============================================================================

#include <lilolify/infra/metadata/pdf_metadata_extractor.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace lilolify::infra::test {

namespace fs = std::filesystem;

class PdfMetadataExtractorTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "lilolify_pdf_test";
        fs::create_directories(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(test_dir_, ec);
    }

    fs::path create_pdf_file(const std::string& name, const std::string& content) {
        auto path = test_dir_ / name;
        std::ofstream file(path, std::ios::binary);
        file << "%PDF-1.4\n";
        file << content;
        file << "\n%%EOF";
        return path;
    }

    PdfMetadataExtractor extractor_;
    fs::path test_dir_;
};

// ============================================================================
// PDF Page Parsing Tests
// ============================================================================

TEST_F(PdfMetadataExtractorTest, ParseValidPagesTree) {
    std::string content =
        "3 0 obj\n"
        "<</Type /Pages\n"
        "/Kids [4 0 R]\n"
        "/Count 12\n"
        ">> \n"
        "endobj\n";

    auto path = create_pdf_file("test1.pdf", content);
    auto result = extractor_.extract(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().page_count, 12u);
}

TEST_F(PdfMetadataExtractorTest, ParsePagesTreeNoSpaces) {
    std::string content =
        "3 0 obj\n"
        "<</Type/Pages/Count 42>>\n"
        "endobj\n";

    auto path = create_pdf_file("test2.pdf", content);
    auto result = extractor_.extract(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().page_count, 42u);
}

TEST_F(PdfMetadataExtractorTest, ParsePagesTreeNewlines) {
    std::string content =
        "3 0 obj\n"
        "<</Type\n"
        "/Pages\n"
        "/Count\n"
        "99\n"
        ">>\n"
        "endobj\n";

    auto path = create_pdf_file("test3.pdf", content);
    auto result = extractor_.extract(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().page_count, 99u);
}

TEST_F(PdfMetadataExtractorTest, FallbackCountScan) {
    // Simple mock where Pages tree might be malformed, but Count is readable
    std::string content =
        "some dummy pdf objects here\n"
        "/Count 5\n"
        "some other trailer info";

    auto path = create_pdf_file("fallback.pdf", content);
    auto result = extractor_.extract(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().page_count, 5u);
}

TEST_F(PdfMetadataExtractorTest, CorruptPdfReturnsError) {
    // Missing any /Count references
    std::string content = "just some raw text with no pdf objects at all";
    auto path = create_pdf_file("corrupt.pdf", content);
    auto result = extractor_.extract(path);

    EXPECT_TRUE(result.has_error());
}

}  // namespace lilolify::infra::test
