// ============================================================================
// Lilolify — Unit Tests: Sha256 & Sha256Calculator
// ============================================================================

#include <lilolify/infra/crypto/sha256.hpp>
#include <lilolify/infra/crypto/sha256_calculator.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace lilolify::infra::test {

namespace fs = std::filesystem;

class Sha256Test : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "lilolify_sha_test";
        fs::create_directories(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(test_dir_, ec);
    }

    fs::path create_temp_file(const std::string& name, const std::string& content) {
        auto path = test_dir_ / name;
        std::ofstream file(path, std::ios::binary);
        file << content;
        return path;
    }

    fs::path test_dir_;
};

// ============================================================================
// Raw Sha256 Algorithm Tests (Standard Test Vectors)
// ============================================================================

TEST_F(Sha256Test, HashEmptyString) {
    // Standard SHA-256 for empty string: e3b0c442...
    std::string expected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
    EXPECT_EQ(Sha256::hash_string(""), expected);
}

TEST_F(Sha256Test, HashAbc) {
    // Standard SHA-256 for "abc"
    std::string expected = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    EXPECT_EQ(Sha256::hash_string("abc"), expected);
}

TEST_F(Sha256Test, HashLongMessage) {
    // Standard SHA-256 for "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
    std::string message = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    std::string expected = "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1";
    EXPECT_EQ(Sha256::hash_string(message), expected);
}

// ============================================================================
// Sha256Calculator (Streaming) Tests
// ============================================================================

TEST_F(Sha256Test, CalculatorHashesEmptyFile) {
    auto path = create_temp_file("empty.txt", "");
    Sha256Calculator calc;
    auto result = calc.calculate_sha256(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_F(Sha256Test, CalculatorHashesFileWithContent) {
    auto path = create_temp_file("abc.txt", "abc");
    Sha256Calculator calc;
    auto result = calc.calculate_sha256(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST_F(Sha256Test, CalculatorHashesLargeChunks) {
    // Create content exactly matching 65 KB (crosses 64 KB buffer boundary)
    std::string content(65 * 1024, 'x');
    auto path = create_temp_file("large.txt", content);

    // Compute expected hash using basic algorithm
    std::string expected = Sha256::hash_string(content);

    Sha256Calculator calc;
    auto result = calc.calculate_sha256(path);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), expected);
}

TEST_F(Sha256Test, CalculatorNonexistentFileReturnsError) {
    Sha256Calculator calc;
    auto result = calc.calculate_sha256("C:/nonexistent/file/path.txt");

    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), core::ErrorCode::kFileNotFound);
}

}  // namespace lilolify::infra::test
