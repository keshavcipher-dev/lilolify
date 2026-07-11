// ============================================================================
// Lilolify — Unit Tests: FileEntry Entity
// ============================================================================

#include <lilolify/core/entities/file_entry.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>

namespace lilolify::core::test {

namespace fs = std::filesystem;

// Helper to create a FileEntry with test data
FileEntry make_test_entry(
    const std::string& filename = "test_photo.jpg",
    FileSize size = 1024 * 1024) {  // 1 MB

    auto now = std::chrono::system_clock::now();
    return FileEntry(
        fs::path("C:/Users/test/Photos") / filename,
        size,
        now,
        now);
}

// ============================================================================
// Construction Tests
// ============================================================================

TEST(FileEntryTest, ConstructionSetsAllFields) {
    auto now = std::chrono::system_clock::now();
    FileEntry entry(fs::path("C:/test/report.pdf"), 2048, now, now);

    EXPECT_EQ(entry.path(), fs::path("C:/test/report.pdf"));
    EXPECT_EQ(entry.filename(), "report.pdf");
    EXPECT_EQ(entry.extension(), ".pdf");
    EXPECT_EQ(entry.size_bytes(), 2048u);
    EXPECT_EQ(entry.status(), ProcessingStatus::kPending);
    EXPECT_EQ(entry.id(), kInvalidEntityId);
    EXPECT_FALSE(entry.is_persisted());
    EXPECT_TRUE(entry.mime_type().empty());
}

TEST(FileEntryTest, ExtensionNormalizedToLowercase) {
    auto entry = make_test_entry("Photo.JPG");
    EXPECT_EQ(entry.extension(), ".jpg");
}

TEST(FileEntryTest, ExtensionNormalizedMixedCase) {
    auto entry = make_test_entry("Document.PdF");
    EXPECT_EQ(entry.extension(), ".pdf");
}

TEST(FileEntryTest, NoExtensionReturnsEmpty) {
    auto entry = make_test_entry("Makefile");
    EXPECT_EQ(entry.extension(), "");
}

TEST(FileEntryTest, HiddenFileExtension) {
    auto entry = make_test_entry(".gitignore");
    // .gitignore has no extension — it IS the filename
    EXPECT_EQ(entry.filename(), ".gitignore");
}

// ============================================================================
// Mutator Tests
// ============================================================================

TEST(FileEntryTest, SetIdUpdatesId) {
    auto entry = make_test_entry();
    EXPECT_FALSE(entry.is_persisted());

    entry.set_id(42);
    EXPECT_EQ(entry.id(), 42);
    EXPECT_TRUE(entry.is_persisted());
}

TEST(FileEntryTest, SetMimeType) {
    auto entry = make_test_entry();
    EXPECT_TRUE(entry.mime_type().empty());

    entry.set_mime_type("image/jpeg");
    EXPECT_EQ(entry.mime_type(), "image/jpeg");
}

TEST(FileEntryTest, SetStatus) {
    auto entry = make_test_entry();
    EXPECT_EQ(entry.status(), ProcessingStatus::kPending);

    entry.set_status(ProcessingStatus::kAnalyzing);
    EXPECT_EQ(entry.status(), ProcessingStatus::kAnalyzing);
}

// ============================================================================
// Query Tests
// ============================================================================

TEST(FileEntryTest, IsImageWithImageMime) {
    auto entry = make_test_entry();
    entry.set_mime_type("image/jpeg");
    EXPECT_TRUE(entry.is_image());
}

TEST(FileEntryTest, IsImageWithNonImageMime) {
    auto entry = make_test_entry();
    entry.set_mime_type("application/pdf");
    EXPECT_FALSE(entry.is_image());
}

TEST(FileEntryTest, IsDocumentWithPdf) {
    auto entry = make_test_entry("doc.pdf");
    entry.set_mime_type("application/pdf");
    EXPECT_TRUE(entry.is_document());
}

TEST(FileEntryTest, IsDocumentWithDocx) {
    auto entry = make_test_entry("doc.docx");
    entry.set_mime_type("application/vnd.openxmlformats-officedocument.wordprocessingml.document");
    EXPECT_TRUE(entry.is_document());
}

// ============================================================================
// Human-Readable Size Tests
// ============================================================================

TEST(FileEntryTest, HumanReadableSizeBytes) {
    auto entry = make_test_entry("tiny.txt", 512);
    EXPECT_EQ(entry.human_readable_size(), "512 B");
}

TEST(FileEntryTest, HumanReadableSizeKilobytes) {
    auto entry = make_test_entry("small.txt", 2560);  // 2.5 KB
    auto size = entry.human_readable_size();
    EXPECT_TRUE(size.find("KB") != std::string::npos);
}

TEST(FileEntryTest, HumanReadableSizeMegabytes) {
    auto entry = make_test_entry("photo.jpg", 5 * 1024 * 1024);  // 5 MB
    auto size = entry.human_readable_size();
    EXPECT_TRUE(size.find("MB") != std::string::npos);
}

TEST(FileEntryTest, HumanReadableSizeGigabytes) {
    auto entry = make_test_entry("video.mp4", 2ULL * 1024 * 1024 * 1024);  // 2 GB
    auto size = entry.human_readable_size();
    EXPECT_TRUE(size.find("GB") != std::string::npos);
}

// ============================================================================
// Copy/Move Semantics
// ============================================================================

TEST(FileEntryTest, CopyConstruction) {
    auto original = make_test_entry();
    original.set_mime_type("image/png");

    auto copy = original;

    EXPECT_EQ(copy.path(), original.path());
    EXPECT_EQ(copy.mime_type(), "image/png");
}

TEST(FileEntryTest, MoveConstruction) {
    auto original = make_test_entry();
    auto original_path = original.path();

    auto moved = std::move(original);

    EXPECT_EQ(moved.path(), original_path);
}

}  // namespace lilolify::core::test
