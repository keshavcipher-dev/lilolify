// ============================================================================
// Lilolify — AI Service Types & Value Objects
// ============================================================================
// Data models for prompt construction, image sending, and token usage tracking.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lilolify::core {

/// A single message in a chat conversation payload sent to the AI.
struct AiMessage {
    /// Role of the message creator: "system", "user", "assistant".
    std::string role;

    /// Plain text prompt.
    std::string text_content;

    /// Optional raw Base64-encoded image data for vision models.
    std::string image_base64;

    /// Optional MIME type of the image (e.g. "image/jpeg", "image/png").
    std::string image_mime_type;
};

/// Parameters for querying the AI Provider model.
struct AiRequest {
    /// Full conversation context.
    std::vector<AiMessage> messages;

    /// Specific model name override (if empty, the provider's default is used).
    std::string model;

    /// Control randomness (0.0 = deterministic, 1.0 = creative).
    /// Default 0.1 for high logic stability.
    double temperature = 0.1;

    /// Maximum tokens allowed in the completion.
    std::uint32_t max_tokens = 1000;
};

/// Statistics tracking token usage for billing, logging, and rate limit budgets.
struct TokenUsage {
    std::uint32_t prompt_tokens = 0;
    std::uint32_t completion_tokens = 0;
    std::uint32_t total_tokens = 0;
};

/// Struct representing the response returned by a Vision AI Provider.
struct AiResponse {
    /// Raw generated text response.
    std::string text;

    /// Token usage count statistics.
    TokenUsage usage;

    /// Provider identifier (e.g. "openai", "gemini", "claude").
    std::string provider;

    /// The exact model string that responded (e.g. "gpt-4o-2024-05-13").
    std::string model;
};

}  // namespace lilolify::core
