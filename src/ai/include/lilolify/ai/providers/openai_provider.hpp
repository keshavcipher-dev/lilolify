// ============================================================================
// Lilolify — OpenAiProvider
// ============================================================================
// Concrete implementation of IAiProvider for OpenAI Vision models (GPT-4o).
// ============================================================================

#pragma once

#include <lilolify/core/interfaces/i_ai_provider.hpp>
#include <lilolify/core/interfaces/i_http_client.hpp>

namespace lilolify::ai {

/// AI Provider adapter calling OpenAI completions endpoints.
class OpenAiProvider : public core::IAiProvider {
public:
    /// Construct OpenAI adapter.
    ///
    /// @param client   The HTTP client implementation to perform REST calls.
    /// @param api_key  OpenAI Secret API Key.
    /// @param model    Default model identifier (defaults to "gpt-4o").
    OpenAiProvider(
        core::IHttpClient& client,
        std::string api_key,
        std::string model = "gpt-4o",
        std::string base_url = "");

    ~OpenAiProvider() override = default;

    /// Execute a chat/vision query.
    [[nodiscard]] core::Result<core::AiResponse, core::Error> generate(
        const core::AiRequest& request) override;

    [[nodiscard]] std::string provider_name() const noexcept override;

private:
    core::IHttpClient& client_;
    std::string api_key_;
    std::string default_model_;
    std::string base_url_;
};

}  // namespace lilolify::ai
