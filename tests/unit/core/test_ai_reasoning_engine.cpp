// ============================================================================
// Lilolify — Unit Tests: AI Reasoning Engine
// ============================================================================

#include <lilolify/core/services/ai_reasoning_engine.hpp>

#include <gtest/gtest.h>

namespace lilolify::core::test {

// ============================================================================
// Test Mocks
// ============================================================================

class MockAiProvider : public IAiProvider {
public:
    Result<AiResponse, Error> generate(const AiRequest& request) override {
        last_request = request;
        if (should_fail) {
            return Result<AiResponse, Error>::failure(
                Error(ErrorCode::kAiProviderError, "AI service offline"));
        }
        AiResponse res;
        res.text = response_text;
        res.provider = "mock";
        res.model = "mock-model";
        return Result<AiResponse, Error>::success(res);
    }
    
    [[nodiscard]] std::string provider_name() const noexcept override { return "mock"; }

    AiRequest last_request;
    std::string response_text;
    bool should_fail = false;
};

class MockOcrEngine : public IOcrEngine {
public:
    Result<void, Error> initialize(const std::string&, const std::string&) override {
        initialized = true;
        return Result<void, Error>::success();
    }
    
    Result<std::string, Error> extract_text(const FilePath& path) override {
        last_path = path;
        ocr_called = true;
        return Result<std::string, Error>::success("OCR Mock Text: Sample extracted content from image");
    }
    
    [[nodiscard]] std::string language() const noexcept override { return "eng"; }
    [[nodiscard]] bool is_initialized() const noexcept override { return initialized; }

    bool initialized = true;
    bool ocr_called = false;
    FilePath last_path;
};

// ============================================================================
// Test Cases
// ============================================================================

TEST(AiReasoningEngineTest, SuccessfulParsingOfCleanJson) {
    MockAiProvider ai;
    MockOcrEngine ocr;
    AiReasoningEngine engine(ai, ocr);

    ai.response_text = R"({
        "category": "Receipt",
        "suggested_filename": "2026-07-07_Cafe_Receipt.png",
        "suggested_path": "Receipts/2026/2026-07-07_Cafe_Receipt.png",
        "tags": ["food", "finance"],
        "attributes": {
            "vendor": "Cafe Anti-Gravity",
            "total_amount": "45.20",
            "date": "2026-07-07"
        },
        "confidence": 0.98,
        "reasoning": "OCR text matches transaction total and date"
    })";

    FileEntry entry("C:/docs/receipt.png", 1024, std::chrono::system_clock::now(), std::chrono::system_clock::now());
    entry.set_mime_type("image/png");

    auto result = engine.analyze(entry);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().category, "Receipt");
    EXPECT_EQ(result.value().suggested_filename, "2026-07-07_Cafe_Receipt.png");
    EXPECT_EQ(result.value().suggested_path, "Receipts/2026/2026-07-07_Cafe_Receipt.png");
    EXPECT_EQ(result.value().tags.size(), 2u);
    EXPECT_EQ(result.value().tags[0], "food");
    EXPECT_EQ(result.value().attributes.at("vendor"), "Cafe Anti-Gravity");
    EXPECT_EQ(result.value().confidence, 0.98);
    EXPECT_TRUE(ocr.ocr_called);
}

TEST(AiReasoningEngineTest, SuccessfulParsingOfMarkdownFencedJson) {
    MockAiProvider ai;
    MockOcrEngine ocr;
    AiReasoningEngine engine(ai, ocr);

    // AI output wrapped in markdown code fence blocks
    ai.response_text = R"(```json
    {
        "category": "Invoice",
        "suggested_filename": "invoice_99.pdf",
        "suggested_path": "Invoices/invoice_99.pdf",
        "tags": ["billing"],
        "attributes": {
            "vendor": "DeepMind"
        },
        "confidence": 0.95,
        "reasoning": "Clear Invoice header found"
    }
    ```)";

    FileEntry entry("C:/docs/invoice_99.pdf", 512, std::chrono::system_clock::now(), std::chrono::system_clock::now());
    entry.set_mime_type("application/pdf");

    auto result = engine.analyze(entry);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().category, "Invoice");
    EXPECT_EQ(result.value().suggested_filename, "invoice_99.pdf");
    EXPECT_FALSE(ocr.ocr_called); // Non-image MIME type should not trigger OCR
}

TEST(AiReasoningEngineTest, GracefulFallbackOnAiError) {
    MockAiProvider ai;
    MockOcrEngine ocr;
    AiReasoningEngine engine(ai, ocr);

    ai.should_fail = true; // Trigger mock network failure

    // Use a non-image MIME type so OCR is not triggered, testing pure filename fallback
    FileEntry entry("C:/docs/random_unknown_file.dat", 1024, std::chrono::system_clock::now(), std::chrono::system_clock::now());
    entry.set_mime_type("application/octet-stream");

    auto result = engine.analyze(entry);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().category, "Uncategorized");
    EXPECT_EQ(result.value().suggested_filename, "random_unknown_file.dat");
    EXPECT_EQ(result.value().suggested_path, "Uncategorized/random_unknown_file.dat");
    EXPECT_EQ(result.value().confidence, 0.0);
    EXPECT_TRUE(result.value().reasoning.find("AI generation failed") != std::string::npos);
}

TEST(AiReasoningEngineTest, GracefulFallbackOnCorruptJson) {
    MockAiProvider ai;
    MockOcrEngine ocr;
    AiReasoningEngine engine(ai, ocr);

    ai.response_text = "Corrupt non-JSON response text from buggy LLM.";

    // Use a non-image MIME type so OCR is not triggered, testing pure filename fallback
    FileEntry entry("C:/docs/random_unknown_file.dat", 1024, std::chrono::system_clock::now(), std::chrono::system_clock::now());
    entry.set_mime_type("application/octet-stream");

    auto result = engine.analyze(entry);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().category, "Uncategorized");
    EXPECT_EQ(result.value().suggested_filename, "random_unknown_file.dat");
    EXPECT_EQ(result.value().suggested_path, "Uncategorized/random_unknown_file.dat");
    EXPECT_EQ(result.value().confidence, 0.0);
    EXPECT_TRUE(result.value().reasoning.find("invalid JSON structure") != std::string::npos);
}

}  // namespace lilolify::core::test
