// ============================================================================
// Lilolify — OpenAiProvider Implementation
// ============================================================================

#include <lilolify/ai/providers/openai_provider.hpp>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace lilolify::ai {

OpenAiProvider::OpenAiProvider(
    core::IHttpClient& client,
    std::string api_key,
    std::string model,
    std::string base_url)
    : client_(client), 
      api_key_(std::move(api_key)), 
      default_model_(std::move(model)),
      base_url_(std::move(base_url)) {}

core::Result<core::AiResponse, core::Error> OpenAiProvider::generate(
    const core::AiRequest& request) {

    if (api_key_.empty()) {
        return core::Result<core::AiResponse, core::Error>::failure(
            core::Error(core::ErrorCode::kAiProviderError, "OpenAI API Key is empty"));
    }

    std::string target_model = request.model.empty() ? default_model_ : request.model;

    // Build the request JSON payload
    json payload;
    payload["model"] = target_model;
    payload["temperature"] = request.temperature;
    payload["max_tokens"] = request.max_tokens;

    json messages_arr = json::array();
    for (const auto& msg : request.messages) {
        json msg_obj;
        msg_obj["role"] = msg.role;

        // If message has image data, serialize as multi-part array
        if (!msg.image_base64.empty()) {
            json content_arr = json::array();

            json text_part;
            text_part["type"] = "text";
            text_part["text"] = msg.text_content;
            content_arr.push_back(text_part);

            json image_part;
            image_part["type"] = "image_url";
            
            std::string mime = msg.image_mime_type.empty() ? "image/jpeg" : msg.image_mime_type;
            image_part["image_url"]["url"] = "data:" + mime + ";base64," + msg.image_base64;
            content_arr.push_back(image_part);

            msg_obj["content"] = content_arr;
        } else {
            msg_obj["content"] = msg.text_content;
        }

        messages_arr.push_back(msg_obj);
    }
    payload["messages"] = messages_arr;

    std::string request_body = payload.dump();

    // Configure headers
    std::vector<std::string> headers = {
        "Content-Type: application/json",
        "Authorization: Bearer " + api_key_
    };

    if (base_url_.find("openrouter.ai") != std::string::npos) {
        headers.push_back("HTTP-Referer: https://github.com/keshavbhardwaj/lilolify");
        headers.push_back("X-Title: Lilolify");
    }

    std::string endpoint = base_url_.empty() ? "https://api.openai.com/v1/chat/completions" : base_url_;

    // Perform REST POST request
    auto post_result = client_.post(endpoint, request_body, headers);
    if (post_result.has_error()) {
        return core::Result<core::AiResponse, core::Error>::failure(std::move(post_result).error());
    }

    // Parse Response
    try {
        json response_json = json::parse(post_result.value());

        // Check if OpenAI returned API error payload inside a 200 OK wrapper (rare but possible)
        if (response_json.contains("error")) {
            std::string err_msg = response_json["error"]["message"].get<std::string>();
            return core::Result<core::AiResponse, core::Error>::failure(
                core::Error(core::ErrorCode::kAiProviderError, "OpenAI API Error: " + err_msg));
        }

        core::AiResponse response;
        
        // Parse generated text content
        response.text = response_json["choices"][0]["message"]["content"].get<std::string>();
        
        // Parse token usage stats
        if (response_json.contains("usage")) {
            auto usage_json = response_json["usage"];
            response.usage.prompt_tokens = usage_json.value("prompt_tokens", 0u);
            response.usage.completion_tokens = usage_json.value("completion_tokens", 0u);
            response.usage.total_tokens = usage_json.value("total_tokens", 0u);
        }

        response.provider = "openai";
        response.model = response_json.value("model", target_model);

        return core::Result<core::AiResponse, core::Error>::success(std::move(response));

    } catch (const json::parse_error& e) {
        return core::Result<core::AiResponse, core::Error>::failure(
            core::Error(core::ErrorCode::kConfigParseError,
                        "Failed to parse OpenAI JSON response: " + std::string(e.what())));
    } catch (const std::exception& e) {
        return core::Result<core::AiResponse, core::Error>::failure(
            core::Error(core::ErrorCode::kAiProviderError,
                        "Unexpected error parsing OpenAI response: " + std::string(e.what())));
    }
}

std::string OpenAiProvider::provider_name() const noexcept {
    return "openai";
}

}  // namespace lilolify::ai
