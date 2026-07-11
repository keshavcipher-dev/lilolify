// ============================================================================
// Lilolify — PdfMetadataExtractor Implementation
// ============================================================================

#include <lilolify/infra/metadata/pdf_metadata_extractor.hpp>

#include <algorithm>
#include <fstream>
#include <regex>
#include <string>
#include <vector>

namespace lilolify::infra {

bool PdfMetadataExtractor::supports(
    const std::string& mime_type,
    const std::string& extension) const {

    return mime_type == "application/pdf" || extension == ".pdf";
}

core::Result<core::FileMetadata, core::Error> PdfMetadataExtractor::extract(
    const core::FilePath& path) {

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return core::Result<core::FileMetadata, core::Error>::failure(
            core::Error(core::ErrorCode::kFileNotFound,
                        "Could not open PDF file: " + path.string()));
    }

    std::streamsize file_size = file.tellg();
    if (file_size < 10) {
        return core::Result<core::FileMetadata, core::Error>::failure(
            core::Error(core::ErrorCode::kFileReadError,
                        "PDF file too small: " + path.string()));
    }

    // Read blocks from the end of the file first (most PDFs put catalog and tree near the end)
    // We'll read up to 64KB from the end of the file.
    std::streamsize read_size = std::min(file_size, static_cast<std::streamsize>(64 * 1024));
    file.seekg(file_size - read_size);

    std::vector<char> buffer(static_cast<std::size_t>(read_size));
    file.read(buffer.data(), read_size);
    if (file.gcount() < read_size) {
        return core::Result<core::FileMetadata, core::Error>::failure(
            core::Error(core::ErrorCode::kFileReadError,
                        "Failed to read trailing block of PDF: " + path.string()));
    }

    std::string content(buffer.data(), buffer.size());

    // Strategy 1: Regular expression matching the /Pages tree count
    // PDF format: /Type /Pages /Count X or /Type/Pages/Count X
    // We match: /Type\s*/Pages.*?/Count\s*(\d+)  (using non-greedy scan)
    // Or we scan for /Count\s*(\d+) and look for /Type\s*/Pages in proximity.
    // Let's use simple string search first to find "/Type" and "/Pages", then extract "/Count".
    // Alternatively, let's write a simple scan:

    std::size_t offset = 0;
    std::uint32_t page_count = 0;
    bool found = false;

    // Search backwards in our read trailing block
    while (true) {
        auto type_pos = content.find("/Type", offset);
        if (type_pos == std::string::npos) {
            break;
        }

        // Check if `/Pages` is nearby (within 30 characters of `/Type`)
        auto pages_pos = content.find("/Pages", type_pos);
        if (pages_pos != std::string::npos && (pages_pos - type_pos) < 30) {
            // Found a Pages dictionary. Now look for "/Count" in a window of 256 bytes after type_pos
            auto count_pos = content.find("/Count", type_pos);
            if (count_pos != std::string::npos && (count_pos - type_pos) < 256) {
                // Parse the digits following "/Count"
                auto digit_start = count_pos + 6; // length of "/Count"
                while (digit_start < content.size() &&
                       (content[digit_start] == ' ' || content[digit_start] == '\t' ||
                        content[digit_start] == '\r' || content[digit_start] == '\n')) {
                    digit_start++;
                }

                auto digit_end = digit_start;
                while (digit_end < content.size() && std::isdigit(static_cast<unsigned char>(content[digit_end]))) {
                    digit_end++;
                }

                if (digit_end > digit_start) {
                    std::string count_str = content.substr(digit_start, digit_end - digit_start);
                    try {
                        page_count = static_cast<std::uint32_t>(std::stoul(count_str));
                        found = true;
                    } catch (...) {
                        // ignore parse errors and keep looking
                    }
                }
            }
        }
        offset = type_pos + 5;
    }

    // Strategy 2: If not found in trailing block, do a simple regex scan for "/Count" in the trailer
    // or fall back to linear scan of the whole file if the file is small (< 1MB)
    if (!found && file_size < 2 * 1024 * 1024) {
        file.seekg(0);
        std::vector<char> full_buffer(static_cast<std::size_t>(file_size));
        file.read(full_buffer.data(), file_size);
        std::string full_content(full_buffer.data(), full_buffer.size());

        offset = 0;
        while (true) {
            auto type_pos = full_content.find("/Type", offset);
            if (type_pos == std::string::npos) {
                break;
            }

            auto pages_pos = full_content.find("/Pages", type_pos);
            if (pages_pos != std::string::npos && (pages_pos - type_pos) < 30) {
                auto count_pos = full_content.find("/Count", type_pos);
                if (count_pos != std::string::npos && (count_pos - type_pos) < 256) {
                    auto digit_start = count_pos + 6;
                    while (digit_start < full_content.size() &&
                           (full_content[digit_start] == ' ' || full_content[digit_start] == '\t' ||
                            full_content[digit_start] == '\r' || full_content[digit_start] == '\n')) {
                        digit_start++;
                    }

                    auto digit_end = digit_start;
                    while (digit_end < full_content.size() && std::isdigit(static_cast<unsigned char>(full_content[digit_end]))) {
                        digit_end++;
                    }

                    if (digit_end > digit_start) {
                        std::string count_str = full_content.substr(digit_start, digit_end - digit_start);
                        try {
                            page_count = static_cast<std::uint32_t>(std::stoul(count_str));
                            found = true;
                            break; // Stop on first valid root pages tree
                        } catch (...) {
                            // keep looking
                        }
                    }
                }
            }
            offset = type_pos + 5;
        }
    }

    if (!found) {
        // Many simplified PDFs might just have a `/Count X` entry at the root dictionary or catalog.
        // We look for any `/Count \d+` in the trailing block as a fallback.
        std::regex count_fallback(R"(/Count\s+(\d+))", std::regex::ECMAScript);
        std::smatch match;
        if (std::regex_search(content, match, count_fallback)) {
            try {
                page_count = static_cast<std::uint32_t>(std::stoul(match[1].str()));
                found = true;
            } catch (...) {
                // ignore
            }
        }
    }

    if (!found) {
        return core::Result<core::FileMetadata, core::Error>::failure(
            core::Error(core::ErrorCode::kOcrProcessingError,
                        "Could not parse PDF page count from structure: " + path.string()));
    }

    core::FileMetadata metadata;
    metadata.page_count = page_count;

    return core::Result<core::FileMetadata, core::Error>::success(std::move(metadata));
}

}  // namespace lilolify::infra
