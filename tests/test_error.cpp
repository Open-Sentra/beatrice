#include <gtest/gtest.h>
#include "beatrice/Error.hpp"

TEST(ErrorTest, ErrorCreation) {
    beatrice::Error error("Test error");
    EXPECT_EQ(error.getMessage(), "Test error");
} 