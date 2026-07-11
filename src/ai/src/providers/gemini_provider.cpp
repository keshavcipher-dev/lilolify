// ============================================================================
// Lilolify — GeminiProvider Implementation
// ============================================================================

#include <lilolify/ai/providers/gemini_provider.hpp>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace lilolify::ai {

GeminiProvider::GeminiProvider(
    core::IHttpClient& client,
    std::string api_key,
    std::string model)
    : client_(client), api_key_(std::move(api_key)), default_model_(std::move(model)) {}

core::Result<core::AiResponse, core::Error> GeminiProvider::generate(
    const core::AiRequest& request) {

    if (api_key_.empty()) {
        return core::Result<core::AiResponse, core::Error>::failure(
            core::Error(core::ErrorCode::kAiProviderError, "Gemini API Key is empty"));
    }

    std::string target_model = request.model.empty() ? default_model_ : request.model;

    // Build JSON Payload matching Google's Gemini API
    json payload;
    json contents_arr = json::array();

    for (const auto& msg : request.messages) {
        if (msg.role == "system") {
            // System instructions are set globally at root in Gemini API
            json sys_instruction;
            sys_instruction["parts"] = json::array({{{"text", msg.text_content}}});
            payload["systemInstruction"] = sys_instruction;
            continue;
        }

        json content_obj;
        // Map roles: "assistant" -> "model", "user" -> "user"
        content_obj["role"] = (msg.role == "assistant") ? "model" : "user";

        json parts_arr = json::array();
        
        // Text prompt part
        parts_arr.push_back({{"text", msg.text_content}});

        // Optional image part
        if (!msg.image_base64.empty()) {
            std::string mime = msg.image_mime_type.empty() ? "image/jpeg" : msg.image_mime_type;
            parts_arr.push_back({
                {"inlineData", {
                    {"mimeType", mime},
                    {"data", msg.image_base64}
                }}
            });
        }

        content_obj["parts"] = parts_arr;
        contents_arr.push_back(content_obj);
    }
    payload["contents"] = contents_arr;

    // Configure generation settings
    json config;
    config["temperature"] = request.temperature;
    config["maxOutputTokens"] = request.max_tokens;
    payload["generationConfig"] = config;

    std::string request_body = payload.dump();

    // Endpoints include the API key directly in query parameters
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/" +
                      target_model + ":generateContent?key=" + api_key_;

    std::vector<std::string> headers = {
        "Content-Type: application/json"
    };

    auto post_result = client_.post(url, request_body, headers);
    if (post_result.has_error()) {
        return core::Result<core::AiResponse, core::Error>::failure(std::move(post_result).error());
    }

    try {
        json response_json = json::parse(post_result.value());

        // Check for Gemini API error payload
        if (response_json.contains("error")) {
            std::string err_msg = response_json["error"]["message"].get<std::string>();
            return core::Result<core::AiResponse, core::Error>::failure(
                core::Error(core::ErrorCode::kAiProviderError, "Gemini API Error: " + err_msg));
        }

        core::AiResponse response;

        // Parse extracted text from candidate parts
        auto candidate = response_json["candidates"][0];
        response.text = candidate["content"]["parts"][0]["text"].get<std::string>();

        // Parse metadata tokens
        if (response_json.contains("usageMetadata")) {
            auto usage_json = response_json["usageMetadata"];
            response.usage.prompt_tokens = usage_json.value("promptTokenCount", 0u);
            response.usage.completion_tokens = usage_json.value("candidatesTokenCount", 0u);
            response.usage.total_tokens = usage_json.value("totalTokenCount", 0u);
        }

        response.provider = "gemini";
        response.model = target_model;

        return core::Result<core::AiResponse, core::Error>::success(std::move(response));

    } catch (const json::parse_error& e) {
        return core::Result<core::AiResponse, core::Error>::failure(
            core::Error(core::ErrorCode::kConfigParseError,
                        "Failed to parse Gemini JSON response: " + std::string(e.what())));
    } catch (const std::exception& e) {
        return core::Result<core::AiResponse, core::Error>::failure(
            core::Error(core::ErrorCode::kAiProviderError,
                        "Unexpected error parsing Gemini response: " + std::string(e.what())));
    }
}

std::string GeminiProvider::provider_name() const noexcept {
    return "gemini";
}

}  // namespace lilolify::ai
