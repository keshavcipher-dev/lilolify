// ============================================================================
// Lilolify — AiReasoningEngine Implementation
// ============================================================================

#include <lilolify/core/services/ai_reasoning_engine.hpp>

#include <nlohmann/json.hpp>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <vector>

using json = nlohmann::json;

namespace lilolify::core {

namespace {

// Helper to strip markdown code blocks like ```json ... ```
std::string strip_markdown_fences(const std::string& input) {
    std::string text = input;
    
    // Trim leading whitespace
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    
    // Trim trailing whitespace
    text.erase(std::find_if(text.rbegin(), text.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), text.end());

    // Strip starting ```json or ```
    if (text.rfind("```json", 0) == 0) {
        text = text.substr(7);
    } else if (text.rfind("```", 0) == 0) {
        text = text.substr(3);
    }

    // Strip ending ```
    if (text.size() >= 3 && text.compare(text.size() - 3, 3, "```") == 0) {
        text = text.substr(0, text.size() - 3);
    }

    // Trim again
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    text.erase(std::find_if(text.rbegin(), text.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), text.end());

    return text;
}

std::string base64_encode(const std::vector<std::uint8_t>& data) {
    static const char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    
    int val = 0, valb = -6;
    for (std::uint8_t c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(lookup[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        out.push_back(lookup[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (out.size() % 4) {
        out.push_back('=');
    }
    return out;
}

}  // namespace

// ============================================================================
// Constructor
// ============================================================================

AiReasoningEngine::AiReasoningEngine(
    IAiProvider& ai_provider,
    IOcrEngine& ocr_engine) noexcept
    : ai_provider_(ai_provider), ocr_engine_(ocr_engine) {}

// ============================================================================
// Core Analysis Pipeline
// ============================================================================

Result<OrganizationDecision, Error> AiReasoningEngine::analyze(const FileEntry& entry) {
    std::string extracted_text;

    // 1. If it's an image, attempt to run OCR to capture textual properties
    if (entry.mime_type().rfind("image/", 0) == 0) {
        if (ocr_engine_.is_initialized()) {
            auto ocr_res = ocr_engine_.extract_text(entry.path());
            if (ocr_res.has_value()) {
                extracted_text = ocr_res.value();
            }
        }
    }

    // 2. If standard text metadata preview is already populated in Phase 3, append it
    if (entry.metadata().has_value() && !entry.metadata().value().text_preview.empty()) {
        if (!extracted_text.empty()) {
            extracted_text += "\n";
        }
        extracted_text += entry.metadata().value().text_preview;
    }

    // 3. Build prompts
    std::string system_prompt = build_system_prompt();
    std::string user_prompt = build_user_prompt(entry, extracted_text);

    // 4. Query AI model
    AiRequest ai_req;
    ai_req.messages.push_back({"system", system_prompt});

    AiMessage user_msg;
    user_msg.role = "user";
    user_msg.text_content = user_prompt;

    // If it's an image, also attach base64 image data for Multimodal Vision capability
    if (entry.mime_type().rfind("image/", 0) == 0) {
        std::ifstream file(entry.path().string(), std::ios::binary);
        if (file.is_open()) {
            std::vector<std::uint8_t> file_bytes(
                (std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>()
            );
            if (!file_bytes.empty()) {
                user_msg.image_base64 = base64_encode(file_bytes);
                user_msg.image_mime_type = entry.mime_type();
            }
        }
    }
    ai_req.messages.push_back(user_msg);

    auto ai_res = ai_provider_.generate(ai_req);

    // 5. Fallback if the remote AI service failed (e.g. rate limit / network error)
    if (ai_res.has_error()) {
        return Result<OrganizationDecision, Error>::success(
            make_fallback_decision(entry, "AI generation failed: " + std::string(ai_res.error().message()), extracted_text));
    }

    // 6. Parse JSON payload recommendation
    auto decision_res = parse_ai_json(ai_res.value().text);
    if (decision_res.has_error()) {
        // Fallback on JSON parsing error
        return Result<OrganizationDecision, Error>::success(
            make_fallback_decision(entry, "AI returned invalid JSON structure: " + std::string(decision_res.error().message()) +
                                          ". Raw response: " + ai_res.value().text, extracted_text));
    }

    // Ensure suggested filename defaults to original filename if left empty
    auto decision = decision_res.value();
    if (decision.suggested_filename.empty()) {
        decision.suggested_filename = entry.path().filename().string();
    }

    return Result<OrganizationDecision, Error>::success(std::move(decision));
}

// ============================================================================
// Prompts Formatting
// ============================================================================

std::string AiReasoningEngine::build_system_prompt() const {
    return "You are an expert digital file organization assistant named Lilolify.\n"
           "Your job is to analyze file metadata and contents, and recommend how to classify and organize the file.\n"
           "You MUST respond ONLY with a raw JSON object matching the following structure. Do not output conversational text outside the JSON.\n\n"
           "JSON Schema:\n"
           "{\n"
           "  \"category\": \"string (e.g. Invoice, Receipt, ID_Card, Document, Image, Code, Unknown)\",\n"
           "  \"suggested_filename\": \"string (a clean, descriptive name, e.g. YYYY-MM-DD_Vendor_DocType.ext)\",\n"
           "  \"suggested_path\": \"string (relative directory classification path, e.g. Invoices/2026/file.pdf)\",\n"
           "  \"tags\": [\"array of strings\"],\n"
           "  \"attributes\": {\n"
           "    \"vendor\": \"string (if applicable)\",\n"
           "    \"date\": \"string (if applicable, YYYY-MM-DD)\",\n"
           "    \"total_amount\": \"string (if applicable, e.g. 42.50)\",\n"
           "    \"subject\": \"string (general subject summary)\"\n"
           "  },\n"
           "  \"confidence\": 0.95 (float value between 0.0 and 1.0),\n"
           "  \"reasoning\": \"string (brief explanation for this organization recommendation)\"\n"
           "}";
}

std::string AiReasoningEngine::build_user_prompt(
    const FileEntry& entry,
    const std::string& extra_text) const {

    std::ostringstream oss;
    oss << "File Details:\n"
        << "- Original Path: " << entry.path().string() << "\n"
        << "- File Extension: " << entry.extension() << "\n"
        << "- MIME Type: " << entry.mime_type() << "\n"
        << "- File Size: " << entry.size_bytes() << " bytes\n";

    if (entry.metadata().has_value()) {
        const auto& meta = entry.metadata().value();
        if (meta.image_width.has_value() && meta.image_height.has_value() &&
            *meta.image_width > 0 && *meta.image_height > 0) {
            oss << "- Image Dimensions: " << *meta.image_width << "x" << *meta.image_height << "\n";
        }
        if (meta.page_count.has_value() && *meta.page_count > 0) {
            oss << "- PDF Pages: " << *meta.page_count << "\n";
        }
        if (!meta.sha256.empty()) {
            oss << "- SHA-256: " << meta.sha256 << "\n";
        }
    }

    if (!extra_text.empty()) {
        oss << "\nExtracted Content Snippet/OCR:\n"
            << "========================================\n"
            << extra_text << "\n"
            << "========================================\n";
    }

    oss << "\nPlease analyze this file and return the classification JSON.";
    return oss.str();
}

// ============================================================================
// JSON Parsing
// ============================================================================

Result<OrganizationDecision, Error> AiReasoningEngine::parse_ai_json(
    const std::string& ai_json_text) const {

    std::string cleaned = strip_markdown_fences(ai_json_text);

    try {
        json j = json::parse(cleaned);

        OrganizationDecision decision;
        decision.category = j.value("category", "Unknown");
        decision.suggested_filename = j.value("suggested_filename", "");
        decision.suggested_path = j.value("suggested_path", "");

        if (j.contains("tags") && j["tags"].is_array()) {
            for (const auto& item : j["tags"]) {
                if (item.is_string()) {
                    decision.tags.push_back(item.get<std::string>());
                }
            }
        }

        if (j.contains("attributes") && j["attributes"].is_object()) {
            for (auto it = j["attributes"].begin(); it != j["attributes"].end(); ++it) {
                if (it.value().is_string()) {
                    decision.attributes[it.key()] = it.value().get<std::string>();
                }
            }
        }

        decision.confidence = j.value("confidence", 0.5);
        decision.reasoning = j.value("reasoning", "");

        return Result<OrganizationDecision, Error>::success(std::move(decision));

    } catch (const json::parse_error& e) {
        return Result<OrganizationDecision, Error>::failure(
            Error(ErrorCode::kConfigParseError, "JSON parse failure: " + std::string(e.what())));
    } catch (const std::exception& e) {
        return Result<OrganizationDecision, Error>::failure(
            Error(ErrorCode::kAiProviderError, "Unexpected error populating decision: " + std::string(e.what())));
    }
}

// ============================================================================
// Fallback Generator
// ============================================================================

OrganizationDecision AiReasoningEngine::make_fallback_decision(
    const FileEntry& entry,
    const std::string& reasoning_reason,
    const std::string& extracted_text) const {

    OrganizationDecision decision;
    decision.suggested_filename = entry.path().filename().string();
    decision.confidence = 0.1;
    decision.reasoning = "Heuristic fallback: " + reasoning_reason;

    // Convert filename to lowercase for matching
    std::string filename_lower = decision.suggested_filename;
    std::transform(filename_lower.begin(), filename_lower.end(), filename_lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Also convert OCR/metadata extracted text to lowercase for content-aware matching
    std::string content_lower = extracted_text;
    std::transform(content_lower.begin(), content_lower.end(), content_lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    bool is_image = entry.mime_type().rfind("image/", 0) == 0;

    // Helper lambda: search for a keyword in content (and filename if not an image)
    auto has_keyword = [&](const std::string& keyword) -> bool {
        if (is_image) {
            // For images, focus EXCLUSIVELY on the content (OCR extracted text)
            return content_lower.find(keyword) != std::string::npos;
        } else {
            // For other files, fallback to both filename and text content
            return filename_lower.find(keyword) != std::string::npos ||
                   content_lower.find(keyword) != std::string::npos;
        }
    };

    // Apply keyword matcher rules against filename + OCR text
    if (has_keyword("datesheet") || has_keyword("schedule") ||
        has_keyword("timetable") || has_keyword("semester")) {
        decision.category = "Schedules";
        decision.suggested_path = "Schedules/" + decision.suggested_filename;
        decision.tags = {"schedules", "academic"};
        if (!extracted_text.empty()) decision.confidence = 0.6;
    } else if (has_keyword("cgl") || has_keyword("ssc") ||
               has_keyword("question") || has_keyword("ques") ||
               has_keyword("exam") || has_keyword("test paper") ||
               has_keyword("quiz") || has_keyword("mcq") ||
               has_keyword("assignment") || has_keyword("homework") ||
               has_keyword("syllabus") || has_keyword("study") ||
               has_keyword("lecture") || has_keyword("practice paper")) {
        decision.category = "Academic";
        decision.suggested_path = "Academic/" + decision.suggested_filename;
        decision.tags = {"academic", "education", "study"};
        if (!extracted_text.empty()) decision.confidence = 0.6;
    } else if (has_keyword("invoice") || has_keyword("bill") ||
               has_keyword("receipt") || has_keyword("payment") ||
               has_keyword("transaction") || has_keyword("amount due") ||
               has_keyword("total amount") || has_keyword("gst") ||
               has_keyword("tax invoice")) {
        decision.category = "Financial";
        decision.suggested_path = "Financial/" + decision.suggested_filename;
        decision.tags = {"billing", "invoice"};
        if (!extracted_text.empty()) decision.confidence = 0.6;
    } else if (has_keyword("passport") || has_keyword("id_card") ||
               has_keyword("license") || has_keyword("identity") ||
               has_keyword("adhaar") || has_keyword("aadhaar") ||
               has_keyword("aadhar") || has_keyword("unique identification") ||
               has_keyword("pan") || has_keyword("permanent account") ||
               has_keyword("family") || has_keyword("voter") ||
               has_keyword("election commission") || has_keyword("driving licence") ||
               has_keyword("government of india") || has_keyword("date of birth") ||
               has_keyword("card")) {
        decision.category = "Identification";
        decision.suggested_path = "Identification/" + decision.suggested_filename;
        decision.tags = {"personal", "identity"};
        if (!extracted_text.empty()) decision.confidence = 0.6;
    } else if (has_keyword("resume") || has_keyword("curriculum vitae") ||
               has_keyword("cv") || has_keyword("portfolio") ||
               has_keyword("certificate") || has_keyword("degree") ||
               has_keyword("diploma") || has_keyword("transcript") ||
               has_keyword("marksheet") || has_keyword("grade") ||
               has_keyword("university") || has_keyword("awarded")) {
        decision.category = "Credentials";
        decision.suggested_path = "Credentials/" + decision.suggested_filename;
        decision.tags = {"academic", "career", "credentials"};
        if (!extracted_text.empty()) decision.confidence = 0.6;
    } else if (has_keyword("note") || has_keyword("memo") ||
               has_keyword("diary") || has_keyword("journal")) {
        decision.category = "Notes";
        decision.suggested_path = "Notes/" + decision.suggested_filename;
        decision.tags = {"personal", "notes"};
        if (!extracted_text.empty()) decision.confidence = 0.6;
    } else if (has_keyword("prescription") || has_keyword("diagnosis") ||
               has_keyword("medical") || has_keyword("hospital") ||
               has_keyword("patient") || has_keyword("dr.") ||
               has_keyword("clinic") || has_keyword("health")) {
        decision.category = "Medical";
        decision.suggested_path = "Medical/" + decision.suggested_filename;
        decision.tags = {"health", "medical"};
        if (!extracted_text.empty()) decision.confidence = 0.6;
    } else if (has_keyword("agreement") || has_keyword("contract") ||
               has_keyword("terms and conditions") || has_keyword("legal") ||
               has_keyword("affidavit") || has_keyword("notarized")) {
        decision.category = "Legal";
        decision.suggested_path = "Legal/" + decision.suggested_filename;
        decision.tags = {"legal", "documents"};
        if (!extracted_text.empty()) decision.confidence = 0.6;
    } else if (has_keyword("photo") || has_keyword("image") ||
               has_keyword("screenshot") || has_keyword("selfie") ||
               has_keyword("wallpaper")) {
        decision.category = "Media";
        decision.suggested_path = "Media/" + decision.suggested_filename;
        decision.tags = {"graphics", "image"};
    } else {
        decision.category = "Uncategorized";
        decision.suggested_path = "Uncategorized/" + decision.suggested_filename;
        decision.tags = {"uncategorized"};
        decision.confidence = 0.0;
    }

    // Append OCR evidence snippet to reasoning if content was used for classification
    if (!extracted_text.empty() && decision.category != "Uncategorized") {
        std::string snippet = extracted_text.substr(0, 200);
        decision.reasoning += " | OCR content matched: \"" + snippet + "...\"";
    }

    return decision;
}

}  // namespace lilolify::core
