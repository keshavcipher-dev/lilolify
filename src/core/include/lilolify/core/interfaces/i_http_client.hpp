// ============================================================================
// Lilolify — IHttpClient Interface (Port)
// ============================================================================
// Abstract port interface for HTTP network operations.
// Decouples REST calls from curl/socket library dependencies, enabling
// mock-injected, offline testing.
// ============================================================================

#pragma once

#include <lilolify/core/error.hpp>
#include <lilolify/core/result.hpp>

#include <string>
#include <vector>

namespace lilolify::core {

/// Abstract port interface for performing HTTP REST requests.
class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    IHttpClient() = default;
    IHttpClient(const IHttpClient&) = delete;
    IHttpClient& operator=(const IHttpClient&) = delete;
    IHttpClient(IHttpClient&&) = delete;
    IHttpClient& operator=(IHttpClient&&) = delete;

    /// Send a synchronous HTTP POST request.
    ///
    /// @param url      Target REST endpoint URL.
    /// @param body     JSON string body payload.
    /// @param headers  HTTP headers list (e.g. "Content-Type: application/json").
    /// @return The response body string on success, or Error on network/timeout.
    [[nodiscard]] virtual Result<std::string, Error> post(
        const std::string& url,
        const std::string& body,
        const std::vector<std::string>& headers) = 0;
};

}  // namespace lilolify::core
