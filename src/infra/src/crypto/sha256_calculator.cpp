// ============================================================================
// Lilolify — Sha256Calculator Implementation
// ============================================================================

#include <lilolify/infra/crypto/sha256.hpp>
#include <lilolify/infra/crypto/sha256_calculator.hpp>

#include <fstream>
#include <vector>

namespace lilolify::infra {

core::Result<std::string, core::Error> Sha256Calculator::calculate_sha256(
    const core::FilePath& path) {

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return core::Result<std::string, core::Error>::failure(
            core::Error(core::ErrorCode::kFileNotFound,
                        "Could not open file for hashing: " + path.string()));
    }

    // Use a 64 KB buffer for streaming (optimal for high I/O throughput)
    constexpr std::size_t kBufferSize = 64 * 1024;
    std::vector<char> buffer(kBufferSize);

    Sha256 sha;

    while (file.good()) {
        file.read(buffer.data(), kBufferSize);
        auto bytes_read = static_cast<std::size_t>(file.gcount());
        if (bytes_read > 0) {
            sha.update(buffer.data(), bytes_read);
        }
    }

    if (file.bad()) {
        return core::Result<std::string, core::Error>::failure(
            core::Error(core::ErrorCode::kFileReadError,
                        "I/O error occurred while hashing file: " + path.string()));
    }

    return core::Result<std::string, core::Error>::success(sha.finalize());
}

}  // namespace lilolify::infra
