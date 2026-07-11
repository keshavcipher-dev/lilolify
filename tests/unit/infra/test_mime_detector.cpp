// ============================================================================
// Lilolify — Unit Tests: MimeDetector
// ============================================================================

#include <lilolify/infra/filesystem/mime_detector.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace lilolify::infra::test {

namespace fs = std::filesystem;

class MimeDetectorTest : public ::testing::Test {
protected:
    MimeDetector detector_;

    // Create a temp file with specific magic bytes
    fs::path create_temp_file(
        const std::string& filename,
        const std::vector<std::uint8_t>& content) {

        auto dir = fs::temp_directory_path() / "lilolify_test_mime";
        fs::create_directories(dir);
        auto path = dir / filename;

        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(content.data()),
                   static_cast<std::streamsize>(content.size()));
        file.close();

        temp_files_.push_back(path);
        return path;
    }

    void TearDown() override {
        for (const auto& path : temp_files_) {
            std::error_code ec;
            fs::remove(path, ec);
        }
        std::error_code ec;
        fs::remove(fs::temp_directory_path() / "lilolify_test_mime", ec);
    }

private:
    std::vector<fs::path> temp_files_;
};

// ============================================================================
// Extension-Based Detection Tests
// ============================================================================

TEST_F(MimeDetectorTest, DetectJpegByExtension) {
    EXPECT_EQ(detector_.detect_from_extension(".jpg"), "image/jpeg");
    EXPECT_EQ(detector_.detect_from_extension(".jpeg"), "image/jpeg");
}

TEST_F(MimeDetectorTest, DetectPngByExtension) {
    EXPECT_EQ(detector_.detect_from_extension(".png"), "image/png");
}

TEST_F(MimeDetectorTest, DetectPdfByExtension) {
    EXPECT_EQ(detector_.detect_from_extension(".pdf"), "application/pdf");
}

TEST_F(MimeDetectorTest, DetectDocxByExtension) {
    auto mime = detector_.detect_from_extension(".docx");
    EXPECT_TRUE(mime.starts_with("application/vnd.openxmlformats"));
}

TEST_F(MimeDetectorTest, DetectMp4ByExtension) {
    EXPECT_EQ(detector_.detect_from_extension(".mp4"), "video/mp4");
}

TEST_F(MimeDetectorTest, CaseInsensitiveExtension) {
    EXPECT_EQ(detector_.detect_from_extension(".JPG"), "image/jpeg");
    EXPECT_EQ(detector_.detect_from_extension(".Png"), "image/png");
    EXPECT_EQ(detector_.detect_from_extension(".PDF"), "application/pdf");
}

TEST_F(MimeDetectorTest, ExtensionWithoutDot) {
    EXPECT_EQ(detector_.detect_from_extension("jpg"), "image/jpeg");
}

TEST_F(MimeDetectorTest, UnknownExtension) {
    auto mime = detector_.detect_from_extension(".xyz123");
    EXPECT_EQ(mime, MimeDetector::kUnknownMime);
}

TEST_F(MimeDetectorTest, EmptyExtension) {
    auto mime = detector_.detect_from_extension("");
    EXPECT_EQ(mime, MimeDetector::kUnknownMime);
}

// ============================================================================
// Magic Byte Detection Tests
// ============================================================================

TEST_F(MimeDetectorTest, DetectJpegByMagicBytes) {
    auto path = create_temp_file("test.dat",
        {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46});
    EXPECT_EQ(detector_.detect_from_file(path), "image/jpeg");
}

TEST_F(MimeDetectorTest, DetectPngByMagicBytes) {
    auto path = create_temp_file("test.dat",
        {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00});
    EXPECT_EQ(detector_.detect_from_file(path), "image/png");
}

TEST_F(MimeDetectorTest, DetectPdfByMagicBytes) {
    auto path = create_temp_file("test.dat",
        {0x25, 0x50, 0x44, 0x46, 0x2D, 0x31, 0x2E, 0x34});
    EXPECT_EQ(detector_.detect_from_file(path), "application/pdf");
}

TEST_F(MimeDetectorTest, DetectGifByMagicBytes) {
    auto path = create_temp_file("test.dat",
        {0x47, 0x49, 0x46, 0x38, 0x39, 0x61});  // GIF89a
    EXPECT_EQ(detector_.detect_from_file(path), "image/gif");
}

TEST_F(MimeDetectorTest, DetectBmpByMagicBytes) {
    auto path = create_temp_file("test.dat",
        {0x42, 0x4D, 0x00, 0x00, 0x00, 0x00});  // BM
    EXPECT_EQ(detector_.detect_from_file(path), "image/bmp");
}

TEST_F(MimeDetectorTest, DetectZipByMagicBytes) {
    auto path = create_temp_file("test.dat",
        {0x50, 0x4B, 0x03, 0x04, 0x0A, 0x00});  // PK..
    EXPECT_EQ(detector_.detect_from_file(path), "application/zip");
}

TEST_F(MimeDetectorTest, FallbackToExtensionWhenMagicFails) {
    // Random bytes but .jpg extension
    auto path = create_temp_file("test.jpg",
        {0x00, 0x01, 0x02, 0x03, 0x04, 0x05});
    // Should fall back to extension detection
    EXPECT_EQ(detector_.detect_from_file(path), "image/jpeg");
}

TEST_F(MimeDetectorTest, NonexistentFileReturnsUnknown) {
    auto path = fs::path("C:/nonexistent/file/that/does/not/exist.xyz");
    auto mime = detector_.detect_from_file(path);
    EXPECT_EQ(mime, MimeDetector::kUnknownMime);
}

// ============================================================================
// Classification Tests
// ============================================================================

TEST_F(MimeDetectorTest, IsImageMime) {
    EXPECT_TRUE(MimeDetector::is_image_mime("image/jpeg"));
    EXPECT_TRUE(MimeDetector::is_image_mime("image/png"));
    EXPECT_TRUE(MimeDetector::is_image_mime("image/gif"));
    EXPECT_FALSE(MimeDetector::is_image_mime("application/pdf"));
    EXPECT_FALSE(MimeDetector::is_image_mime("text/plain"));
}

TEST_F(MimeDetectorTest, IsDocumentMime) {
    EXPECT_TRUE(MimeDetector::is_document_mime("application/pdf"));
    EXPECT_TRUE(MimeDetector::is_document_mime("application/msword"));
    EXPECT_TRUE(MimeDetector::is_document_mime(
        "application/vnd.openxmlformats-officedocument.wordprocessingml.document"));
    EXPECT_FALSE(MimeDetector::is_document_mime("image/jpeg"));
    EXPECT_FALSE(MimeDetector::is_document_mime("text/plain"));
}

}  // namespace lilolify::infra::test
