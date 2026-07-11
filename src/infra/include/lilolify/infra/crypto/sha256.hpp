// ============================================================================
// Lilolify — SHA-256 Crypto Implementation
// ============================================================================
// A self-contained, standard-compliant SHA-256 implementation.
// Avoids dependencies on external libraries (like OpenSSL or WinCrypt) for
// portability and cross-platform consistency.
// ============================================================================

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace lilolify::infra {

/// Self-contained standard SHA-256 hash generator.
class Sha256 {
public:
    Sha256() noexcept;

    /// Reset the hashing state.
    void reset() noexcept;

    /// Process a block of data.
    void update(const std::uint8_t* data, std::size_t len) noexcept;
    void update(const char* data, std::size_t len) noexcept;

    /// Finalize hashing and return the hex-encoded hash string.
    std::string finalize() noexcept;

    /// Static convenience method to hash a full buffer in one pass.
    static std::string hash_buffer(const std::uint8_t* data, std::size_t len) noexcept;
    static std::string hash_string(const std::string& str) noexcept;

private:
    void transform(const std::uint8_t* message) noexcept;

    std::array<std::uint32_t, 8> state_;
    std::array<std::uint8_t, 64> buffer_;
    std::uint32_t bit_count_low_;
    std::uint32_t bit_count_high_;
};

}  // namespace lilolify::infra
