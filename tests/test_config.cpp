#include <gtest/gtest.h>
#include "beatrice/Config.hpp"

TEST(ConfigTest, ConfigCreation) {
    beatrice::Config config;
    EXPECT_NE(&config, nullptr);
} 