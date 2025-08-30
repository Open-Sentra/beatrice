#ifndef BEATRICE_PACKET_HPP
#define BEATRICE_PACKET_HPP

#include <cstdint>
#include <chrono>
#include <string>
#include <memory>
#include <vector>
#include <optional>

namespace beatrice {

/**
 * @brief Network packet structure for Beatrice SDK
 * 
 * Represents a captured network packet with metadata and zero-copy data access.
 * The packet data is managed through smart pointers to ensure proper memory management.
 */
class Packet {
public:
    /**
     * @brief Packet metadata structure
     */
    struct Metadata {
        std::string interface;           ///< Network interface name
        std::string source_mac;          ///< Source MAC address
        std::string destination_mac;     ///< Destination MAC address
        std::string source_ip;           ///< Source IP address
        std::string destination_ip;      ///< Destination IP address
        uint16_t source_port;            ///< Source port (for TCP/UDP)
        uint16_t destination_port;       ///< Destination port (for TCP/UDP)
        uint8_t protocol;                ///< Protocol number (TCP=6, UDP=17, etc.)
        uint16_t vlan_id;                ///< VLAN ID if applicable
        bool is_ipv6;                    ///< True if IPv6 packet
        bool is_fragment;                ///< True if IP fragment
        uint16_t fragment_offset;        ///< Fragment offset
        uint8_t ttl;                     ///< Time to live
        uint8_t tos;                     ///< Type of service
        uint32_t flow_label;             ///< IPv6 flow label
    };

    /**
     * @brief Default constructor
     */
    Packet() = default;

    /**
     * @brief Constructor with data
     * @param data Packet data
     * @param length Data length
     * @param timestamp Capture timestamp
     */
    Packet(std::shared_ptr<const uint8_t[]> data, size_t length, 
           std::chrono::steady_clock::time_point timestamp = std::chrono::steady_clock::now());

    /**
     * @brief Copy constructor
     * @param other Packet to copy
     */
    Packet(const Packet& other) = default;

    /**
     * @brief Move constructor
     * @param other Packet to move from
     */
    Packet(Packet&& other) noexcept = default;

    /**
     * @brief Copy assignment operator
     * @param other Packet to copy
     * @return Reference to this packet
     */
    Packet& operator=(const Packet& other) = default;

    /**
     * @brief Move assignment operator
     * @param other Packet to move from
     * @return Reference to this packet
     */
    Packet& operator=(Packet&& other) noexcept = default;

    /**
     * @brief Destructor
     */
    ~Packet() = default;

    /**
     * @brief Get packet data
     * @return Pointer to packet data
     */
    const uint8_t* data() const noexcept { return data_.get(); }

    /**
     * @brief Get packet data as shared pointer
     * @return Shared pointer to packet data
     */
    std::shared_ptr<const uint8_t[]> getData() const noexcept { return data_; }

    /**
     * @brief Get packet length
     * @return Packet length in bytes
     */
    size_t length() const noexcept { return length_; }

    /**
     * @brief Get capture timestamp
     * @return Capture timestamp
     */
    const std::chrono::steady_clock::time_point& timestamp() const noexcept { return timestamp_; }

    /**
     * @brief Get packet metadata
     * @return Packet metadata
     */
    const Metadata& metadata() const noexcept { return metadata_; }

    /**
     * @brief Get mutable packet metadata
     * @return Mutable packet metadata
     */
    Metadata& metadata() noexcept { return metadata_; }

    /**
     * @brief Set packet metadata
     * @param metadata New metadata
     */
    void setMetadata(const Metadata& metadata) { metadata_ = metadata; }

    /**
     * @brief Check if packet is empty
     * @return true if packet has no data
     */
    bool empty() const noexcept { return !data_ || length_ == 0; }

    /**
     * @brief Get packet size
     * @return Packet size in bytes
     */
    size_t size() const noexcept { return length_; }

    /**
     * @brief Get packet as vector
     * @return Vector containing packet data
     */
    std::vector<uint8_t> toVector() const;

    /**
     * @brief Get packet as string
     * @return String containing packet data
     */
    std::string toString() const;

    /**
     * @brief Get packet as hex string
     * @return Hex string representation of packet data
     */
    std::string toHexString() const;

    /**
     * @brief Get packet header (first N bytes)
     * @param bytes Number of bytes to get
     * @return Vector containing header bytes
     */
    std::vector<uint8_t> getHeader(size_t bytes) const;

    /**
     * @brief Get packet payload (data after header)
     * @param headerSize Header size in bytes
     * @return Vector containing payload data
     */
    std::vector<uint8_t> getPayload(size_t headerSize) const;

    /**
     * @brief Check if packet has specific protocol
     * @param protocol Protocol number to check
     * @return true if packet has specified protocol
     */
    bool hasProtocol(uint8_t protocol) const noexcept { return metadata_.protocol == protocol; }

    /**
     * @brief Check if packet is TCP
     * @return true if TCP packet
     */
    bool isTCP() const noexcept { return hasProtocol(6); }

    /**
     * @brief Check if packet is UDP
     * @return true if UDP packet
     */
    bool isUDP() const noexcept { return hasProtocol(17); }

    /**
     * @brief Check if packet is ICMP
     * @return true if ICMP packet
     */
    bool isICMP() const noexcept { return hasProtocol(1); }

    /**
     * @brief Check if packet is IPv6
     * @return true if IPv6 packet
     */
    bool isIPv6() const noexcept { return metadata_.is_ipv6; }

    /**
     * @brief Check if packet is IPv4
     * @return true if IPv4 packet
     */
    bool isIPv4() const noexcept { return !metadata_.is_ipv6; }

private:
    std::shared_ptr<const uint8_t[]> data_;                    ///< Packet data
    size_t length_{0};                                          ///< Data length
    std::chrono::steady_clock::time_point timestamp_;           ///< Capture timestamp
    Metadata metadata_;                                          ///< Packet metadata
};

} // namespace beatrice

#endif // BEATRICE_PACKET_HPP
