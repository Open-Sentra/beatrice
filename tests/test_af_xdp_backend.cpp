#include <gtest/gtest.h>
#include "beatrice/AF_XDPBackend.hpp"

TEST(AF_XDPBackendTest, BackendCreation) {
    beatrice::AF_XDPBackend backend;
    EXPECT_EQ(backend.getName(), "AF_XDP Backend");
} 