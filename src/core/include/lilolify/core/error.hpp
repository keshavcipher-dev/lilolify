// ============================================================================
// Lilolify — Error Hierarchy
// ============================================================================
// A structured, typed error system for the entire application.
//
// Design Decision:
//   We use a concrete Error class with an ErrorCode enum instead of an
//   exception hierarchy. This pairs with Result<T, E> to provide:
//   - Explicit error handling at every call site
//   - Zero-cost when no error occurs (no exception tables)
//   - Pattern-matchable error codes for programmatic handling
//   - Human-readable messages for logging and UI display
//
// Why not std::error_code?
//   std::error_code is designed for system/library interop. Our errors are
//   domain-specific (AI failures, file conflicts, etc.) and benefit from
//   richer context (source location, nested causes). We keep the API simple
//   and avoid the complexity of custom error_category registration.
// ============================================================================

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace lilolify::core {

// ============================================================================
// Error Codes
// ============================================================================

/// Exhaustive error codes covering all failure modes in the system.
/// Grouped by subsystem for readability.
enum class ErrorCode : std::uint16_t {
    // -- General (0xx) --
    kUnknown              = 0,
    kInvalidArgument      = 1,
    kNotFound             = 2,
    kAlreadyExists        = 3,
    kPermissionDenied     = 4,
    kOperationCancelled   = 5,
    kTimeout              = 6,
    kNotImplemented       = 7,

    // -- File System (1xx) --
    kFileNotFound         = 100,
    kFileAccessDenied     = 101,
    kFileAlreadyExists    = 102,
    kFileMoveError        = 103,
    kFileReadError        = 104,
    kFileWriteError       = 105,
    kDirectoryNotFound    = 106,
    kDirectoryCreateError = 107,
    kInvalidPath          = 108,
    kDiskFull             = 109,

    // -- AI Service (2xx) --
    kAiProviderError      = 200,
    kAiAuthError          = 201,
    kAiRateLimited        = 202,
    kAiResponseParseError = 203,
    kAiTimeout            = 204,
    kAiQuotaExceeded      = 205,
    kAiInvalidResponse    = 206,
    kAiProviderNotFound   = 207,
    kAiModelNotAvailable  = 208,

    // -- OCR (3xx) --
    kOcrEngineError       = 300,
    kOcrInitError         = 301,
    kOcrProcessingError   = 302,
    kOcrLanguageNotFound  = 303,

    // -- Database (4xx) --
    kDatabaseOpenError    = 400,
    kDatabaseQueryError   = 401,
    kDatabaseWriteError   = 402,
    kDatabaseMigrationError = 403,
    kDatabaseCorrupted    = 404,

    // -- Pipeline (5xx) --
    kPipelineError        = 500,
    kPipelineStepFailed   = 501,
    kPipelineAborted      = 502,

    // -- Configuration (6xx) --
    kConfigNotFound       = 600,
    kConfigParseError     = 601,
    kConfigValidationError = 602,
};

// ============================================================================
// Error Class
// ============================================================================

/// Structured error type carrying a machine-readable code, a human-readable
/// message, and an optional causal chain for root-cause analysis.
///
/// Example usage:
///   auto err = Error(ErrorCode::kFileNotFound, "Config file missing: app.json");
///   auto wrapped = Error(ErrorCode::kPipelineError, "Pipeline failed", std::move(err));
///   // wrapped.cause()->code() == ErrorCode::kFileNotFound
///
/// This class is move-only (no copies) to prevent accidental duplication of
/// error chains, and is cheap to construct and propagate.
class Error {
public:
    // -- Constructors --

    /// Construct an error with a code and descriptive message.
    Error(ErrorCode code, std::string message) noexcept
        : code_(code), message_(std::move(message)), cause_(nullptr) {}

    /// Construct an error wrapping a root cause (causal chain).
    Error(ErrorCode code, std::string message, Error cause) noexcept
        : code_(code),
          message_(std::move(message)),
          cause_(std::make_unique<Error>(std::move(cause))) {}

    // -- Rule of Five (move-only) --

    Error(const Error&) = delete;
    Error& operator=(const Error&) = delete;

    Error(Error&& other) noexcept = default;
    Error& operator=(Error&& other) noexcept = default;

    ~Error() = default;

    // -- Accessors --

    /// Machine-readable error code for programmatic handling.
    [[nodiscard]] ErrorCode code() const noexcept { return code_; }

    /// Human-readable error description for logging/UI.
    [[nodiscard]] std::string_view message() const noexcept { return message_; }

    /// Optional root cause. Returns nullptr if this is the root error.
    [[nodiscard]] const Error* cause() const noexcept { return cause_.get(); }

    /// Check if the error matches a specific code.
    [[nodiscard]] bool is(ErrorCode code) const noexcept { return code_ == code; }

    // -- Formatting --

    /// Full error description including the causal chain.
    /// Format: "[ErrorCode] message\n  Caused by: [ErrorCode] message\n  ..."
    [[nodiscard]] std::string full_message() const {
        std::string result = "[" + std::to_string(static_cast<uint16_t>(code_)) + "] " + message_;
        const Error* current = cause_.get();
        while (current != nullptr) {
            result += "\n  Caused by: [" +
                      std::to_string(static_cast<uint16_t>(current->code_)) + "] " +
                      std::string(current->message_);
            current = current->cause_.get();
        }
        return result;
    }

    // -- Factory Methods --

    /// Create a generic "not found" error.
    [[nodiscard]] static Error not_found(std::string what) {
        return Error(ErrorCode::kNotFound, std::move(what));
    }

    /// Create a generic "invalid argument" error.
    [[nodiscard]] static Error invalid_argument(std::string what) {
        return Error(ErrorCode::kInvalidArgument, std::move(what));
    }

    /// Create a generic "permission denied" error.
    [[nodiscard]] static Error permission_denied(std::string what) {
        return Error(ErrorCode::kPermissionDenied, std::move(what));
    }

    /// Create a "not implemented" error for stub methods.
    [[nodiscard]] static Error not_implemented(std::string feature) {
        return Error(ErrorCode::kNotImplemented, std::move(feature) + " is not yet implemented");
    }

private:
    ErrorCode code_;
    std::string message_;
    std::unique_ptr<Error> cause_;  // Owning pointer to causal chain
};

// ============================================================================
// ErrorCode Utility
// ============================================================================

/// Convert ErrorCode to human-readable category string.
[[nodiscard]] constexpr std::string_view error_category(ErrorCode code) noexcept {
    auto raw = static_cast<uint16_t>(code);
    if (raw < 100) return "General";
    if (raw < 200) return "FileSystem";
    if (raw < 300) return "AI";
    if (raw < 400) return "OCR";
    if (raw < 500) return "Database";
    if (raw < 600) return "Pipeline";
    if (raw < 700) return "Configuration";
    return "Unknown";
}

}  // namespace lilolify::core
