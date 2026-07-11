// ============================================================================
// Lilolify — IOcrEngine Interface (Port)
// ============================================================================
// Abstract port interface for optical character recognition.
// ============================================================================

#pragma once

#include <lilolify/core/error.hpp>
#include <lilolify/core/result.hpp>
#include <lilolify/core/types.hpp>

#include <string>

namespace lilolify::core {

/// Abstract interface for performing OCR on local files.
/// Separates Tesseract-specific APIs from core domain use cases.
class IOcrEngine {
public:
    virtual ~IOcrEngine() = default;

    IOcrEngine() = default;
    IOcrEngine(const IOcrEngine&) = delete;
    IOcrEngine& operator=(const IOcrEngine&) = delete;
    IOcrEngine(IOcrEngine&&) = delete;
    IOcrEngine& operator=(IOcrEngine&&) = delete;

    /// Initialize the OCR engine.
    ///
    /// @param tessdata_path  Path to the folder containing language files (.traineddata)
    /// @param language       Language code (e.g. "eng", "fra", "deu")
    /// @return Result success, or Error if initialization fails (e.g. invalid tessdata path).
    virtual Result<void, Error> initialize(
        const std::string& tessdata_path,
        const std::string& language) = 0;

    /// Extract text from an image.
    ///
    /// @param image_path  Absolute path to the image file.
    /// @return Hex/UTF-8 text extracted from the image on success, or Error on processing failure.
    [[nodiscard]] virtual Result<std::string, Error> extract_text(
        const FilePath& image_path) = 0;

    /// Returns the current language code configured.
    [[nodiscard]] virtual std::string language() const noexcept = 0;

    /// Returns true if the engine has been successfully initialized.
    [[nodiscard]] virtual bool is_initialized() const noexcept = 0;
};

}  // namespace lilolify::core
