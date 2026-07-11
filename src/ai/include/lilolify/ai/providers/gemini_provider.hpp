// ============================================================================
// Lilolify — GeminiProvider
// ============================================================================
// Concrete implementation of IAiProvider for Google Gemini Vision models.
// ============================================================================

#pragma once

#include <lilolify/core/interfaces/i_ai_provider.hpp>
#include <lilolify/core/interfaces/i_http_client.hpp>

namespace lilolify::ai {

/// AI Provider adapter calling Google Gemini REST API.
class GeminiProvider : public core::IAiProvider {
public:
    /// Construct Gemini adapter.
    ///
    /// @param client   The HTTP client implementation to perform REST calls.
    /// @param api_key  Google Gemini API Key.
    /// @param model    Default model identifier (defaults to "gemini-1.5-flash").
    GeminiProvider(
        core::IHttpClient& client,
        std::string api_key,
        std::string model = "gemini-1.5-flash");

    ~GeminiProvider() override = default;

    /// Execute a content generation request.
    [[nodiscard]] core::Result<core::AiResponse, core::Error> generate(
        const core::AiRequest& request) override;

    [[nodiscard]] std::string provider_name() const noexcept override;

private:
    core::IHttpClient& client_;
    std::string api_key_;
    std::string default_model_;
};

}  // namespace lilolify::ai
