// ============================================================================
// Lilolify — Unit Tests: Error Hierarchy
// ============================================================================
// Tests for the Error class and ErrorCode system covering:
//   - Construction and accessors
//   - Causal chaining (wrapping root causes)
//   - Factory methods
//   - full_message() formatting
//   - error_category() utility
// ============================================================================

#include <lilolify/core/error.hpp>

#include <gtest/gtest.h>

#include <string>
#include <utility>

namespace lilolify::core::test {

// ============================================================================
// Basic Construction Tests
// ============================================================================

TEST(ErrorTest, ConstructWithCodeAndMessage) {
    Error error(ErrorCode::kFileNotFound, "config.json not found");

    EXPECT_EQ(error.code(), ErrorCode::kFileNotFound);
    EXPECT_EQ(error.message(), "config.json not found");
    EXPECT_EQ(error.cause(), nullptr);
}

TEST(ErrorTest, IsMethodMatchesCode) {
    Error error(ErrorCode::kTimeout, "Request timed out");

    EXPECT_TRUE(error.is(ErrorCode::kTimeout));
    EXPECT_FALSE(error.is(ErrorCode::kNotFound));
    EXPECT_FALSE(error.is(ErrorCode::kUnknown));
}

// ============================================================================
// Causal Chain Tests
// ============================================================================

TEST(ErrorTest, ConstructWithCause) {
    Error root_cause(ErrorCode::kFileNotFound, "database.db not found");
    Error error(ErrorCode::kDatabaseOpenError, "Cannot open database", std::move(root_cause));

    EXPECT_EQ(error.code(), ErrorCode::kDatabaseOpenError);
    EXPECT_EQ(error.message(), "Cannot open database");

    ASSERT_NE(error.cause(), nullptr);
    EXPECT_EQ(error.cause()->code(), ErrorCode::kFileNotFound);
    EXPECT_EQ(error.cause()->message(), "database.db not found");
    EXPECT_EQ(error.cause()->cause(), nullptr);
}

TEST(ErrorTest, DeepCausalChain) {
    Error level1(ErrorCode::kFileReadError, "Cannot read file");
    Error level2(ErrorCode::kOcrProcessingError, "OCR failed", std::move(level1));
    Error level3(ErrorCode::kPipelineStepFailed, "Pipeline step failed", std::move(level2));

    EXPECT_EQ(level3.code(), ErrorCode::kPipelineStepFailed);

    const auto* l2 = level3.cause();
    ASSERT_NE(l2, nullptr);
    EXPECT_EQ(l2->code(), ErrorCode::kOcrProcessingError);

    const auto* l1 = l2->cause();
    ASSERT_NE(l1, nullptr);
    EXPECT_EQ(l1->code(), ErrorCode::kFileReadError);

    EXPECT_EQ(l1->cause(), nullptr);
}

// ============================================================================
// Move Semantics Tests
// ============================================================================

TEST(ErrorTest, MoveConstruction) {
    Error original(ErrorCode::kAiTimeout, "AI provider timed out after 30s");
    Error moved(std::move(original));

    EXPECT_EQ(moved.code(), ErrorCode::kAiTimeout);
    EXPECT_EQ(moved.message(), "AI provider timed out after 30s");
}

TEST(ErrorTest, MoveAssignment) {
    Error original(ErrorCode::kDiskFull, "No space left on device");
    Error target(ErrorCode::kUnknown, "placeholder");

    target = std::move(original);

    EXPECT_EQ(target.code(), ErrorCode::kDiskFull);
    EXPECT_EQ(target.message(), "No space left on device");
}

TEST(ErrorTest, MovePreservesCausalChain) {
    Error cause(ErrorCode::kFileNotFound, "missing.txt");
    Error error(ErrorCode::kPipelineError, "Pipeline failed", std::move(cause));
    Error moved(std::move(error));

    EXPECT_EQ(moved.code(), ErrorCode::kPipelineError);
    ASSERT_NE(moved.cause(), nullptr);
    EXPECT_EQ(moved.cause()->code(), ErrorCode::kFileNotFound);
}

// ============================================================================
// Factory Method Tests
// ============================================================================

TEST(ErrorTest, FactoryNotFound) {
    auto error = Error::not_found("User with ID 42");

    EXPECT_EQ(error.code(), ErrorCode::kNotFound);
    EXPECT_EQ(error.message(), "User with ID 42");
}

TEST(ErrorTest, FactoryInvalidArgument) {
    auto error = Error::invalid_argument("Port must be between 1 and 65535");

    EXPECT_EQ(error.code(), ErrorCode::kInvalidArgument);
    EXPECT_EQ(error.message(), "Port must be between 1 and 65535");
}

TEST(ErrorTest, FactoryPermissionDenied) {
    auto error = Error::permission_denied("/etc/shadow");

    EXPECT_EQ(error.code(), ErrorCode::kPermissionDenied);
    EXPECT_EQ(error.message(), "/etc/shadow");
}

TEST(ErrorTest, FactoryNotImplemented) {
    auto error = Error::not_implemented("Cloud Sync");

    EXPECT_EQ(error.code(), ErrorCode::kNotImplemented);
    EXPECT_EQ(error.message(), "Cloud Sync is not yet implemented");
}

// ============================================================================
// full_message() Tests
// ============================================================================

TEST(ErrorTest, FullMessageSimple) {
    Error error(ErrorCode::kNotFound, "Item missing");
    auto msg = error.full_message();

    EXPECT_NE(msg.find("Item missing"), std::string::npos);
    // Should contain the numeric code
    EXPECT_NE(msg.find("2"), std::string::npos);  // ErrorCode::kNotFound = 2
}

TEST(ErrorTest, FullMessageWithCause) {
    Error cause(ErrorCode::kFileNotFound, "data.db");
    Error error(ErrorCode::kDatabaseOpenError, "DB init failed", std::move(cause));

    auto msg = error.full_message();

    EXPECT_NE(msg.find("DB init failed"), std::string::npos);
    EXPECT_NE(msg.find("Caused by"), std::string::npos);
    EXPECT_NE(msg.find("data.db"), std::string::npos);
}

TEST(ErrorTest, FullMessageDeepChain) {
    Error l1(ErrorCode::kFileReadError, "IO error");
    Error l2(ErrorCode::kOcrProcessingError, "OCR failed", std::move(l1));
    Error l3(ErrorCode::kPipelineStepFailed, "Step 3 failed", std::move(l2));

    auto msg = l3.full_message();

    // Should contain all three levels
    EXPECT_NE(msg.find("Step 3 failed"), std::string::npos);
    EXPECT_NE(msg.find("OCR failed"), std::string::npos);
    EXPECT_NE(msg.find("IO error"), std::string::npos);

    // Count "Caused by" occurrences — should be exactly 2
    size_t count = 0;
    size_t pos = 0;
    while ((pos = msg.find("Caused by", pos)) != std::string::npos) {
        ++count;
        pos += 9;  // length of "Caused by"
    }
    EXPECT_EQ(count, 2);
}

// ============================================================================
// error_category() Utility Tests
// ============================================================================

TEST(ErrorCategoryTest, GeneralCodes) {
    EXPECT_EQ(error_category(ErrorCode::kUnknown), "General");
    EXPECT_EQ(error_category(ErrorCode::kNotFound), "General");
    EXPECT_EQ(error_category(ErrorCode::kTimeout), "General");
}

TEST(ErrorCategoryTest, FileSystemCodes) {
    EXPECT_EQ(error_category(ErrorCode::kFileNotFound), "FileSystem");
    EXPECT_EQ(error_category(ErrorCode::kFileMoveError), "FileSystem");
    EXPECT_EQ(error_category(ErrorCode::kDiskFull), "FileSystem");
}

TEST(ErrorCategoryTest, AiCodes) {
    EXPECT_EQ(error_category(ErrorCode::kAiProviderError), "AI");
    EXPECT_EQ(error_category(ErrorCode::kAiRateLimited), "AI");
    EXPECT_EQ(error_category(ErrorCode::kAiTimeout), "AI");
}

TEST(ErrorCategoryTest, OcrCodes) {
    EXPECT_EQ(error_category(ErrorCode::kOcrEngineError), "OCR");
    EXPECT_EQ(error_category(ErrorCode::kOcrLanguageNotFound), "OCR");
}

TEST(ErrorCategoryTest, DatabaseCodes) {
    EXPECT_EQ(error_category(ErrorCode::kDatabaseOpenError), "Database");
    EXPECT_EQ(error_category(ErrorCode::kDatabaseCorrupted), "Database");
}

TEST(ErrorCategoryTest, PipelineCodes) {
    EXPECT_EQ(error_category(ErrorCode::kPipelineError), "Pipeline");
    EXPECT_EQ(error_category(ErrorCode::kPipelineAborted), "Pipeline");
}

TEST(ErrorCategoryTest, ConfigurationCodes) {
    EXPECT_EQ(error_category(ErrorCode::kConfigNotFound), "Configuration");
    EXPECT_EQ(error_category(ErrorCode::kConfigParseError), "Configuration");
}

}  // namespace lilolify::core::test
