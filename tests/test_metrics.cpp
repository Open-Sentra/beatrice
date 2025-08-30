#include <gtest/gtest.h>
#include "beatrice/Metrics.hpp"

TEST(MetricsTest, MetricsCreation) {
    beatrice::metrics::Metrics metrics;
    EXPECT_NE(&metrics, nullptr);
} 