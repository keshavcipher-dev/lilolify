// ============================================================================
// Lilolify — AiProviderFactory Implementation
// ============================================================================

#include <lilolify/ai/providers/ai_provider_factory.hpp>

#include <lilolify/ai/providers/openai_provider.hpp>
#include <lilolify/ai/providers/gemini_provider.hpp>
#include <lilolify/ai/providers/claude_provider.hpp>

#include <algorithm>

namespace lilolify::ai {

core::Result<std::unique_ptr<core::IAiProvider>, core::Error> AiProviderFactory::create(
    const std::string& provider_name,
    const std::string& api_key,
    core::IHttpClient& client,
    const std::string& model_override,
    const std::string& base_url) {

    if (api_key.empty()) {
        return core::Result<std::unique_ptr<core::IAiProvider>, core::Error>::failure(
            core::Error(core::ErrorCode::kAiProviderError, "API Key cannot be empty"));
    }

    std::string name_lower = provider_name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                   [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });

    if (name_lower == "openai") {
        std::string model = model_override.empty() ? "gpt-4o" : model_override;
        return core::Result<std::unique_ptr<core::IAiProvider>, core::Error>::success(
            std::make_unique<OpenAiProvider>(client, api_key, std::move(model), base_url));
    }

    if (name_lower == "openrouter") {
        std::string model = model_override.empty() ? "google/gemini-2.5-flash" : model_override;
        return core::Result<std::unique_ptr<core::IAiProvider>, core::Error>::success(
            std::make_unique<OpenAiProvider>(client, api_key, std::move(model), "https://openrouter.ai/api/v1/chat/completions"));
    }

    if (name_lower == "custom" || name_lower == "gateway") {
        std::string model = model_override.empty() ? "gpt-4o" : model_override;
        std::string url = base_url.empty() ? "https://api.openai.com/v1/chat/completions" : base_url;
        return core::Result<std::unique_ptr<core::IAiProvider>, core::Error>::success(
            std::make_unique<OpenAiProvider>(client, api_key, std::move(model), std::move(url)));
    }

    if (name_lower == "gemini" || name_lower == "google") {
        std::string model = model_override.empty() ? "gemini-1.5-flash" : model_override;
        return core::Result<std::unique_ptr<core::IAiProvider>, core::Error>::success(
            std::make_unique<GeminiProvider>(client, api_key, std::move(model)));
    }

    if (name_lower == "claude" || name_lower == "anthropic") {
        std::string model = model_override.empty() ? "claude-3-5-sonnet-20240620" : model_override;
        return core::Result<std::unique_ptr<core::IAiProvider>, core::Error>::success(
            std::make_unique<ClaudeProvider>(client, api_key, std::move(model)));
    }

    return core::Result<std::unique_ptr<core::IAiProvider>, core::Error>::failure(
        core::Error(core::ErrorCode::kOcrLanguageNotFound, // maps to unsupported provider code
                    "Unsupported AI Provider name: " + provider_name));
}

}  // namespace lilolify::ai
