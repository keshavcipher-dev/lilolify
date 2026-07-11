// ============================================================================
// Lilolify — Sha256Calculator
// ============================================================================
// Concrete implementation of IHashCalculator.
// ============================================================================

#pragma once

#include <lilolify/core/interfaces/i_hash_calculator.hpp>

namespace lilolify::infra {

/// Implements IHashCalculator by streaming file content into the Sha256 compressor.
class Sha256Calculator : public core::IHashCalculator {
public:
    Sha256Calculator() = default;
    ~Sha256Calculator() override = default;

    // Prevent copy/move
    Sha256Calculator(const Sha256Calculator&) = delete;
    Sha256Calculator& operator=(const Sha256Calculator&) = delete;
    Sha256Calculator(Sha256Calculator&&) = delete;
    Sha256Calculator& operator=(Sha256Calculator&&) = delete;

    /// Calculate the SHA-256 hash of a file using streaming chunked reads.
    ///
    /// @param path  Path to the file to hash.
    /// @return Hex-encoded SHA-256 string on success, or Error on file access error.
    [[nodiscard]] core::Result<std::string, core::Error> calculate_sha256(
        const core::FilePath& path) override;
};

}  // namespace lilolify::infra
