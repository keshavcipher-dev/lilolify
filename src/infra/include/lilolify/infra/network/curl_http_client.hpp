// ============================================================================
// Lilolify — CurlHttpClient
// ============================================================================
// Concrete implementation of IHttpClient using libcurl.
// ============================================================================

#pragma once

#include <lilolify/core/interfaces/i_http_client.hpp>

namespace lilolify::infra {

/// Network client wrapper around libcurl.
/// Synchronously transmits POST request JSON payloads to cloud services.
class CurlHttpClient : public core::IHttpClient {
public:
    CurlHttpClient();
    ~CurlHttpClient() override;

    // Prevent copy/move due to RAII libcurl lifetime lifecycle
    CurlHttpClient(const CurlHttpClient&) = delete;
    CurlHttpClient& operator=(const CurlHttpClient&) = delete;
    CurlHttpClient(CurlHttpClient&&) = delete;
    CurlHttpClient& operator=(CurlHttpClient&&) = delete;

    /// Execute synchronous HTTP POST request using libcurl.
    [[nodiscard]] core::Result<std::string, core::Error> post(
        const std::string& url,
        const std::string& body,
        const std::vector<std::string>& headers) override;

private:
    static std::size_t write_callback(
        void* contents,
        std::size_t size,
        std::size_t nmemb,
        void* userp);

    bool global_init_;
};

}  // namespace lilolify::infra
