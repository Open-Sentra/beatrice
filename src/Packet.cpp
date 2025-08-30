#include "beatrice/Packet.hpp"
#include <cstring>

namespace beatrice {

Packet::Packet(std::shared_ptr<const uint8_t[]> data, size_t length, 
               std::chrono::steady_clock::time_point timestamp)
    : data_(data), length_(length), timestamp_(timestamp) {
}

// Other methods can be added here if needed

} // namespace beatrice 