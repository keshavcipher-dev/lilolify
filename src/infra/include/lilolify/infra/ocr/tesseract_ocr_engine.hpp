// ============================================================================
// Lilolify — TesseractOcrEngine
// ============================================================================
// Concrete implementation of IOcrEngine using Tesseract OCR.
// ============================================================================

#pragma once

#include <lilolify/core/interfaces/i_ocr_engine.hpp>

// Forward declarations of Tesseract / Leptonica types to avoid leaking headers
namespace tesseract {
class TessBaseAPI;
}

namespace lilolify::infra {

/// Specialized OCR engine wrapper around Tesseract and Leptonica APIs.
/// Provides full local printed character scanning capability.
class TesseractOcrEngine : public core::IOcrEngine {
public:
    TesseractOcrEngine() noexcept;
    ~TesseractOcrEngine() override;

    // Prevent copy/move due to internal Tesseract pointer management
    TesseractOcrEngine(const TesseractOcrEngine&) = delete;
    TesseractOcrEngine& operator=(const TesseractOcrEngine&) = delete;
    TesseractOcrEngine(TesseractOcrEngine&&) = delete;
    TesseractOcrEngine& operator=(TesseractOcrEngine&&) = delete;

    /// Initialize the Tesseract wrapper.
    ///
    /// @param tessdata_path  Path containing language trained models (.traineddata)
    /// @param language       ISO language code (e.g. "eng")
    core::Result<void, core::Error> initialize(
        const std::string& tessdata_path,
        const std::string& language) override;

    /// Extract text using Leptonica file reader and Tesseract scanner.
    [[nodiscard]] core::Result<std::string, core::Error> extract_text(
        const core::FilePath& image_path) override;

    [[nodiscard]] std::string language() const noexcept override;
    [[nodiscard]] bool is_initialized() const noexcept override;

private:
    tesseract::TessBaseAPI* api_;
    bool initialized_;
    std::string lang_;
};

}  // namespace lilolify::infra
