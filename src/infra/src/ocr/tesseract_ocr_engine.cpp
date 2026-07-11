// ============================================================================
// Lilolify — TesseractOcrEngine Implementation
// ============================================================================

#include <lilolify/infra/ocr/tesseract_ocr_engine.hpp>

#include <filesystem>

#ifdef LILOLIFY_WITH_TESSERACT
#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>
#endif

namespace lilolify::infra {

using core::Result;

#ifdef LILOLIFY_WITH_TESSERACT

TesseractOcrEngine::TesseractOcrEngine() noexcept
    : api_(new tesseract::TessBaseAPI()), initialized_(false), lang_("eng") {}

TesseractOcrEngine::~TesseractOcrEngine() {
    if (api_) {
        api_->End();
        delete api_;
    }
}

Result<void, core::Error> TesseractOcrEngine::initialize(
    const std::string& tessdata_path,
    const std::string& language) {

    if (!api_) {
        return Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kOcrInitError, "Tesseract API pointer is null"));
    }

    if (language.empty()) {
        return Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kOcrInitError, "Language code is empty"));
    }

    // Hand-check if tessdata folder exists (Tesseract doesn't fail gracefully on empty path)
    if (!tessdata_path.empty() && !std::filesystem::exists(tessdata_path)) {
        return Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kOcrInitError,
                        "tessdata folder path does not exist: " + tessdata_path));
    }

    const char* datapath = tessdata_path.empty() ? nullptr : tessdata_path.c_str();

    // Init returns 0 on success, non-zero on failure.
    int code = api_->Init(datapath, language.c_str());
    if (code != 0) {
        return Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kOcrInitError,
                        "Failed to initialize Tesseract API (code " +
                            std::to_string(code) + ") with language '" + language + "'"));
    }

    lang_ = language;
    initialized_ = true;
    return Result<void, core::Error>::success();
}

Result<std::string, core::Error> TesseractOcrEngine::extract_text(
    const core::FilePath& image_path) {

    if (!initialized_) {
        return Result<std::string, core::Error>::failure(
            core::Error(core::ErrorCode::kOcrProcessingError,
                        "Tesseract OCR engine is not initialized"));
    }

    // Verify file exists
    std::error_code ec;
    if (!std::filesystem::exists(image_path, ec) || ec) {
        return Result<std::string, core::Error>::failure(
            core::Error(core::ErrorCode::kFileNotFound,
                        "Image file not found: " + image_path.string()));
    }

    // Leptonica pixRead returns nullptr on load failure.
    // We wrap it in a std::unique_ptr with custom deleter for safety.
    struct PixDeleter {
        void operator()(Pix* p) const noexcept {
            if (p) pixDestroy(&p);
        }
    };

    std::unique_ptr<Pix, PixDeleter> image(pixRead(image_path.string().c_str()));
    if (!image) {
        return Result<std::string, core::Error>::failure(
            core::Error(core::ErrorCode::kFileReadError,
                        "Leptonica failed to load image: " + image_path.string()));
    }

    api_->SetImage(image.get());

    // GetUTF8Text returns raw malloc'd character pointer. Must be freed by caller using delete[]
    char* text_ptr = api_->GetUTF8Text();
    if (!text_ptr) {
        return Result<std::string, core::Error>::failure(
            core::Error(core::ErrorCode::kOcrProcessingError,
                        "Tesseract failed to extract text from image: " + image_path.string()));
    }

    std::string text(text_ptr);
    delete[] text_ptr;

    return Result<std::string, core::Error>::success(std::move(text));
}

std::string TesseractOcrEngine::language() const noexcept {
    return lang_;
}

bool TesseractOcrEngine::is_initialized() const noexcept {
    return initialized_;
}

#else // LILOLIFY_WITH_TESSERACT not compiled

// Fallback stub implementation of TesseractOcrEngine for tests/lightweight builds
TesseractOcrEngine::TesseractOcrEngine() noexcept
    : api_(nullptr), initialized_(false), lang_("eng") {}

TesseractOcrEngine::~TesseractOcrEngine() = default;

Result<void, core::Error> TesseractOcrEngine::initialize(
    const std::string& /*tessdata_path*/,
    const std::string& language) {

    if (language.empty()) {
        return Result<void, core::Error>::failure(
            core::Error(core::ErrorCode::kOcrInitError, "Language code is empty"));
    }

    lang_ = language;
    initialized_ = true;
    return Result<void, core::Error>::success();
}

Result<std::string, core::Error> TesseractOcrEngine::extract_text(
    const core::FilePath& image_path) {

    if (!initialized_) {
        return Result<std::string, core::Error>::failure(
            core::Error(core::ErrorCode::kOcrProcessingError,
                        "Tesseract OCR engine is not initialized (stub)"));
    }

    std::error_code ec;
    if (!std::filesystem::exists(image_path, ec) || ec) {
        return Result<std::string, core::Error>::failure(
            core::Error(core::ErrorCode::kFileNotFound,
                        "Image file not found (stub): " + image_path.string()));
    }

    return Result<std::string, core::Error>::success(
        "Tesseract OCR stub text extracted from: " + image_path.filename().string());
}

std::string TesseractOcrEngine::language() const noexcept {
    return lang_;
}

bool TesseractOcrEngine::is_initialized() const noexcept {
    return initialized_;
}

#endif // LILOLIFY_WITH_TESSERACT

}  // namespace lilolify::infra
