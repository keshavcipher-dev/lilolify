// ============================================================================
// Lilolify — IAiProvider Interface (Port)
// ============================================================================
// Abstract port interface for generating reasoning responses from Vision AI.
// ============================================================================

#pragma once

#include <lilolify/core/error.hpp>
#include <lilolify/core/result.hpp>
#include <lilolify/core/value_objects/ai_types.hpp>

#include <string>

namespace lilolify::core {

/// Abstract port interface representing an AI reasoning engine backend
/// (e.g. OpenAI, Google Gemini, Anthropic Claude).
class IAiProvider {
public:
    virtual ~IAiProvider() = default;

    IAiProvider() = default;
    IAiProvider(const IAiProvider&) = delete;
    IAiProvider& operator=(const IAiProvider&) = delete;
    IAiProvider(IAiProvider&&) = delete;
    IAiProvider& operator=(IAiProvider&&) = delete;

    /// Send a request to the AI model and extract the response structure.
    ///
    /// @param request  Query message structure with prompt and options.
    /// @return Generated response context on success, or Error on API rate-limits/timeouts.
    [[nodiscard]] virtual Result<AiResponse, Error> generate(
        const AiRequest& request) = 0;

    /// Normalized provider name identifier (e.g. "openai", "gemini", "claude").
    [[nodiscard]] virtual std::string provider_name() const noexcept = 0;
};

}  // namespace lilolify::core
