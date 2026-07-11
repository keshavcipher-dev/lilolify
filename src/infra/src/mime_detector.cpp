// ============================================================================
// Lilolify — MIME Type Detector Implementation
// ============================================================================

#include <lilolify/infra/filesystem/mime_detector.hpp>

#include <algorithm>
#include <fstream>

namespace lilolify::infra {

// ============================================================================
// Construction
// ============================================================================

MimeDetector::MimeDetector() {
    init_extension_map();
    init_magic_signatures();
}

// ============================================================================
// Extension-Based Detection
// ============================================================================

std::string MimeDetector::detect_from_extension(std::string_view extension) const {
    if (extension.empty()) {
        return std::string(kUnknownMime);
    }

    // Normalize: ensure lowercase and starts with dot
    std::string ext(extension);
    for (auto& ch : ext) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    if (ext[0] != '.') {
        ext = "." + ext;
    }

    auto it = extension_map_.find(ext);
    if (it != extension_map_.end()) {
        return it->second;
    }
    return std::string(kUnknownMime);
}

// ============================================================================
// Magic Byte Detection
// ============================================================================

std::string MimeDetector::detect_from_file(const core::FilePath& path) const {
    // Try magic bytes first (more accurate)
    auto header = read_header(path, 16);
    if (!header.empty()) {
        auto mime = match_magic_bytes(header);
        if (!mime.empty()) {
            return mime;
        }
    }

    // Fall back to extension-based detection
    auto ext = path.extension().string();
    return detect_from_extension(ext);
}

// ============================================================================
// MIME Type Classification
// ============================================================================

bool MimeDetector::is_image_mime(std::string_view mime) noexcept {
    return mime.starts_with("image/");
}

bool MimeDetector::is_document_mime(std::string_view mime) noexcept {
    return mime == "application/pdf" ||
           mime == "application/msword" ||
           mime.starts_with("application/vnd.openxmlformats") ||
           mime.starts_with("application/vnd.ms-");
}

// ============================================================================
// Private: Extension Map
// ============================================================================

void MimeDetector::init_extension_map() {
    // Images
    extension_map_[".jpg"]  = "image/jpeg";
    extension_map_[".jpeg"] = "image/jpeg";
    extension_map_[".png"]  = "image/png";
    extension_map_[".gif"]  = "image/gif";
    extension_map_[".bmp"]  = "image/bmp";
    extension_map_[".webp"] = "image/webp";
    extension_map_[".tiff"] = "image/tiff";
    extension_map_[".tif"]  = "image/tiff";
    extension_map_[".svg"]  = "image/svg+xml";
    extension_map_[".ico"]  = "image/x-icon";
    extension_map_[".heic"] = "image/heic";
    extension_map_[".heif"] = "image/heif";
    extension_map_[".avif"] = "image/avif";
    extension_map_[".raw"]  = "image/x-raw";

    // Documents
    extension_map_[".pdf"]  = "application/pdf";
    extension_map_[".doc"]  = "application/msword";
    extension_map_[".docx"] = "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    extension_map_[".xls"]  = "application/vnd.ms-excel";
    extension_map_[".xlsx"] = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    extension_map_[".ppt"]  = "application/vnd.ms-powerpoint";
    extension_map_[".pptx"] = "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    extension_map_[".odt"]  = "application/vnd.oasis.opendocument.text";
    extension_map_[".ods"]  = "application/vnd.oasis.opendocument.spreadsheet";
    extension_map_[".odp"]  = "application/vnd.oasis.opendocument.presentation";
    extension_map_[".txt"]  = "text/plain";
    extension_map_[".csv"]  = "text/csv";
    extension_map_[".rtf"]  = "application/rtf";
    extension_map_[".md"]   = "text/markdown";

    // Archives
    extension_map_[".zip"]  = "application/zip";
    extension_map_[".rar"]  = "application/vnd.rar";
    extension_map_[".7z"]   = "application/x-7z-compressed";
    extension_map_[".tar"]  = "application/x-tar";
    extension_map_[".gz"]   = "application/gzip";

    // Audio
    extension_map_[".mp3"]  = "audio/mpeg";
    extension_map_[".wav"]  = "audio/wav";
    extension_map_[".flac"] = "audio/flac";
    extension_map_[".ogg"]  = "audio/ogg";
    extension_map_[".aac"]  = "audio/aac";
    extension_map_[".m4a"]  = "audio/mp4";

    // Video
    extension_map_[".mp4"]  = "video/mp4";
    extension_map_[".avi"]  = "video/x-msvideo";
    extension_map_[".mkv"]  = "video/x-matroska";
    extension_map_[".mov"]  = "video/quicktime";
    extension_map_[".wmv"]  = "video/x-ms-wmv";
    extension_map_[".webm"] = "video/webm";
    extension_map_[".flv"]  = "video/x-flv";

    // Web
    extension_map_[".html"] = "text/html";
    extension_map_[".htm"]  = "text/html";
    extension_map_[".css"]  = "text/css";
    extension_map_[".js"]   = "application/javascript";
    extension_map_[".json"] = "application/json";
    extension_map_[".xml"]  = "application/xml";

    // Programming
    extension_map_[".cpp"]  = "text/x-c++src";
    extension_map_[".hpp"]  = "text/x-c++hdr";
    extension_map_[".c"]    = "text/x-csrc";
    extension_map_[".h"]    = "text/x-chdr";
    extension_map_[".py"]   = "text/x-python";
    extension_map_[".java"] = "text/x-java-source";
    extension_map_[".rs"]   = "text/x-rust";
    extension_map_[".go"]   = "text/x-go";
}

// ============================================================================
// Private: Magic Byte Signatures
// ============================================================================

void MimeDetector::init_magic_signatures() {
    // JPEG: FF D8 FF
    magic_signatures_.push_back({
        "image/jpeg",
        {0xFF, 0xD8, 0xFF},
        0
    });

    // PNG: 89 50 4E 47 0D 0A 1A 0A
    magic_signatures_.push_back({
        "image/png",
        {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A},
        0
    });

    // GIF87a / GIF89a: 47 49 46 38
    magic_signatures_.push_back({
        "image/gif",
        {0x47, 0x49, 0x46, 0x38},
        0
    });

    // BMP: 42 4D
    magic_signatures_.push_back({
        "image/bmp",
        {0x42, 0x4D},
        0
    });

    // WEBP: RIFF....WEBP (bytes 0-3 = RIFF, bytes 8-11 = WEBP)
    // We check the RIFF header first
    magic_signatures_.push_back({
        "image/webp",
        {0x52, 0x49, 0x46, 0x46},  // "RIFF" — partial check
        0
    });

    // TIFF (little-endian): 49 49 2A 00
    magic_signatures_.push_back({
        "image/tiff",
        {0x49, 0x49, 0x2A, 0x00},
        0
    });

    // TIFF (big-endian): 4D 4D 00 2A
    magic_signatures_.push_back({
        "image/tiff",
        {0x4D, 0x4D, 0x00, 0x2A},
        0
    });

    // PDF: 25 50 44 46 (%PDF)
    magic_signatures_.push_back({
        "application/pdf",
        {0x25, 0x50, 0x44, 0x46},
        0
    });

    // ZIP / DOCX / XLSX / PPTX: 50 4B 03 04 (PK..)
    // Note: DOCX/XLSX/PPTX are ZIP files with specific internal structure.
    // We detect as ZIP here; deeper inspection requires unzipping (future).
    magic_signatures_.push_back({
        "application/zip",
        {0x50, 0x4B, 0x03, 0x04},
        0
    });

    // OLE2 (DOC, XLS, PPT): D0 CF 11 E0 A1 B1 1A E1
    magic_signatures_.push_back({
        "application/msword",
        {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1},
        0
    });

    // RAR: 52 61 72 21 1A 07 (Rar!)
    magic_signatures_.push_back({
        "application/vnd.rar",
        {0x52, 0x61, 0x72, 0x21, 0x1A, 0x07},
        0
    });

    // 7-Zip: 37 7A BC AF 27 1C
    magic_signatures_.push_back({
        "application/x-7z-compressed",
        {0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C},
        0
    });

    // GZip: 1F 8B
    magic_signatures_.push_back({
        "application/gzip",
        {0x1F, 0x8B},
        0
    });

    // MP3 (ID3 tag): 49 44 33 (ID3)
    magic_signatures_.push_back({
        "audio/mpeg",
        {0x49, 0x44, 0x33},
        0
    });

    // MP3 (sync word): FF FB or FF F3 or FF F2
    magic_signatures_.push_back({
        "audio/mpeg",
        {0xFF, 0xFB},
        0
    });

    // Sort signatures by length (longest first) for best match accuracy
    std::sort(magic_signatures_.begin(), magic_signatures_.end(),
              [](const MagicSignature& a, const MagicSignature& b) {
                  return a.bytes.size() > b.bytes.size();
              });
}

// ============================================================================
// Private: File Header Reading
// ============================================================================

std::vector<std::uint8_t> MimeDetector::read_header(
    const core::FilePath& path,
    std::size_t max_bytes) const {

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    std::vector<std::uint8_t> buffer(max_bytes);
    file.read(reinterpret_cast<char*>(buffer.data()),
              static_cast<std::streamsize>(max_bytes));

    auto bytes_read = static_cast<std::size_t>(file.gcount());
    buffer.resize(bytes_read);
    return buffer;
}

// ============================================================================
// Private: Magic Byte Matching
// ============================================================================

std::string MimeDetector::match_magic_bytes(
    const std::vector<std::uint8_t>& header) const {

    for (const auto& sig : magic_signatures_) {
        // Check if header is long enough for this signature
        if (header.size() < sig.offset + sig.bytes.size()) {
            continue;
        }

        // Compare bytes at the specified offset
        bool match = std::equal(
            sig.bytes.begin(), sig.bytes.end(),
            header.begin() + static_cast<std::ptrdiff_t>(sig.offset));

        if (match) {
            // Special case: WEBP needs additional check at offset 8
            if (sig.mime_type == "image/webp" && header.size() >= 12) {
                if (header[8] != 'W' || header[9] != 'E' ||
                    header[10] != 'B' || header[11] != 'P') {
                    continue;  // RIFF but not WEBP
                }
            }
            return sig.mime_type;
        }
    }

    return {};  // No match found
}

}  // namespace lilolify::infra
