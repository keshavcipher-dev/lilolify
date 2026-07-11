// ============================================================================
// Lilolify — ClaudeProvider Implementation
// ============================================================================

#include <lilolify/ai/providers/claude_provider.hpp>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace lilolify::ai {

ClaudeProvider::ClaudeProvider(
    core::IHttpClient& client,
    std::string api_key,
    std::string model)
    : client_(client), api_key_(std::move(api_key)), default_model_(std::move(model)) {}

core::Result<core::AiResponse, core::Error> ClaudeProvider::generate(
    const core::AiRequest& request) {

    if (api_key_.empty()) {
        return core::Result<core::AiResponse, core::Error>::failure(
            core::Error(core::ErrorCode::kAiProviderError, "Claude API Key is empty"));
    }

    std::string target_model = request.model.empty() ? default_model_ : request.model;

    // Build Anthropic JSON Payload
    json payload;
    payload["model"] = target_model;
    payload["temperature"] = request.temperature;
    payload["max_tokens"] = request.max_tokens;

    json messages_arr = json::array();
    std::string system_prompt;

    for (const auto& msg : request.messages) {
        if (msg.role == "system") {
            // Claude takes system prompt at the top-level "system" property
            system_prompt = msg.text_content;
            continue;
        }

        json msg_obj;
        // Roles: "user", "assistant" are matched directly
        msg_obj["role"] = msg.role;

        // Content array format (required for vision, supported for text too)
        json content_arr = json::array();

        // Text prompt part
        json text_part;
        text_part["type"] = "text";
        text_part["text"] = msg.text_content;
        content_arr.push_back(text_part);

        // Optional image part
        if (!msg.image_base64.empty()) {
            json image_part;
            image_part["type"] = "image";
            image_part["source"]["type"] = "base64";
            
            std::string mime = msg.image_mime_type.empty() ? "image/jpeg" : msg.image_mime_type;
            image_part["source"]["media_type"] = mime;
            image_part["source"]["data"] = msg.image_base64;
            
            content_arr.push_back(image_part);
        }

        msg_obj["content"] = content_arr;
        messages_arr.push_back(msg_obj);
    }

    payload["messages"] = messages_arr;

    if (!system_prompt.empty()) {
        payload["system"] = system_prompt;
    }

    std::string request_body = payload.dump();

    // Anthropic API headers
    std::vector<std::string> headers = {
        "content-type: application/json",
        "x-api-key: " + api_key_,
        "anthropic-version: 2023-06-01"
    };

    std::string url = "https://api.anthropic.com/v1/messages";

    auto post_result = client_.post(url, request_body, headers);
    if (post_result.has_error()) {
        return core::Result<core::AiResponse, core::Error>::failure(std::move(post_result).error());
    }

    try {
        json response_json = json::parse(post_result.value());

        // Check for Claude API error payload
        if (response_json.contains("error")) {
            std::string err_msg = response_json["error"]["message"].get<std::string>();
            return core::Result<core::AiResponse, core::Error>::failure(
                core::Error(core::ErrorCode::kAiProviderError, "Claude API Error: " + err_msg));
        }

        core::AiResponse response;

        // Parse extracted text from contents list
        response.text = response_json["content"][0]["text"].get<std::string>();

        // Parse usage metadata
        if (response_json.contains("usage")) {
            auto usage_json = response_json["usage"];
            response.usage.prompt_tokens = usage_json.value("input_tokens", 0u);
            response.usage.completion_tokens = usage_json.value("output_tokens", 0u);
            response.usage.total_tokens = response.usage.prompt_tokens + response.usage.completion_tokens;
        }

        response.provider = "claude";
        response.model = target_model;

        return core::Result<core::AiResponse, core::Error>::success(std::move(response));

    } catch (const json::parse_error& e) {
        return core::Result<core::AiResponse, core::Error>::failure(
            core::Error(core::ErrorCode::kConfigParseError,
                        "Failed to parse Claude JSON response: " + std::string(e.what())));
    } catch (const std::exception& e) {
        return core::Result<core::AiResponse, core::Error>::failure(
            core::Error(core::ErrorCode::kAiProviderError,
                        "Unexpected error parsing Claude response: " + std::string(e.what())));
    }
}

std::string ClaudeProvider::provider_name() const noexcept {
    return "claude";
}

}  // namespace lilolify::ai
