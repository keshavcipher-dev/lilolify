// ============================================================================
// Lilolify — AiProviderFactory
// ============================================================================
// Dynamic factory for initializing specific cloud AI Providers.
// ============================================================================

#pragma once

#include <lilolify/core/interfaces/i_ai_provider.hpp>
#include <lilolify/core/interfaces/i_http_client.hpp>

#include <memory>
#include <string>

namespace lilolify::ai {

/// Factory constructing configured Vision LLM API adapters.
class AiProviderFactory {
public:
    /// Create a concrete IAiProvider adapter based on configuration parameters.
    ///
    /// @param provider_name  Normalized provider string: "openai", "gemini", "claude".
    /// @param api_key        API Secret token key.
    /// @param client         The HTTP client reference to inject into the provider.
    /// @param model_override Optional specific model code override.
    /// @return Pointer to constructed provider on success, or Error on invalid name/configuration.
    [[nodiscard]] static core::Result<std::unique_ptr<core::IAiProvider>, core::Error> create(
        const std::string& provider_name,
        const std::string& api_key,
        core::IHttpClient& client,
        const std::string& model_override = "",
        const std::string& base_url = "");
};

}  // namespace lilolify::ai
