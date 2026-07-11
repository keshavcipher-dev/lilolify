// ============================================================================
// Lilolify — MockOcrEngine
// ============================================================================
// Concrete implementation of IOcrEngine for tests or builds without Tesseract.
// ============================================================================

#pragma once

#include <lilolify/core/interfaces/i_ocr_engine.hpp>

#include <filesystem>

namespace lilolify::infra {

using core::Result;

/// Mock OCR Engine that does not require Tesseract library at runtime.
/// Simulates successful text extraction from images.
class MockOcrEngine : public core::IOcrEngine {
public:
    MockOcrEngine() noexcept : initialized_(false), lang_("eng") {}
    ~MockOcrEngine() override = default;

    Result<void, core::Error> initialize(
        const std::string& tessdata_path,
        const std::string& language) override {

        if (language.empty()) {
            return Result<void, core::Error>::failure(
                core::Error(core::ErrorCode::kOcrInitError, "Language code is empty"));
        }

        // Just check if folder path is not empty
        if (tessdata_path.empty()) {
            return Result<void, core::Error>::failure(
                core::Error(core::ErrorCode::kOcrInitError, "tessdata folder path is empty"));
        }

        lang_ = language;
        initialized_ = true;
        return Result<void, core::Error>::success();
    }

    [[nodiscard]] Result<std::string, core::Error> extract_text(
        const core::FilePath& image_path) override {

        if (!initialized_) {
            return Result<std::string, core::Error>::failure(
                core::Error(core::ErrorCode::kOcrProcessingError, "Mock OCR engine not initialized"));
        }

        // Check if file exists to simulate IO error
        std::error_code ec;
        if (!std::filesystem::exists(image_path, ec) || ec) {
            return Result<std::string, core::Error>::failure(
                core::Error(core::ErrorCode::kFileNotFound,
                            "Image file not found: " + image_path.string()));
        }

        // Simulate reading text from image based on its filename
        auto filename = image_path.filename().string();

        if (filename.find("receipt") != std::string::npos) {
            return Result<std::string, core::Error>::success(
                "TOTAL USD $42.50\n"
                "ITEMS: Coffee, Sandwich\n"
                "DATE: 2026-07-07\n"
                "MERCHANT: Cafe Anti-Gravity\n");
        }

        if (filename.find("invoice") != std::string::npos) {
            return Result<std::string, core::Error>::success(
                "INVOICE #99812\n"
                "AMOUNT DUE: $1200.00\n"
                "COMPANY: Google DeepMind\n"
                "DUE DATE: 2026-08-01\n");
        }

        if (filename.find("id_card") != std::string::npos) {
            return Result<std::string, core::Error>::success(
                "IDENTITY CARD\n"
                "NAME: John Doe\n"
                "ID NUMBER: 12345-67890\n"
                "EXPIRY: 2030-01-01\n");
        }

        return Result<std::string, core::Error>::success(
            "Mock OCR text extracted from file: " + filename);
    }

    [[nodiscard]] std::string language() const noexcept override {
        return lang_;
    }

    [[nodiscard]] bool is_initialized() const noexcept override {
        return initialized_;
    }

private:
    bool initialized_;
    std::string lang_;
};

}  // namespace lilolify::infra
