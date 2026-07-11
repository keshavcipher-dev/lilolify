// ============================================================================
// Lilolify — Unit Tests: ScanOptions Value Object
// ============================================================================

#include <lilolify/core/value_objects/scan_options.hpp>

#include <gtest/gtest.h>

namespace lilolify::core::test {

// ============================================================================
// Default Values Tests
// ============================================================================

TEST(ScanOptionsTest, DefaultsAreReasonable) {
    ScanOptions opts;

    EXPECT_TRUE(opts.root_directory.empty());
    EXPECT_TRUE(opts.recursive);
    EXPECT_EQ(opts.max_depth, 50u);
    EXPECT_EQ(opts.max_file_size, 100ULL * 1024 * 1024);  // 100 MB
    EXPECT_EQ(opts.min_file_size, 0u);
    EXPECT_TRUE(opts.include_extensions.empty());
    EXPECT_TRUE(opts.exclude_extensions.empty());
    EXPECT_TRUE(opts.exclude_directories.empty());
    EXPECT_EQ(opts.progress_interval, 100u);
    EXPECT_FALSE(opts.follow_symlinks);
    EXPECT_TRUE(opts.detect_mime_type);
}

// ============================================================================
// Validation Tests
// ============================================================================

TEST(ScanOptionsTest, ValidateFailsOnEmptyRootDirectory) {
    ScanOptions opts;
    auto result = opts.validate();

    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), ErrorCode::kInvalidArgument);
}

TEST(ScanOptionsTest, ValidateSucceedsWithRootDirectory) {
    ScanOptions opts;
    opts.root_directory = "C:/test/path";
    auto result = opts.validate();

    EXPECT_TRUE(result.has_value());
}

TEST(ScanOptionsTest, ValidateFailsOnMinGreaterThanMax) {
    ScanOptions opts;
    opts.root_directory = "C:/test";
    opts.min_file_size = 1000;
    opts.max_file_size = 500;
    auto result = opts.validate();

    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), ErrorCode::kInvalidArgument);
}

TEST(ScanOptionsTest, ValidateFailsOnZeroProgressInterval) {
    ScanOptions opts;
    opts.root_directory = "C:/test";
    opts.progress_interval = 0;
    auto result = opts.validate();

    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), ErrorCode::kInvalidArgument);
}

// ============================================================================
// Extension Normalization Tests
// ============================================================================

TEST(ScanOptionsTest, NormalizeExtensionsToLowercase) {
    ScanOptions opts;
    opts.include_extensions = {".JPG", ".PNG", ".GIF"};
    opts.normalize_extensions();

    EXPECT_EQ(opts.include_extensions[0], ".jpg");
    EXPECT_EQ(opts.include_extensions[1], ".png");
    EXPECT_EQ(opts.include_extensions[2], ".gif");
}

TEST(ScanOptionsTest, NormalizeAddsDotIfMissing) {
    ScanOptions opts;
    opts.include_extensions = {"jpg", "png"};
    opts.normalize_extensions();

    EXPECT_EQ(opts.include_extensions[0], ".jpg");
    EXPECT_EQ(opts.include_extensions[1], ".png");
}

TEST(ScanOptionsTest, NormalizeExcludeExtensions) {
    ScanOptions opts;
    opts.exclude_extensions = {".TMP", "BAK"};
    opts.normalize_extensions();

    EXPECT_EQ(opts.exclude_extensions[0], ".tmp");
    EXPECT_EQ(opts.exclude_extensions[1], ".bak");
}

// ============================================================================
// Extension Filter Tests
// ============================================================================

TEST(ScanOptionsTest, IsExtensionAllowedNoFilters) {
    ScanOptions opts;
    // No include/exclude = everything allowed
    EXPECT_TRUE(opts.is_extension_allowed(".jpg"));
    EXPECT_TRUE(opts.is_extension_allowed(".exe"));
    EXPECT_TRUE(opts.is_extension_allowed(""));
}

TEST(ScanOptionsTest, IsExtensionAllowedWithIncludeList) {
    ScanOptions opts;
    opts.include_extensions = {".jpg", ".png"};

    EXPECT_TRUE(opts.is_extension_allowed(".jpg"));
    EXPECT_TRUE(opts.is_extension_allowed(".png"));
    EXPECT_FALSE(opts.is_extension_allowed(".gif"));
    EXPECT_FALSE(opts.is_extension_allowed(".exe"));
}

TEST(ScanOptionsTest, IsExtensionAllowedWithExcludeList) {
    ScanOptions opts;
    opts.exclude_extensions = {".tmp", ".bak"};

    EXPECT_TRUE(opts.is_extension_allowed(".jpg"));
    EXPECT_FALSE(opts.is_extension_allowed(".tmp"));
    EXPECT_FALSE(opts.is_extension_allowed(".bak"));
}

TEST(ScanOptionsTest, IsExtensionAllowedBothLists) {
    ScanOptions opts;
    opts.include_extensions = {".jpg", ".png", ".tmp"};
    opts.exclude_extensions = {".tmp"};

    EXPECT_TRUE(opts.is_extension_allowed(".jpg"));
    EXPECT_FALSE(opts.is_extension_allowed(".gif"));  // Not in include
    EXPECT_FALSE(opts.is_extension_allowed(".tmp"));   // In exclude
}

// ============================================================================
// Size Filter Tests
// ============================================================================

TEST(ScanOptionsTest, IsSizeAllowedDefault) {
    ScanOptions opts;
    EXPECT_TRUE(opts.is_size_allowed(0));
    EXPECT_TRUE(opts.is_size_allowed(1024));
    EXPECT_TRUE(opts.is_size_allowed(100ULL * 1024 * 1024));   // 100 MB = max
    EXPECT_FALSE(opts.is_size_allowed(100ULL * 1024 * 1024 + 1));  // > max
}

TEST(ScanOptionsTest, IsSizeAllowedCustomRange) {
    ScanOptions opts;
    opts.min_file_size = 100;
    opts.max_file_size = 10000;

    EXPECT_FALSE(opts.is_size_allowed(50));     // Below min
    EXPECT_TRUE(opts.is_size_allowed(100));      // Exactly min
    EXPECT_TRUE(opts.is_size_allowed(5000));     // In range
    EXPECT_TRUE(opts.is_size_allowed(10000));    // Exactly max
    EXPECT_FALSE(opts.is_size_allowed(10001));   // Above max
}

}  // namespace lilolify::core::test
