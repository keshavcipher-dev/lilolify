// ============================================================================
// Lilolify — Result<T, E> Monadic Error Handling
// ============================================================================
// A sum type representing either a success value (T) or an error (E).
//
// This is the primary error-handling mechanism in Lilolify. Every function
// that can fail returns Result<T, Error> instead of throwing exceptions.
//
// Design Rationale:
//   1. EXPLICIT: The return type tells you a function can fail — no surprises.
//   2. COMPOSABLE: .map() and .and_then() enable functional error pipelines.
//   3. ZERO-COST: No exception tables, no unwinding overhead on success path.
//   4. THREAD-SAFE: No hidden control flow — safe in multithreaded contexts.
//
// Why not std::expected<T, E> (C++23)?
//   - C++23 compiler support is still uneven across MSVC/GCC/Clang
//   - We can add custom combinators (.map, .and_then, .or_else)
//   - Full control over the API, no surprise behavioral changes across compilers
//
// Usage:
//   Result<int, Error> divide(int a, int b) {
//       if (b == 0)
//           return Result<int, Error>::failure(
//               Error(ErrorCode::kInvalidArgument, "Division by zero"));
//       return Result<int, Error>::success(a / b);
//   }
//
//   auto result = divide(10, 2);
//   if (result.has_value()) {
//       std::cout << result.value();  // 5
//   }
//
//   // Or with combinators:
//   auto doubled = divide(10, 2).map([](int v) { return v * 2; });
// ============================================================================

#pragma once

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace lilolify::core {

// Forward declaration — the void specialization is defined below the primary.
template <typename T, typename E>
class Result;

// ============================================================================
// Result<void, E> Specialization
// ============================================================================
// Defined FIRST so that MSVC doesn't eagerly instantiate the primary template
// with T=void (which would fail because void can't be a struct member or
// function parameter). This is a known MSVC two-phase lookup issue.
// ============================================================================

template <typename E>
class Result<void, E> {
public:
    using value_type = void;
    using error_type = E;

    /// Create a successful void Result.
    [[nodiscard]] static Result success() {
        return Result(SuccessTag{});
    }

    /// Create a failed void Result.
    [[nodiscard]] static Result failure(E error) {
        return Result(FailureTag{}, std::move(error));
    }

    [[nodiscard]] bool has_value() const noexcept { return is_success_; }
    [[nodiscard]] bool has_error() const noexcept { return !is_success_; }
    [[nodiscard]] explicit operator bool() const noexcept { return is_success_; }

    [[nodiscard]] E& error() & { return *error_; }
    [[nodiscard]] const E& error() const& { return *error_; }
    [[nodiscard]] E&& error() && { return std::move(*error_); }

    /// Chain another Result-returning operation on success.
    /// F must be callable with no arguments and return a Result type.
    template <typename F>
    [[nodiscard]] auto and_then(F&& func) && -> std::invoke_result_t<F> {
        if (has_value()) {
            return std::invoke(std::forward<F>(func));
        }
        using ReturnType = std::invoke_result_t<F>;
        return ReturnType::failure(std::move(*this).error());
    }

    /// Handle the error case.
    template <typename F>
    [[nodiscard]] auto or_else(F&& func) && -> Result<void, E> {
        if (has_value()) {
            return Result::success();
        }
        return std::invoke(std::forward<F>(func), std::move(*this).error());
    }

    /// Execute a function on success (for side effects like logging).
    template <typename F>
    Result& on_success(F&& func) & {
        if (has_value()) {
            std::invoke(std::forward<F>(func));
        }
        return *this;
    }

    /// Execute a function on the error if failed.
    template <typename F>
    Result& on_error(F&& func) & {
        if (has_error()) {
            std::invoke(std::forward<F>(func), error());
        }
        return *this;
    }

private:
    struct SuccessTag {};
    struct FailureTag {};

    Result(SuccessTag) : is_success_(true) {}
    Result(FailureTag, E error) : is_success_(false), error_(std::move(error)) {}

    bool is_success_;
    std::optional<E> error_;
};

// ============================================================================
// Primary Result<T, E> Template
// ============================================================================
// For operations that return a value on success.
// The void specialization (above) handles the no-return-value case.
// ============================================================================

/// Result<T, E>: A type that holds either a success value of type T,
/// or an error of type E. Never both. Never neither.
///
/// @tparam T  The success value type. Use Result<void, E> for no return value.
/// @tparam E  The error type.
template <typename T, typename E>
class Result {
    static_assert(!std::is_void_v<T>,
                  "Use Result<void, E> specialization for void value types. "
                  "This primary template requires a non-void T.");

public:
    using value_type = T;
    using error_type = E;

    // ====================================================================
    // Factory Methods
    // ====================================================================

    /// Create a successful Result holding a value.
    [[nodiscard]] static Result success(T value) {
        return Result(SuccessTag{}, std::move(value));
    }

    /// Create a failed Result holding an error.
    [[nodiscard]] static Result failure(E error) {
        return Result(FailureTag{}, std::move(error));
    }

    // ====================================================================
    // Observers
    // ====================================================================

    /// Returns true if this Result holds a success value.
    [[nodiscard]] bool has_value() const noexcept {
        return std::holds_alternative<SuccessWrapper>(data_);
    }

    /// Returns true if this Result holds an error.
    [[nodiscard]] bool has_error() const noexcept { return !has_value(); }

    /// Implicit boolean conversion: true = success, false = error.
    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

    // ====================================================================
    // Value Access
    // ====================================================================

    /// Access the success value. Undefined behavior if has_error().
    /// Prefer checking has_value() first or using map/and_then.
    [[nodiscard]] T& value() & {
        return std::get<SuccessWrapper>(data_).value;
    }

    [[nodiscard]] const T& value() const& {
        return std::get<SuccessWrapper>(data_).value;
    }

    [[nodiscard]] T&& value() && {
        return std::move(std::get<SuccessWrapper>(data_).value);
    }

    /// Access the error. Undefined behavior if has_value().
    [[nodiscard]] E& error() & {
        return std::get<FailureWrapper>(data_).error;
    }

    [[nodiscard]] const E& error() const& {
        return std::get<FailureWrapper>(data_).error;
    }

    [[nodiscard]] E&& error() && {
        return std::move(std::get<FailureWrapper>(data_).error);
    }

    /// Access the value or return a default if this is an error.
    [[nodiscard]] T value_or(T default_value) const& {
        if (has_value()) return value();
        return default_value;
    }

    [[nodiscard]] T value_or(T default_value) && {
        if (has_value()) return std::move(*this).value();
        return default_value;
    }

    // ====================================================================
    // Combinators (Functional Composition)
    // ====================================================================

    /// Transform the success value using function F: T -> U.
    /// If this is an error, the error is propagated unchanged.
    ///
    /// Example:
    ///   Result<int, E>::success(5).map([](int v) { return v * 2; })
    ///   // => Result<int, E>::success(10)
    template <typename F>
    [[nodiscard]] auto map(F&& func) const& -> Result<std::invoke_result_t<F, const T&>, E> {
        using U = std::invoke_result_t<F, const T&>;
        if (has_value()) {
            return Result<U, E>::success(std::invoke(std::forward<F>(func), value()));
        }
        // Propagate error — requires E to be copyable or we need move overload
        // Since our Error is move-only, caller should use the && overload below.
        // For const& we need a workaround — create a new error with same info.
        return Result<U, E>::failure(E(error().code(), std::string(error().message())));
    }

    template <typename F>
    [[nodiscard]] auto map(F&& func) && -> Result<std::invoke_result_t<F, T&&>, E> {
        using U = std::invoke_result_t<F, T&&>;
        if (has_value()) {
            return Result<U, E>::success(
                std::invoke(std::forward<F>(func), std::move(*this).value()));
        }
        return Result<U, E>::failure(std::move(*this).error());
    }

    /// Chain operations that themselves return Result.
    /// F: T -> Result<U, E>
    ///
    /// Example:
    ///   parse_int("42").and_then([](int v) { return divide(v, 2); })
    template <typename F>
    [[nodiscard]] auto and_then(F&& func) && -> std::invoke_result_t<F, T&&> {
        if (has_value()) {
            return std::invoke(std::forward<F>(func), std::move(*this).value());
        }
        using ReturnType = std::invoke_result_t<F, T&&>;
        return ReturnType::failure(std::move(*this).error());
    }

    /// Handle the error case. F: E -> Result<T, E>
    /// If this is a success, returns *this unchanged.
    ///
    /// Example:
    ///   load_config("custom.json")
    ///       .or_else([](Error&&) { return load_config("default.json"); })
    template <typename F>
    [[nodiscard]] auto or_else(F&& func) && -> Result<T, E> {
        if (has_value()) {
            return Result::success(std::move(*this).value());
        }
        return std::invoke(std::forward<F>(func), std::move(*this).error());
    }

    // ====================================================================
    // Side-Effect Helpers
    // ====================================================================

    /// Execute a function on the value if successful, then return *this.
    /// Useful for logging or debugging without breaking a chain.
    template <typename F>
    Result& on_success(F&& func) & {
        if (has_value()) {
            std::invoke(std::forward<F>(func), value());
        }
        return *this;
    }

    /// Execute a function on the error if failed, then return *this.
    template <typename F>
    Result& on_error(F&& func) & {
        if (has_error()) {
            std::invoke(std::forward<F>(func), error());
        }
        return *this;
    }

private:
    // Tag types for constructor disambiguation
    struct SuccessTag {};
    struct FailureTag {};

    // Wrapper types to disambiguate T and E if they are the same type
    struct SuccessWrapper {
        T value;
    };
    struct FailureWrapper {
        E error;
    };

    // Construct a success
    Result(SuccessTag, T value)
        : data_(SuccessWrapper{std::move(value)}) {}

    // Construct a failure
    Result(FailureTag, E error)
        : data_(FailureWrapper{std::move(error)}) {}

    std::variant<SuccessWrapper, FailureWrapper> data_;
};

}  // namespace lilolify::core
