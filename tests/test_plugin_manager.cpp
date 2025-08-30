#include <gtest/gtest.h>
#include "beatrice/PluginManager.hpp"

TEST(PluginManagerTest, PluginManagerCreation) {
    beatrice::PluginManager manager;
    EXPECT_NE(&manager, nullptr);
} 