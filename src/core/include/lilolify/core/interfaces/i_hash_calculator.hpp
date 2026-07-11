// ============================================================================
// Lilolify — IHashCalculator Interface (Port)
// ============================================================================
// Abstract port interface for computing cryptographic file hashes.
// ============================================================================

#pragma once

#include <lilolify/core/error.hpp>
#include <lilolify/core/result.hpp>
#include <lilolify/core/types.hpp>

#include <string>

namespace lilolify::core {

/// Abstract interface for calculating file hashes in a streaming chunked manner.
class IHashCalculator {
public:
    virtual ~IHashCalculator() = default;

    IHashCalculator() = default;
    IHashCalculator(const IHashCalculator&) = delete;
    IHashCalculator& operator=(const IHashCalculator&) = delete;
    IHashCalculator(IHashCalculator&&) = delete;
    IHashCalculator& operator=(IHashCalculator&&) = delete;

    /// Calculate the SHA-256 hash of a file.
    ///
    /// @param path  Path to the file to hash.
    /// @return Hex-encoded SHA-256 string on success, or Error on read failure.
    [[nodiscard]] virtual Result<std::string, Error> calculate_sha256(const FilePath& path) = 0;
};

}  // namespace lilolify::core
