// ============================================================================
// Lilolify — Unit Tests: MetadataEngine Orchestrator
// ============================================================================

#include <lilolify/core/interfaces/i_hash_calculator.hpp>
#include <lilolify/core/interfaces/i_metadata_extractor.hpp>
#include <lilolify/core/services/metadata_engine.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace lilolify::core::test {

// ============================================================================
// Mocks
// ============================================================================

class MockHashCalculator : public IHashCalculator {
public:
    MOCK_METHOD((Result<std::string, Error>), calculate_sha256, (const FilePath& path), (override));
};

class MockMetadataExtractor : public IMetadataExtractor {
public:
    MOCK_METHOD(bool, supports, (const std::string& mime_type, const std::string& extension), (const, override));
    MOCK_METHOD((Result<FileMetadata, Error>), extract, (const FilePath& path), (override));
};

// ============================================================================
// Orchestrator Tests
// ============================================================================

TEST(MetadataEngineTest, ConstructorNullHashCalculatorFailsOnExtract) {
    std::vector<std::shared_ptr<IMetadataExtractor>> extractors;
    MetadataEngine engine(nullptr, extractors);

    auto result = engine.extract_metadata("test.txt", "text/plain", ".txt");

    ASSERT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), ErrorCode::kInvalidArgument);
}

TEST(MetadataEngineTest, StandardHashingRunsForAllFiles) {
    auto mock_hash = std::make_shared<MockHashCalculator>();
    std::vector<std::shared_ptr<IMetadataExtractor>> extractors;

    EXPECT_CALL(*mock_hash, calculate_sha256(FilePath("photo.jpg")))
        .WillOnce(::testing::Return(Result<std::string, Error>::success("mock_sha256_checksum")));

    MetadataEngine engine(mock_hash, extractors);

    auto result = engine.extract_metadata("photo.jpg", "image/jpeg", ".jpg");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().sha256, "mock_sha256_checksum");
    EXPECT_FALSE(result.value().image_width.has_value());
}

TEST(MetadataEngineTest, ExtractorIsInvokedIfSupported) {
    auto mock_hash = std::make_shared<MockHashCalculator>();
    auto mock_extractor = std::make_shared<MockMetadataExtractor>();
    std::vector<std::shared_ptr<IMetadataExtractor>> extractors = {mock_extractor};

    EXPECT_CALL(*mock_hash, calculate_sha256(FilePath("photo.jpg")))
        .WillOnce(::testing::Return(Result<std::string, Error>::success("mock_sha256_checksum")));

    // Extractor supports JPEG
    EXPECT_CALL(*mock_extractor, supports("image/jpeg", ".jpg"))
        .WillOnce(::testing::Return(true));

    FileMetadata extracted_data;
    extracted_data.image_width = 1920;
    extracted_data.image_height = 1080;

    EXPECT_CALL(*mock_extractor, extract(FilePath("photo.jpg")))
        .WillOnce(::testing::Return(Result<FileMetadata, Error>::success(std::move(extracted_data))));

    MetadataEngine engine(mock_hash, extractors);

    auto result = engine.extract_metadata("photo.jpg", "image/jpeg", ".jpg");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().sha256, "mock_sha256_checksum");
    EXPECT_EQ(result.value().image_width, 1920u);
    EXPECT_EQ(result.value().image_height, 1080u);
}

TEST(MetadataEngineTest, FirstMatchingExtractorWins) {
    auto mock_hash = std::make_shared<MockHashCalculator>();
    auto mock1 = std::make_shared<MockMetadataExtractor>();
    auto mock2 = std::make_shared<MockMetadataExtractor>();

    // Mock 1 matches and extracts data
    EXPECT_CALL(*mock_hash, calculate_sha256(FilePath("doc.pdf")))
        .WillOnce(::testing::Return(Result<std::string, Error>::success("sha")));

    EXPECT_CALL(*mock1, supports("application/pdf", ".pdf"))
        .WillOnce(::testing::Return(true));

    FileMetadata data1;
    data1.page_count = 5;

    EXPECT_CALL(*mock1, extract(FilePath("doc.pdf")))
        .WillOnce(::testing::Return(Result<FileMetadata, Error>::success(data1)));

    // Mock 2 should not be checked at all because Mock 1 won
    EXPECT_CALL(*mock2, supports(::testing::_, ::testing::_))
        .Times(0);

    std::vector<std::shared_ptr<IMetadataExtractor>> extractors = {mock1, mock2};
    MetadataEngine engine(mock_hash, extractors);

    auto result = engine.extract_metadata("doc.pdf", "application/pdf", ".pdf");

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().page_count, 5u);
}

TEST(MetadataEngineTest, ExtractorErrorBubbelsUp) {
    auto mock_hash = std::make_shared<MockHashCalculator>();
    auto mock_extractor = std::make_shared<MockMetadataExtractor>();
    std::vector<std::shared_ptr<IMetadataExtractor>> extractors = {mock_extractor};

    EXPECT_CALL(*mock_hash, calculate_sha256(FilePath("photo.jpg")))
        .WillOnce(::testing::Return(Result<std::string, Error>::success("sha")));

    EXPECT_CALL(*mock_extractor, supports("image/jpeg", ".jpg"))
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*mock_extractor, extract(FilePath("photo.jpg")))
        .WillOnce(::testing::Return(Result<FileMetadata, Error>::failure(
            Error(ErrorCode::kFileReadError, "Corrupt file parsing"))));

    MetadataEngine engine(mock_hash, extractors);

    auto result = engine.extract_metadata("photo.jpg", "image/jpeg", ".jpg");

    ASSERT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), ErrorCode::kOcrProcessingError); // wrapped error code
    EXPECT_TRUE(result.error().cause() != nullptr);
    EXPECT_EQ(result.error().cause()->message(), "Corrupt file parsing");
}

}  // namespace lilolify::core::test
