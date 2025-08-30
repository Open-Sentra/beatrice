#include <gtest/gtest.h>
#include "beatrice/BeatriceContext.hpp"
#include "beatrice/AF_XDPBackend.hpp"
#include "beatrice/PluginManager.hpp"

TEST(BeatriceContextTest, ContextCreation) {
    auto backend = std::make_unique<beatrice::AF_XDPBackend>();
    auto pluginMgr = std::make_unique<beatrice::PluginManager>();
    
    beatrice::BeatriceContext context(std::move(backend), std::move(pluginMgr));
    EXPECT_NE(&context, nullptr);
} 