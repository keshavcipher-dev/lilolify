// ============================================================================
// Lilolify — CurlHttpClient Implementation
// ============================================================================

#include <lilolify/infra/network/curl_http_client.hpp>

#include <curl/curl.h>

namespace lilolify::infra {

namespace {
// Thread-safe flag tracking curl global init state
static bool s_curl_globally_initialized = false;
}  // namespace

// ============================================================================
// Construction / Destruction
// ============================================================================

CurlHttpClient::CurlHttpClient() : global_init_(false) {
    if (!s_curl_globally_initialized) {
        // Initialize libcurl context once per application process lifetime
        curl_global_init(CURL_GLOBAL_ALL);
        s_curl_globally_initialized = true;
        global_init_ = true;
    }
}

CurlHttpClient::~CurlHttpClient() {
    if (global_init_) {
        // Clean up global context if this instance initialized it
        curl_global_cleanup();
        s_curl_globally_initialized = false;
    }
}

// ============================================================================
// HTTP POST Request Implementation
// ============================================================================

core::Result<std::string, core::Error> CurlHttpClient::post(
    const std::string& url,
    const std::string& body,
    const std::vector<std::string>& headers) {

    CURL* curl = curl_easy_init();
    if (!curl) {
        return core::Result<std::string, core::Error>::failure(
            core::Error(core::ErrorCode::kPipelineError, "Failed to initialize Curl easy handle"));
    }

    std::string response_buffer;
    curl_slist* chunk = nullptr;

    // Load headers into libcurl slist
    for (const auto& header : headers) {
        chunk = curl_slist_append(chunk, header.c_str());
    }

    // Configure Curl options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));

    if (chunk) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    }

    // Capture response contents
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);

    // Enforce reasonable connection (10s) and transfer (30s) timeouts
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    // Perform the REST call
    CURLcode res = curl_easy_perform(curl);

    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    // Cleanup resources
    if (chunk) {
        curl_slist_free_all(chunk);
    }
    curl_easy_cleanup(curl);

    // Handle Curl errors
    if (res != CURLE_OK) {
        std::string err_msg(curl_easy_strerror(res));
        return core::Result<std::string, core::Error>::failure(
            core::Error(core::ErrorCode::kAiTimeout, // maps to timeout or network error
                        "HTTP POST request failed: " + err_msg));
    }

    // Handle HTTP error codes (e.g. 401 Unauthorized, 429 Rate limited, 500 Server error)
    if (response_code >= 400) {
        core::ErrorCode ec = core::ErrorCode::kAiProviderError;
        if (response_code == 429) {
            ec = core::ErrorCode::kAiRateLimited;
        } else if (response_code == 408 || response_code == 504) {
            ec = core::ErrorCode::kAiTimeout;
        }

        return core::Result<std::string, core::Error>::failure(
            core::Error(ec, "HTTP POST returned error code " + std::to_string(response_code) +
                            ". Response: " + response_buffer));
    }

    return core::Result<std::string, core::Error>::success(std::move(response_buffer));
}

// ============================================================================
// Write Callback
// ============================================================================

std::size_t CurlHttpClient::write_callback(
    void* contents,
    std::size_t size,
    std::size_t nmemb,
    void* userp) {

    auto* buffer = static_cast<std::string*>(userp);
    std::size_t total_size = size * nmemb;
    buffer->append(static_cast<const char*>(contents), total_size);
    return total_size;
}

}  // namespace lilolify::infra
