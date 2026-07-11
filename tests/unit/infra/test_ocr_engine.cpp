// ============================================================================
// Lilolify — Unit Tests: OCR Engine Integration
// ============================================================================

#include <lilolify/infra/ocr/mock_ocr_engine.hpp>
#include <lilolify/infra/ocr/tesseract_ocr_engine.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace lilolify::infra::test {

using core::Result;
using core::Error;

namespace fs = std::filesystem;

class OcrEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "lilolify_ocr_unit_test";
        fs::create_directories(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(test_dir_, ec);
    }

    fs::path create_dummy_file(const std::string& name) {
        auto path = test_dir_ / name;
        std::ofstream file(path);
        file << "dummy data";
        return path;
    }

    fs::path test_dir_;
};

// ============================================================================
// MockOcrEngine Tests
// ============================================================================

TEST_F(OcrEngineTest, MockEngineNotInitializedFails) {
    MockOcrEngine mock;
    auto file = create_dummy_file("receipt_test.png");
    auto result = mock.extract_text(file);

    ASSERT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), core::ErrorCode::kOcrProcessingError);
}

TEST_F(OcrEngineTest, MockEngineInitializationSucceeds) {
    MockOcrEngine mock;
    auto init = mock.initialize("C:/some/tessdata", "eng");

    ASSERT_TRUE(init.has_value());
    EXPECT_TRUE(mock.is_initialized());
    EXPECT_EQ(mock.language(), "eng");
}

TEST_F(OcrEngineTest, MockEngineMissingFileReturnsError) {
    MockOcrEngine mock;
    ASSERT_TRUE(mock.initialize("C:/some/tessdata", "eng").has_value());

    auto result = mock.extract_text("C:/nonexistent/image.png");
    ASSERT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), core::ErrorCode::kFileNotFound);
}

TEST_F(OcrEngineTest, MockEngineReceiptParsing) {
    MockOcrEngine mock;
    ASSERT_TRUE(mock.initialize("C:/some/tessdata", "eng").has_value());

    auto file = create_dummy_file("store_receipt.png");
    auto result = mock.extract_text(file);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().find("USD $42.50") != std::string::npos);
}

TEST_F(OcrEngineTest, MockEngineInvoiceParsing) {
    MockOcrEngine mock;
    ASSERT_TRUE(mock.initialize("C:/some/tessdata", "eng").has_value());

    auto file = create_dummy_file("client_invoice.png");
    auto result = mock.extract_text(file);

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().find("INVOICE #99812") != std::string::npos);
}

// ============================================================================
// TesseractOcrEngine Stub / Real API Tests
// ============================================================================

TEST_F(OcrEngineTest, TesseractEngineInitializationValidatesPaths) {
    TesseractOcrEngine tess;

    // A nonexistent path should fail initialization (real mode check)
    // In stub mode it will succeed, so we handle both compile paths gracefully.
#ifdef LILOLIFY_WITH_TESSERACT
    auto result = tess.initialize("C:/nonexistent/tessdata/folder", "eng");
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), core::ErrorCode::kOcrInitError);
#else
    auto result = tess.initialize("C:/some/tessdata", "eng");
    EXPECT_TRUE(result.has_value());
#endif
}

TEST_F(OcrEngineTest, TesseractEngineNotInitializedFails) {
    TesseractOcrEngine tess;
    auto file = create_dummy_file("test.png");
    auto result = tess.extract_text(file);

    ASSERT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), core::ErrorCode::kOcrProcessingError);
}

TEST_F(OcrEngineTest, TesseractEngineMissingFileReturnsError) {
    TesseractOcrEngine tess;
    ASSERT_TRUE(tess.initialize("C:/some/tessdata", "eng").has_value());

    auto result = tess.extract_text("C:/nonexistent/file.png");
    ASSERT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), core::ErrorCode::kFileNotFound);
}

}  // namespace lilolify::infra::test
