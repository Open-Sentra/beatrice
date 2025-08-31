#include <gtest/gtest.h>
#include "beatrice/Error.hpp"

TEST(ErrorTest, BeatriceExceptionCreation) {
    beatrice::BeatriceException exception("Test error");
    EXPECT_EQ(exception.getMessage(), "Test error");
}

TEST(ErrorTest, ErrorCodeTest) {
    beatrice::BeatriceException exception("Test error", beatrice::ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(exception.getCode(), beatrice::ErrorCode::INVALID_ARGUMENT);
}

TEST(ErrorTest, ResultSuccessTest) {
    auto result = beatrice::Result<int>::success(42);
    EXPECT_TRUE(result.isSuccess());
    EXPECT_EQ(result.getValue(), 42);
}

TEST(ErrorTest, ResultErrorTest) {
    auto result = beatrice::Result<int>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "Invalid input");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.getErrorCode(), beatrice::ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(result.getErrorMessage(), "Invalid input");
} 