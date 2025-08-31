#ifndef BEATRICE_PACKET_FILTER_HPP
#define BEATRICE_PACKET_FILTER_HPP

#include "Packet.hpp"
#include "Error.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <regex>

namespace beatrice {

/**
 * @brief Packet filtering engine with BPF support and custom filters
 */
class PacketFilter {
public:
    /**
     * @brief Filter types supported by the engine
     */
    enum class FilterType {
        BPF,            ///< Berkeley Packet Filter
        PROTOCOL,       ///< Protocol-based filtering
        IP_RANGE,       ///< IP address range filtering
        PORT_RANGE,     ///< Port range filtering
        PAYLOAD,        ///< Payload content filtering
        CUSTOM          ///< Custom filter function
    };

    /**
     * @brief Filter configuration structure
     */
    struct FilterConfig {
        FilterType type = FilterType::PROTOCOL;
        std::string expression;          ///< Filter expression (BPF, regex, etc.)
        bool enabled = true;             ///< Whether filter is active
        int priority = 0;                ///< Filter priority (higher = first)
        std::string description;         ///< Human-readable description
        std::unordered_map<std::string, std::string> parameters; ///< Additional parameters
    };

    /**
     * @brief Filter result with metadata
     */
    struct FilterResult {
        bool passed = false;             ///< Whether packet passed the filter
        std::string filterName;          ///< Name of the filter that processed
        std::chrono::microseconds processingTime{0}; ///< Time taken to process
        std::string reason;              ///< Reason for pass/fail
        std::unordered_map<std::string, std::string> metadata; ///< Additional metadata
    };

    PacketFilter();
    ~PacketFilter();

    /**
     * @brief Add a new filter to the chain
     * @param name Filter name
     * @param config Filter configuration
     * @return Result indicating success/failure
     */
    Result<void> addFilter(const std::string& name, const FilterConfig& config);

    /**
     * @brief Remove a filter by name
     * @param name Filter name
     * @return Result indicating success/failure
     */
    Result<void> removeFilter(const std::string& name);

    /**
     * @brief Enable/disable a filter
     * @param name Filter name
     * @param enabled Whether to enable the filter
     * @return Result indicating success/failure
     */
    Result<void> setFilterEnabled(const std::string& name, bool enabled);

    /**
     * @brief Apply all active filters to a packet
     * @param packet Packet to filter
     * @return Filter result
     */
    FilterResult applyFilters(const Packet& packet);

    /**
     * @brief Apply filters to multiple packets
     * @param packets Vector of packets to filter
     * @return Vector of filter results
     */
    std::vector<FilterResult> applyFilters(const std::vector<Packet>& packets);

    /**
     * @brief Get list of active filters
     * @return Vector of filter names
     */
    std::vector<std::string> getActiveFilters() const;

    /**
     * @brief Get filter statistics
     * @return Statistics about filter usage
     */
    struct FilterStats {
        uint64_t packetsProcessed = 0;
        uint64_t packetsPassed = 0;
        uint64_t packetsDropped = 0;
        std::chrono::microseconds totalProcessingTime{0};
        std::unordered_map<std::string, uint64_t> filterCounts;
    };
    FilterStats getStats() const;

    /**
     * @brief Reset filter statistics
     */
    void resetStats();

    /**
     * @brief Set custom filter function
     * @param name Filter name
     * @param filterFunc Custom filter function
     * @return Result indicating success/failure
     */
    Result<void> setCustomFilter(const std::string& name, 
                                std::function<bool(const Packet&)> filterFunc);

private:
    struct FilterEntry {
        FilterConfig config;
        std::function<bool(const Packet&)> customFunc;
        uint64_t packetsProcessed = 0;
        uint64_t packetsPassed = 0;
        uint64_t packetsDropped = 0;
        std::chrono::microseconds totalTime{0};
    };

    std::unordered_map<std::string, FilterEntry> filters_;
    mutable std::mutex filtersMutex_;
    FilterStats stats_;
    mutable std::mutex statsMutex_;

    // Internal filter methods
    bool applyBPFFilter(const Packet& packet, const FilterConfig& config);
    bool applyProtocolFilter(const Packet& packet, const FilterConfig& config);
    bool applyIPRangeFilter(const Packet& packet, const FilterConfig& config);
    bool applyPortRangeFilter(const Packet& packet, const FilterConfig& config);
    bool applyPayloadFilter(const Packet& packet, const FilterConfig& config);
    bool applyCustomFilter(const Packet& packet, const FilterEntry& entry);

    // Helper methods
    std::vector<uint8_t> parseIPAddress(const std::string& ipStr);
    bool isIPInRange(const std::vector<uint8_t>& ip, const std::string& range);
    bool isPortInRange(uint16_t port, const std::string& range);
    void updateStats(const std::string& filterName, bool passed, std::chrono::microseconds time);
};

} // namespace beatrice

#endif // BEATRICE_PACKET_FILTER_HPP 