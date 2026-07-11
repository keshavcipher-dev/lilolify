// ============================================================================
// Lilolify — SHA-256 Crypto Implementation
// ============================================================================

#include <lilolify/infra/crypto/sha256.hpp>

#include <iomanip>
#include <sstream>

namespace lilolify::infra {

namespace {

// SHA-256 Constants
constexpr std::array<std::uint32_t, 64> K = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// Logical operations helper functions
inline std::uint32_t rotr(std::uint32_t val, std::uint32_t count) noexcept {
    return (val >> count) | (val << (32 - count));
}

inline std::uint32_t ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
    return (x & y) ^ (~x & z);
}

inline std::uint32_t maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept {
    return (x & y) ^ (x & z) ^ (y & z);
}

inline std::uint32_t ep0(std::uint32_t x) noexcept {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

inline std::uint32_t ep1(std::uint32_t x) noexcept {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

inline std::uint32_t sig0(std::uint32_t x) noexcept {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}

inline std::uint32_t sig1(std::uint32_t x) noexcept {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

}  // namespace

// ============================================================================
// Hashing Implementation
// ============================================================================

Sha256::Sha256() noexcept {
    reset();
}

void Sha256::reset() noexcept {
    state_[0] = 0x6a09e667;
    state_[1] = 0xbb67ae85;
    state_[2] = 0x3c6ef372;
    state_[3] = 0xa54ff53a;
    state_[4] = 0x510e527f;
    state_[5] = 0x9b05688c;
    state_[6] = 0x1f83d9ab;
    state_[7] = 0x5be0cd19;

    bit_count_low_ = 0;
    bit_count_high_ = 0;
}

void Sha256::update(const std::uint8_t* data, std::size_t len) noexcept {
    for (std::size_t i = 0; i < len; ++i) {
        // Offset in current buffer
        auto buffer_offset = static_cast<std::size_t>((bit_count_low_ >> 3) & 63);
        buffer_[buffer_offset] = data[i];

        // Increment bits processed (low and high overflow tracking)
        bit_count_low_ += 8;
        if (bit_count_low_ == 0) {
            bit_count_high_ += 1;
        }

        // If buffer is full, process it
        if (((bit_count_low_ >> 3) & 63) == 0) {
            transform(buffer_.data());
        }
    }
}

void Sha256::update(const char* data, std::size_t len) noexcept {
    update(reinterpret_cast<const std::uint8_t*>(data), len);
}

std::string Sha256::finalize() noexcept {
    // 1. Save bit counts
    std::uint32_t final_bits_low = bit_count_low_;
    std::uint32_t final_bits_high = bit_count_high_;

    // 2. Pad: append 0x80 byte
    std::uint8_t pad_byte = 0x80;
    update(&pad_byte, 1);

    // 3. Keep padding with 0x00 until offset is 56 (leaving 8 bytes for length)
    while (((bit_count_low_ >> 3) & 63) != 56) {
        std::uint8_t zero_byte = 0x00;
        update(&zero_byte, 1);
    }

    // 4. Append big-endian 64-bit length
    std::array<std::uint8_t, 8> length_bytes;
    length_bytes[0] = static_cast<std::uint8_t>(final_bits_high >> 24);
    length_bytes[1] = static_cast<std::uint8_t>(final_bits_high >> 16);
    length_bytes[2] = static_cast<std::uint8_t>(final_bits_high >> 8);
    length_bytes[3] = static_cast<std::uint8_t>(final_bits_high);
    length_bytes[4] = static_cast<std::uint8_t>(final_bits_low >> 24);
    length_bytes[5] = static_cast<std::uint8_t>(final_bits_low >> 16);
    length_bytes[6] = static_cast<std::uint8_t>(final_bits_low >> 8);
    length_bytes[7] = static_cast<std::uint8_t>(final_bits_low);

    update(length_bytes.data(), 8);

    // 5. Build final hexadecimal string
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (auto val : state_) {
        ss << std::setw(8) << val;
    }

    return ss.str();
}

std::string Sha256::hash_buffer(const std::uint8_t* data, std::size_t len) noexcept {
    Sha256 hasher;
    hasher.update(data, len);
    return hasher.finalize();
}

std::string Sha256::hash_string(const std::string& str) noexcept {
    return hash_buffer(reinterpret_cast<const std::uint8_t*>(str.data()), str.size());
}

// Compression function
void Sha256::transform(const std::uint8_t* message) noexcept {
    std::array<std::uint32_t, 64> w;

    // Load message block (big-endian conversion)
    for (std::size_t i = 0, j = 0; i < 16; ++i, j += 4) {
        w[i] = (static_cast<std::uint32_t>(message[j]) << 24) |
               (static_cast<std::uint32_t>(message[j + 1]) << 16) |
               (static_cast<std::uint32_t>(message[j + 2]) << 8) |
               (static_cast<std::uint32_t>(message[j + 3]));
    }

    // Extend block
    for (std::size_t i = 16; i < 64; ++i) {
        w[i] = sig1(w[i - 2]) + w[i - 7] + sig0(w[i - 15]) + w[i - 16];
    }

    // Initialize round variables
    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];
    std::uint32_t f = state_[5];
    std::uint32_t g = state_[6];
    std::uint32_t h = state_[7];

    // Compression loop
    for (std::size_t i = 0; i < 64; ++i) {
        std::uint32_t t1 = h + ep1(e) + ch(e, f, g) + K[i] + w[i];
        std::uint32_t t2 = ep0(a) + maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    // Add compression result to current state
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

}  // namespace lilolify::infra
