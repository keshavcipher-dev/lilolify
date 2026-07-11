// ============================================================================
// Lilolify — Unit Tests: Result<T, E>
// ============================================================================
// Exhaustive tests for the Result monad covering:
//   - Construction (success/failure)
//   - Value access and observers
//   - Combinators (map, and_then, or_else)
//   - void specialization
//   - Edge cases (move semantics, value_or)
// ============================================================================

#include <lilolify/core/error.hpp>
#include <lilolify/core/result.hpp>

#include <gtest/gtest.h>

#include <string>
#include <utility>

namespace lilolify::core::test {

// Convenience alias for tests
using IntResult = Result<int, Error>;
using StringResult = Result<std::string, Error>;
using VoidResult = Result<void, Error>;

// ============================================================================
// Construction Tests
// ============================================================================

TEST(ResultTest, SuccessConstructionHoldsValue) {
    auto result = IntResult::success(42);

    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result.has_error());
    EXPECT_TRUE(static_cast<bool>(result));
    EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, FailureConstructionHoldsError) {
    auto result = IntResult::failure(Error(ErrorCode::kNotFound, "Item not found"));

    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.has_error());
    EXPECT_FALSE(static_cast<bool>(result));
    EXPECT_EQ(result.error().code(), ErrorCode::kNotFound);
    EXPECT_EQ(result.error().message(), "Item not found");
}

TEST(ResultTest, SuccessWithStringValue) {
    auto result = StringResult::success("hello world");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "hello world");
}

TEST(ResultTest, SuccessWithZeroIsStillSuccess) {
    // Ensure that a "falsy" value like 0 is still treated as success
    auto result = IntResult::success(0);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 0);
}

TEST(ResultTest, SuccessWithEmptyStringIsStillSuccess) {
    auto result = StringResult::success("");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "");
}

// ============================================================================
// Move Semantics Tests
// ============================================================================

TEST(ResultTest, MoveConstructionTransfersOwnership) {
    auto original = StringResult::success("owned string");
    auto moved = std::move(original);

    EXPECT_TRUE(moved.has_value());
    EXPECT_EQ(moved.value(), "owned string");
}

TEST(ResultTest, MoveAssignmentTransfersOwnership) {
    auto original = IntResult::success(99);
    auto target = IntResult::failure(Error(ErrorCode::kUnknown, "placeholder"));

    target = std::move(original);

    EXPECT_TRUE(target.has_value());
    EXPECT_EQ(target.value(), 99);
}

TEST(ResultTest, MoveFromRvalueExtractsValue) {
    auto value = IntResult::success(42).value();
    EXPECT_EQ(value, 42);
}

TEST(ResultTest, MoveFromRvalueExtractsError) {
    auto error = IntResult::failure(Error(ErrorCode::kTimeout, "timed out")).error();
    EXPECT_EQ(error.code(), ErrorCode::kTimeout);
}

// ============================================================================
// value_or Tests
// ============================================================================

TEST(ResultTest, ValueOrReturnsValueOnSuccess) {
    auto result = IntResult::success(42);
    EXPECT_EQ(result.value_or(0), 42);
}

TEST(ResultTest, ValueOrReturnsDefaultOnFailure) {
    auto result = IntResult::failure(Error(ErrorCode::kNotFound, "missing"));
    EXPECT_EQ(result.value_or(-1), -1);
}

TEST(ResultTest, ValueOrWithRvalueReturnsValueOnSuccess) {
    EXPECT_EQ(IntResult::success(42).value_or(0), 42);
}

TEST(ResultTest, ValueOrWithRvalueReturnsDefaultOnFailure) {
    auto result = IntResult::failure(Error(ErrorCode::kNotFound, "missing"));
    EXPECT_EQ(std::move(result).value_or(-1), -1);
}

// ============================================================================
// map() Combinator Tests
// ============================================================================

TEST(ResultTest, MapTransformsSuccessValue) {
    auto result = IntResult::success(5);
    auto doubled = std::move(result).map([](int v) { return v * 2; });

    EXPECT_TRUE(doubled.has_value());
    EXPECT_EQ(doubled.value(), 10);
}

TEST(ResultTest, MapPropagatesError) {
    auto result = IntResult::failure(Error(ErrorCode::kFileNotFound, "missing.txt"));
    auto mapped = std::move(result).map([](int v) { return v * 2; });

    EXPECT_TRUE(mapped.has_error());
    EXPECT_EQ(mapped.error().code(), ErrorCode::kFileNotFound);
}

TEST(ResultTest, MapChangesType) {
    auto result = IntResult::success(42);
    auto as_string = std::move(result).map([](int v) { return std::to_string(v); });

    EXPECT_TRUE(as_string.has_value());
    EXPECT_EQ(as_string.value(), "42");
}

TEST(ResultTest, MapChaining) {
    auto result = IntResult::success(3);
    auto final_result = std::move(result)
                            .map([](int v) { return v + 7; })    // 10
                            .map([](int v) { return v * 2; });   // 20

    EXPECT_TRUE(final_result.has_value());
    EXPECT_EQ(final_result.value(), 20);
}

// ============================================================================
// and_then() Combinator Tests
// ============================================================================

TEST(ResultTest, AndThenChainsSuccessfulOperations) {
    auto divide = [](int v) -> IntResult {
        if (v == 0)
            return IntResult::failure(Error(ErrorCode::kInvalidArgument, "Division by zero"));
        return IntResult::success(100 / v);
    };

    auto result = IntResult::success(5);
    auto divided = std::move(result).and_then(divide);

    EXPECT_TRUE(divided.has_value());
    EXPECT_EQ(divided.value(), 20);
}

TEST(ResultTest, AndThenShortCircuitsOnFirstError) {
    auto divide = [](int v) -> IntResult {
        if (v == 0)
            return IntResult::failure(Error(ErrorCode::kInvalidArgument, "Division by zero"));
        return IntResult::success(100 / v);
    };

    auto result = IntResult::success(0);  // Will cause division by zero
    auto divided = std::move(result).and_then(divide);

    EXPECT_TRUE(divided.has_error());
    EXPECT_EQ(divided.error().code(), ErrorCode::kInvalidArgument);
}

TEST(ResultTest, AndThenPropagatesOriginalError) {
    auto never_called = [](int) -> IntResult {
        ADD_FAILURE() << "This function should never be called";
        return IntResult::success(0);
    };

    auto result = IntResult::failure(Error(ErrorCode::kTimeout, "timed out"));
    auto chained = std::move(result).and_then(never_called);

    EXPECT_TRUE(chained.has_error());
    EXPECT_EQ(chained.error().code(), ErrorCode::kTimeout);
}

// ============================================================================
// or_else() Combinator Tests
// ============================================================================

TEST(ResultTest, OrElseDoesNothingOnSuccess) {
    auto result = IntResult::success(42);
    auto recovered = std::move(result).or_else([](Error&&) -> IntResult {
        ADD_FAILURE() << "Recovery should not be called on success";
        return IntResult::success(0);
    });

    EXPECT_TRUE(recovered.has_value());
    EXPECT_EQ(recovered.value(), 42);
}

TEST(ResultTest, OrElseRecoveriesFromError) {
    auto result = IntResult::failure(Error(ErrorCode::kNotFound, "missing"));
    auto recovered = std::move(result).or_else([](Error&&) -> IntResult {
        return IntResult::success(-1);  // Fallback value
    });

    EXPECT_TRUE(recovered.has_value());
    EXPECT_EQ(recovered.value(), -1);
}

// ============================================================================
// Side-Effect Helper Tests
// ============================================================================

TEST(ResultTest, OnSuccessCalledForSuccess) {
    auto result = IntResult::success(42);
    bool called = false;

    result.on_success([&called](int v) {
        EXPECT_EQ(v, 42);
        called = true;
    });

    EXPECT_TRUE(called);
}

TEST(ResultTest, OnSuccessNotCalledForError) {
    auto result = IntResult::failure(Error(ErrorCode::kUnknown, "err"));
    bool called = false;

    result.on_success([&called](int) { called = true; });

    EXPECT_FALSE(called);
}

TEST(ResultTest, OnErrorCalledForError) {
    auto result = IntResult::failure(Error(ErrorCode::kTimeout, "slow"));
    bool called = false;

    result.on_error([&called](const Error& err) {
        EXPECT_EQ(err.code(), ErrorCode::kTimeout);
        called = true;
    });

    EXPECT_TRUE(called);
}

TEST(ResultTest, OnErrorNotCalledForSuccess) {
    auto result = IntResult::success(1);
    bool called = false;

    result.on_error([&called](const Error&) { called = true; });

    EXPECT_FALSE(called);
}

// ============================================================================
// void Specialization Tests
// ============================================================================

TEST(ResultVoidTest, SuccessCreation) {
    auto result = VoidResult::success();

    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result.has_error());
    EXPECT_TRUE(static_cast<bool>(result));
}

TEST(ResultVoidTest, FailureCreation) {
    auto result = VoidResult::failure(Error(ErrorCode::kFileMoveError, "Access denied"));

    EXPECT_FALSE(result.has_value());
    EXPECT_TRUE(result.has_error());
    EXPECT_EQ(result.error().code(), ErrorCode::kFileMoveError);
}

TEST(ResultVoidTest, AndThenOnSuccess) {
    auto result = VoidResult::success();
    auto chained = std::move(result).and_then([]() -> IntResult {
        return IntResult::success(42);
    });

    EXPECT_TRUE(chained.has_value());
    EXPECT_EQ(chained.value(), 42);
}

TEST(ResultVoidTest, AndThenOnFailure) {
    auto result = VoidResult::failure(Error(ErrorCode::kPermissionDenied, "no access"));
    auto chained = std::move(result).and_then([]() -> IntResult {
        ADD_FAILURE() << "Should not be called";
        return IntResult::success(0);
    });

    EXPECT_TRUE(chained.has_error());
    EXPECT_EQ(chained.error().code(), ErrorCode::kPermissionDenied);
}

TEST(ResultVoidTest, OrElseOnSuccess) {
    auto result = VoidResult::success();
    auto recovered = std::move(result).or_else([](Error&&) -> VoidResult {
        ADD_FAILURE() << "Should not be called";
        return VoidResult::success();
    });

    EXPECT_TRUE(recovered.has_value());
}

TEST(ResultVoidTest, OrElseRecoversFromError) {
    auto result = VoidResult::failure(Error(ErrorCode::kNotFound, "missing"));
    auto recovered = std::move(result).or_else([](Error&&) -> VoidResult {
        return VoidResult::success();
    });

    EXPECT_TRUE(recovered.has_value());
}

TEST(ResultVoidTest, OnSuccessSideEffect) {
    auto result = VoidResult::success();
    bool called = false;

    result.on_success([&called]() { called = true; });

    EXPECT_TRUE(called);
}

TEST(ResultVoidTest, OnErrorSideEffect) {
    auto result = VoidResult::failure(Error(ErrorCode::kUnknown, "err"));
    bool called = false;

    result.on_error([&called](const Error&) { called = true; });

    EXPECT_TRUE(called);
}

}  // namespace lilolify::core::test
