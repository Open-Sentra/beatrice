#include <gtest/gtest.h>
#include "beatrice/Metrics.hpp"

TEST(MetricsTest, CounterCreation) {
    auto& registry = beatrice::MetricsRegistry::get();
    auto counter = registry.createCounter("test_counter", "Test counter");
    EXPECT_NE(counter, nullptr);
    EXPECT_EQ(counter->getName(), "test_counter");
}

TEST(MetricsTest, CounterIncrement) {
    auto& registry = beatrice::MetricsRegistry::get();
    auto counter = registry.createCounter("test_counter", "Test counter");
    
    EXPECT_EQ(counter->getValue(), 0.0);
    counter->increment();
    EXPECT_EQ(counter->getValue(), 1.0);
    counter->increment(5.0);
    EXPECT_EQ(counter->getValue(), 6.0);
}

TEST(MetricsTest, GaugeCreation) {
    auto& registry = beatrice::MetricsRegistry::get();
    auto gauge = registry.createGauge("test_gauge", "Test gauge");
    EXPECT_NE(gauge, nullptr);
    EXPECT_EQ(gauge->getName(), "test_gauge");
}

TEST(MetricsTest, HistogramCreation) {
    auto& registry = beatrice::MetricsRegistry::get();
    auto histogram = registry.createHistogram("test_histogram", "Test histogram");
    EXPECT_NE(histogram, nullptr);
    EXPECT_EQ(histogram->getName(), "test_histogram");
} 