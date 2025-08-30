#include <gtest/gtest.h>
#include "beatrice/Packet.hpp"

TEST(PacketTest, PacketCreation) {
    beatrice::Packet packet;
    EXPECT_EQ(packet.size(), 0);
}

TEST(PacketTest, PacketSize) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto dataPtr = std::make_shared<const uint8_t[]>(data.size());
    std::copy(data.begin(), data.end(), const_cast<uint8_t*>(dataPtr.get()));
    beatrice::Packet packet(dataPtr, data.size());
    EXPECT_EQ(packet.size(), 5);
} 