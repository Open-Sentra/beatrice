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

class PacketFilter {
public:
    enum class FilterType {
        BPF,            ///< Berkeley Packet Filter
        PROTOCOL,       ///< Protocol-based filtering
        IP_RANGE,       ///< IP address range filtering
        PORT_RANGE,     ///< Port range filtering
        PAYLOAD,        ///< Payload content filtering
        CUSTOM          ///< Custom filter function
    };

    struct FilterConfig {
        FilterType type = FilterType::PROTOCOL;
        std::string expression;          ///< Filter expression (BPF, regex, etc.)
        bool enabled = true;             ///< Whether filter is active
        int priority = 0;                ///< Filter priority (higher = first)
        std::string description;         ///< Human-readable description
        std::unordered_map<std::string, std::string> parameters; ///< Additional parameters
    };

    struct FilterResult {
        bool passed = false;             ///< Whether packet passed the filter
        std::string filterName;          ///< Name of the filter that processed
        std::chrono::microseconds processingTime{0}; ///< Time taken to process
        std::string reason;              ///< Reason for pass/fail
        std::unordered_map<std::string, std::string> metadata; ///< Additional metadata
    };

    PacketFilter();
    ~PacketFilter();

    Result<void> addFilter(const std::string& name, const FilterConfig& config);

    Result<void> removeFilter(const std::string& name);
    Result<void> setFilterEnabled(const std::string& name, bool enabled);
    FilterResult applyFilters(const Packet& packet);
    std::vector<FilterResult> applyFilters(const std::vector<Packet>& packets);
    std::vector<std::string> getActiveFilters() const;

    struct FilterStats {
        uint64_t packetsProcessed = 0;
        uint64_t packetsPassed = 0;
        uint64_t packetsDropped = 0;
        std::chrono::microseconds totalProcessingTime{0};
        std::unordered_map<std::string, uint64_t> filterCounts;
    };
    FilterStats getStats() const;
    void resetStats();
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