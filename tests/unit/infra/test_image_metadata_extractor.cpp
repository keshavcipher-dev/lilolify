// ============================================================================
// Lilolify — Unit Tests: ImageMetadataExtractor
// ============================================================================

#include <lilolify/infra/metadata/image_metadata_extractor.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <vector>

namespace lilolify::infra::test {

namespace fs = std::filesystem;

class ImageMetadataExtractorTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "lilolify_img_test";
        fs::create_directories(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(test_dir_, ec);
    }

    fs::path create_binary_file(const std::string& name, const std::vector<std::uint8_t>& bytes) {
        auto path = test_dir_ / name;
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        return path;
    }

    ImageMetadataExtractor extractor_;
    fs::path test_dir_;
};

// ============================================================================
// PNG Header Tests
// ============================================================================

TEST_F(ImageMetadataExtractorTest, ParseValidPng) {
    // PNG format:
    // Signature: 89 50 4E 47 0D 0A 1A 0A (8B)
    // IHDR block length: 00 00 00 0D (4B)
    // IHDR type: 49 48 44 52 (4B) ("IHDR")
    // Width: 00 00 04 00 (1024px) (4B)
    // Height: 00 00 03 00 (768px) (4B)
    std::vector<std::uint8_t> png_bytes = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, // signature
        0x00, 0x00, 0x00, 0x0D,                         // length
        0x49, 0x48, 0x44, 0x52,                         // type
        0x00, 0x00, 0x04, 0x00,                         // width
        0x00, 0x00, 0x03, 0x00,                         // height
        0x08, 0x06, 0x00, 0x00, 0x00                    // other IHDR fields
    };

    auto path = create_binary_file("valid.png", png_bytes);
    auto result = extractor_.extract(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().image_width, 1024u);
    EXPECT_EQ(result.value().image_height, 768u);
}

// ============================================================================
// GIF Header Tests
// ============================================================================

TEST_F(ImageMetadataExtractorTest, ParseValidGif) {
    // GIF format:
    // Signature: "GIF89a" = 47 49 46 38 39 61 (6B)
    // Width: 20 00 (32px, little-endian) (2B)
    // Height: 10 00 (16px, little-endian) (2B)
    std::vector<std::uint8_t> gif_bytes = {
        0x47, 0x49, 0x46, 0x38, 0x39, 0x61, // signature
        0x20, 0x00,                         // width
        0x10, 0x00,                         // height
        0x70, 0x00, 0x00                    // logical screen descriptor fields
    };

    auto path = create_binary_file("valid.gif", gif_bytes);
    auto result = extractor_.extract(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().image_width, 32u);
    EXPECT_EQ(result.value().image_height, 16u);
}

// ============================================================================
// BMP Header Tests
// ============================================================================

TEST_F(ImageMetadataExtractorTest, ParseValidBmp) {
    // BMP format:
    // File header: BM = 42 4D (2B), file size (4B), reserved (4B), offset (4B) = 14B total
    // DIB header size: 28 00 00 00 (40 bytes) (4B)
    // Width: 40 01 00 00 (320px, signed little-endian) (4B)
    // Height: F0 00 00 00 (240px, signed little-endian) (4B)
    std::vector<std::uint8_t> bmp_bytes = {
        0x42, 0x4D,                         // signature
        0x00, 0x00, 0x00, 0x00,             // size (dummy)
        0x00, 0x00, 0x00, 0x00,             // reserved
        0x36, 0x00, 0x00, 0x00,             // offset
        0x28, 0x00, 0x00, 0x00,             // DIB size
        0x40, 0x01, 0x00, 0x00,             // width (320)
        0xF0, 0x00, 0x00, 0x00              // height (240)
    };

    auto path = create_binary_file("valid.bmp", bmp_bytes);
    auto result = extractor_.extract(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().image_width, 320u);
    EXPECT_EQ(result.value().image_height, 240u);
}

// ============================================================================
// JPEG Header Tests
// ============================================================================

TEST_F(ImageMetadataExtractorTest, ParseValidJpeg) {
    // JPEG SOI: FF D8
    // APP0 segment: FF E0, length: 00 10 (16B), identifier "JFIF\0"
    // SOF0 segment: FF C0, length: 00 11 (17B), precision: 08 (1B), height: 02 00 (512px, big-endian) (2B), width: 03 00 (768px, big-endian) (2B)
    std::vector<std::uint8_t> jpeg_bytes = {
        0xFF, 0xD8,                         // SOI
        0xFF, 0xE0, 0x00, 0x0A,             // APP0 marker + length (10 bytes)
        0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, // "JFIF\0"
        0x01, 0x01,
        0xFF, 0xC0, 0x00, 0x0B,             // SOF0 marker + length (11 bytes)
        0x08,                               // precision
        0x02, 0x00,                         // height (512)
        0x03, 0x00,                         // width (768)
        0x03, 0x01, 0x11, 0x00              // dummy component info
    };

    auto path = create_binary_file("valid.jpg", jpeg_bytes);
    auto result = extractor_.extract(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().image_width, 768u);
    EXPECT_EQ(result.value().image_height, 512u);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ImageMetadataExtractorTest, CorruptFileReturnsError) {
    // Random bytes which don't match any signature
    std::vector<std::uint8_t> corrupt_bytes = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    auto path = create_binary_file("corrupt.dat", corrupt_bytes);
    auto result = extractor_.extract(path);

    EXPECT_TRUE(result.has_error());
}

TEST_F(ImageMetadataExtractorTest, PrematureEofReturnsError) {
    // Just the PNG signature and length, missing width/height
    std::vector<std::uint8_t> short_bytes = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
        0x00, 0x00, 0x00, 0x0D,
        0x49, 0x48, 0x44, 0x52
    };

    auto path = create_binary_file("short.png", short_bytes);
    auto result = extractor_.extract(path);

    EXPECT_TRUE(result.has_error());
}

}  // namespace lilolify::infra::test
