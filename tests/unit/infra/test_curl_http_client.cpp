// ============================================================================
// Lilolify — Unit Tests: CurlHttpClient
// ============================================================================

#include <lilolify/infra/network/curl_http_client.hpp>

#include <gtest/gtest.h>

namespace lilolify::infra::test {

TEST(CurlHttpClientTest, RequestToInvalidDomainReturnsFailure) {
    CurlHttpClient client;
    
    // Attempting to post to a completely invalid domain should fail at DNS layer
    auto result = client.post("http://invalid.domain.xyz-gravity-nonexistent-12345.com",
                              "{}",
                              {"Content-Type: application/json"});
                              
    ASSERT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), core::ErrorCode::kAiTimeout);
}

}  // namespace lilolify::infra::test
