// ============================================================================
// Lilolify — ClaudeProvider
// ============================================================================
// Concrete implementation of IAiProvider for Anthropic Claude Vision models.
// ============================================================================

#pragma once

#include <lilolify/core/interfaces/i_ai_provider.hpp>
#include <lilolify/core/interfaces/i_http_client.hpp>

namespace lilolify::ai {

/// AI Provider adapter calling Anthropic Claude Messages API.
class ClaudeProvider : public core::IAiProvider {
public:
    /// Construct Claude adapter.
    ///
    /// @param client   The HTTP client implementation to perform REST calls.
    /// @param api_key  Anthropic Claude API Key.
    /// @param model    Default model identifier (defaults to "claude-3-5-sonnet-20240620").
    ClaudeProvider(
        core::IHttpClient& client,
        std::string api_key,
        std::string model = "claude-3-5-sonnet-20240620");

    ~ClaudeProvider() override = default;

    /// Execute a messages request.
    [[nodiscard]] core::Result<core::AiResponse, core::Error> generate(
        const core::AiRequest& request) override;

    [[nodiscard]] std::string provider_name() const noexcept override;

private:
    core::IHttpClient& client_;
    std::string api_key_;
    std::string default_model_;
};

}  // namespace lilolify::ai
