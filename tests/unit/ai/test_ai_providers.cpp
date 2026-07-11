// ============================================================================
// Lilolify — Unit Tests: AI Providers (Offline REST verification)
// ============================================================================

#include <lilolify/ai/providers/openai_provider.hpp>
#include <lilolify/ai/providers/gemini_provider.hpp>
#include <lilolify/ai/providers/claude_provider.hpp>
#include <lilolify/ai/providers/ai_provider_factory.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace lilolify::ai::test {

// ============================================================================
// Mock HTTP Client for Offline API Verification
// ============================================================================

class MockHttpClient : public core::IHttpClient {
public:
    core::Result<std::string, core::Error> post(
        const std::string& url,
        const std::string& body,
        const std::vector<std::string>& headers) override {

        last_url = url;
        last_body = body;
        last_headers = headers;

        if (should_fail) {
            return core::Result<std::string, core::Error>::failure(
                core::Error(fail_code, "Simulated HTTP error code " + std::to_string(http_status_code)));
        }

        return core::Result<std::string, core::Error>::success(response_to_return);
    }

    std::string response_to_return;
    std::string last_url;
    std::string last_body;
    std::vector<std::string> last_headers;

    bool should_fail = false;
    int http_status_code = 200;
    core::ErrorCode fail_code = core::ErrorCode::kAiProviderError;
};

// ============================================================================
// OpenAI Provider Tests
// ============================================================================

TEST(AiProviderTest, OpenAiGeneratesValidPayloadAndParsesResponse) {
    MockHttpClient mock_client;
    OpenAiProvider provider(mock_client, "sk-test-key-12345");

    // Pre-program OpenAI mock response
    mock_client.response_to_return = R"({
        "id": "chatcmpl-mock",
        "object": "chat.completion",
        "created": 1677652288,
        "model": "gpt-4o-2024-05-13",
        "choices": [{
            "index": 0,
            "message": {
                "role": "assistant",
                "content": "Normalized classification: [Receipt]"
            },
            "finish_reason": "stop"
        }],
        "usage": {
            "prompt_tokens": 150,
            "completion_tokens": 25,
            "total_tokens": 175
        }
    })";

    core::AiRequest request;
    request.messages.push_back({"system", "Analyze the file type."});
    request.messages.push_back({"user", "What is this document?", "base64data", "image/png"});

    auto result = provider.generate(request);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().text, "Normalized classification: [Receipt]");
    EXPECT_EQ(result.value().usage.prompt_tokens, 150u);
    EXPECT_EQ(result.value().usage.completion_tokens, 25u);
    EXPECT_EQ(result.value().usage.total_tokens, 175u);
    EXPECT_EQ(result.value().provider, "openai");
    EXPECT_EQ(result.value().model, "gpt-4o-2024-05-13");

    // Verify request payload was serialized properly
    json parsed_req = json::parse(mock_client.last_body);
    EXPECT_EQ(parsed_req["model"], "gpt-4o");
    EXPECT_EQ(parsed_req["messages"].size(), 2u);
    EXPECT_EQ(parsed_req["messages"][0]["role"], "system");
    EXPECT_EQ(parsed_req["messages"][0]["content"], "Analyze the file type.");
    EXPECT_EQ(parsed_req["messages"][1]["role"], "user");

    auto content_array = parsed_req["messages"][1]["content"];
    ASSERT_TRUE(content_array.is_array());
    EXPECT_EQ(content_array[0]["type"], "text");
    EXPECT_EQ(content_array[0]["text"], "What is this document?");
    EXPECT_EQ(content_array[1]["type"], "image_url");
    EXPECT_EQ(content_array[1]["image_url"]["url"], "data:image/png;base64,base64data");

    // Verify authorization header was set
    bool auth_header_found = false;
    for (const auto& header : mock_client.last_headers) {
        if (header.find("Authorization: Bearer sk-test-key-12345") != std::string::npos) {
            auth_header_found = true;
        }
    }
    EXPECT_TRUE(auth_header_found);
}

// ============================================================================
// Gemini Provider Tests
// ============================================================================

TEST(AiProviderTest, GeminiGeneratesValidPayloadAndParsesResponse) {
    MockHttpClient mock_client;
    GeminiProvider provider(mock_client, "gemini-test-key");

    mock_client.response_to_return = R"({
        "candidates": [{
            "content": {
                "parts": [{
                    "text": "Intelligent reasoning result."
                }],
                "role": "model"
            },
            "finishReason": "STOP"
        }],
        "usageMetadata": {
            "promptTokenCount": 200,
            "candidatesTokenCount": 50,
            "totalTokenCount": 250
        }
    })";

    core::AiRequest request;
    request.messages.push_back({"system", "Classify content."});
    request.messages.push_back({"user", "Query text", "base64image", "image/jpeg"});

    auto result = provider.generate(request);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().text, "Intelligent reasoning result.");
    EXPECT_EQ(result.value().usage.prompt_tokens, 200u);
    EXPECT_EQ(result.value().usage.completion_tokens, 50u);
    EXPECT_EQ(result.value().usage.total_tokens, 250u);
    EXPECT_EQ(result.value().provider, "gemini");

    // Verify request url & payload structure
    EXPECT_TRUE(mock_client.last_url.find("key=gemini-test-key") != std::string::npos);

    json parsed_req = json::parse(mock_client.last_body);
    EXPECT_EQ(parsed_req["systemInstruction"]["parts"][0]["text"], "Classify content.");
    EXPECT_EQ(parsed_req["contents"].size(), 1u);
    EXPECT_EQ(parsed_req["contents"][0]["role"], "user");
    
    auto parts = parsed_req["contents"][0]["parts"];
    EXPECT_EQ(parts[0]["text"], "Query text");
    EXPECT_EQ(parts[1]["inlineData"]["mimeType"], "image/jpeg");
    EXPECT_EQ(parts[1]["inlineData"]["data"], "base64image");
}

// ============================================================================
// Claude Provider Tests
// ============================================================================

TEST(AiProviderTest, ClaudeGeneratesValidPayloadAndParsesResponse) {
    MockHttpClient mock_client;
    ClaudeProvider provider(mock_client, "claude-key");

    mock_client.response_to_return = R"({
        "id": "msg_01X",
        "type": "message",
        "role": "assistant",
        "content": [{
            "type": "text",
            "text": "Claude output text."
        }],
        "model": "claude-3-5-sonnet-20240620",
        "usage": {
            "input_tokens": 300,
            "output_tokens": 60
        }
    })";

    core::AiRequest request;
    request.messages.push_back({"system", "Roleplay system."});
    request.messages.push_back({"user", "Analyze visual", "base64claude", "image/png"});

    auto result = provider.generate(request);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().text, "Claude output text.");
    EXPECT_EQ(result.value().usage.prompt_tokens, 300u);
    EXPECT_EQ(result.value().usage.completion_tokens, 60u);
    EXPECT_EQ(result.value().usage.total_tokens, 360u);
    EXPECT_EQ(result.value().provider, "claude");

    // Verify request headers & system prompt mapping
    bool apiKeyFound = false;
    for (const auto& header : mock_client.last_headers) {
        if (header == "x-api-key: claude-key") {
            apiKeyFound = true;
        }
    }
    EXPECT_TRUE(apiKeyFound);

    json parsed_req = json::parse(mock_client.last_body);
    EXPECT_EQ(parsed_req["system"], "Roleplay system.");
    EXPECT_EQ(parsed_req["messages"].size(), 1u);
    EXPECT_EQ(parsed_req["messages"][0]["role"], "user");

    auto content = parsed_req["messages"][0]["content"];
    EXPECT_EQ(content[0]["type"], "text");
    EXPECT_EQ(content[0]["text"], "Analyze visual");
    EXPECT_EQ(content[1]["type"], "image");
    EXPECT_EQ(content[1]["source"]["media_type"], "image/png");
    EXPECT_EQ(content[1]["source"]["data"], "base64claude");
}

// ============================================================================
// Factory Tests
// ============================================================================

TEST(AiProviderTest, FactoryInitializesCorrectProviders) {
    MockHttpClient client;
    
    auto openai = AiProviderFactory::create("openai", "key", client);
    ASSERT_TRUE(openai.has_value());
    EXPECT_EQ(openai.value()->provider_name(), "openai");

    auto gemini = AiProviderFactory::create("GEMINI", "key", client);
    ASSERT_TRUE(gemini.has_value());
    EXPECT_EQ(gemini.value()->provider_name(), "gemini");

    auto claude = AiProviderFactory::create("Claude", "key", client);
    ASSERT_TRUE(claude.has_value());
    EXPECT_EQ(claude.value()->provider_name(), "claude");

    // Fails on empty key
    auto badKey = AiProviderFactory::create("openai", "", client);
    EXPECT_TRUE(badKey.has_error());

    // Fails on unknown provider
    auto unknown = AiProviderFactory::create("unknown-ai", "key", client);
    EXPECT_TRUE(unknown.has_error());
}

}  // namespace lilolify::ai::test
