#include <gtest/gtest.h>
#include "beatrice/DPDKBackend.hpp"
#include "beatrice/AF_XDPBackend.hpp"

TEST(PerformanceTest, DPDKBackendCreation) {
    auto backend = std::make_unique<beatrice::DPDKBackend>();
    EXPECT_NE(backend, nullptr);
}

TEST(PerformanceTest, AF_XDPBackendCreation) {
    auto backend = std::make_unique<beatrice::AF_XDPBackend>();
    EXPECT_NE(backend, nullptr);
} 