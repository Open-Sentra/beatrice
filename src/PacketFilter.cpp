#include "beatrice/PacketFilter.hpp"
#include "beatrice/Logger.hpp"
#include <algorithm>
#include <sstream>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/if_ether.h>

namespace beatrice {

PacketFilter::PacketFilter() {
}

PacketFilter::~PacketFilter() {
}

Result<void> PacketFilter::addFilter(const std::string& name, const FilterConfig& config) {
    std::lock_guard<std::mutex> lock(filtersMutex_);
    
    if (filters_.find(name) != filters_.end()) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, "Filter already exists: " + name);
    }
    
    FilterEntry entry;
    entry.config = config;
    filters_[name] = entry;
    
    return Result<void>::success();
}

Result<void> PacketFilter::removeFilter(const std::string& name) {
    std::lock_guard<std::mutex> lock(filtersMutex_);
    
    auto it = filters_.find(name);
    if (it == filters_.end()) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, "Filter not found: " + name);
    }
    
    filters_.erase(it);
    return Result<void>::success();
}

Result<void> PacketFilter::setFilterEnabled(const std::string& name, bool enabled) {
    std::lock_guard<std::mutex> lock(filtersMutex_);
    
    auto it = filters_.find(name);
    if (it == filters_.end()) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, "Filter not found: " + name);
    }
    
    it->second.config.enabled = enabled;
    return Result<void>::success();
}

PacketFilter::FilterResult PacketFilter::applyFilters(const Packet& packet) {
    FilterResult result;
    result.passed = true;
    
    std::lock_guard<std::mutex> lock(filtersMutex_);
    
    std::vector<std::pair<std::string, FilterEntry*>> sortedFilters;
    for (auto& [name, entry] : filters_) {
        if (entry.config.enabled) {
            sortedFilters.emplace_back(name, &entry);
        }
    }
    
    std::sort(sortedFilters.begin(), sortedFilters.end(), 
              [](const auto& a, const auto& b) {
                  return a.second->config.priority > b.second->config.priority;
              });
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (auto& [name, entry] : sortedFilters) {
        if (!entry->config.enabled) continue;
        
        bool filterResult = false;
        switch (entry->config.type) {
            case FilterType::BPF:
                filterResult = applyBPFFilter(packet, entry->config);
                break;
            case FilterType::PROTOCOL:
                filterResult = applyProtocolFilter(packet, entry->config);
                break;
            case FilterType::IP_RANGE:
                filterResult = applyIPRangeFilter(packet, entry->config);
                break;
            case FilterType::PORT_RANGE:
                filterResult = applyPortRangeFilter(packet, entry->config);
                break;
            case FilterType::PAYLOAD:
                filterResult = applyPayloadFilter(packet, entry->config);
                break;
            case FilterType::CUSTOM:
                filterResult = applyCustomFilter(packet, *entry);
                break;
        }
        
        if (!filterResult) {
            result.passed = false;
            result.filterName = name;
            result.reason = "Filter " + name + " rejected packet";
            break;
        }
        
        result.filterName = name;
        result.reason = "Packet passed all filters";
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    result.processingTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    updateStats(result.filterName, result.passed, result.processingTime);
    
    return result;
}

std::vector<PacketFilter::FilterResult> PacketFilter::applyFilters(const std::vector<Packet>& packets) {
    std::vector<FilterResult> results;
    results.reserve(packets.size());
    
    for (const auto& packet : packets) {
        results.push_back(applyFilters(packet));
    }
    
    return results;
}

std::vector<std::string> PacketFilter::getActiveFilters() const {
    std::lock_guard<std::mutex> lock(filtersMutex_);
    
    std::vector<std::string> activeFilters;
    for (const auto& [name, entry] : filters_) {
        if (entry.config.enabled) {
            activeFilters.push_back(name);
        }
    }
    
    return activeFilters;
}

PacketFilter::FilterStats PacketFilter::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void PacketFilter::resetStats() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_ = FilterStats{};
}

Result<void> PacketFilter::setCustomFilter(const std::string& name, 
                                         std::function<bool(const Packet&)> filterFunc) {
    std::lock_guard<std::mutex> lock(filtersMutex_);
    
    auto it = filters_.find(name);
    if (it == filters_.end()) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, "Filter not found: " + name);
    }
    
    it->second.customFunc = std::move(filterFunc);
    return Result<void>::success();
}

bool PacketFilter::applyBPFFilter(const Packet& packet, const FilterConfig& config) {
    if (config.expression.empty()) return true;
    
    const uint8_t* data = packet.data();
    size_t length = packet.length();
    
    if (length < sizeof(struct ether_header)) return false;
    
    struct ether_header* eth = (struct ether_header*)data;
    uint16_t ethType = ntohs(eth->ether_type);
    
    if (ethType == ETHERTYPE_IP) {
        if (length < sizeof(struct ether_header) + sizeof(struct iphdr)) return false;
        
        struct iphdr* ip = (struct iphdr*)(data + sizeof(struct ether_header));
        uint8_t protocol = ip->protocol;
        
        if (config.expression.find("tcp") != std::string::npos && protocol == IPPROTO_TCP) return true;
        if (config.expression.find("udp") != std::string::npos && protocol == IPPROTO_UDP) return true;
        if (config.expression.find("icmp") != std::string::npos && protocol == IPPROTO_ICMP) return true;
    }
    
    return false;
}

bool PacketFilter::applyProtocolFilter(const Packet& packet, const FilterConfig& config) {
    if (config.expression.empty()) return true;
    
    const uint8_t* data = packet.data();
    size_t length = packet.length();
    
    if (length < sizeof(struct ether_header)) return false;
    
    struct ether_header* eth = (struct ether_header*)data;
    uint16_t ethType = ntohs(eth->ether_type);
    
    if (ethType == ETHERTYPE_IP) {
        if (length < sizeof(struct ether_header) + sizeof(struct iphdr)) return false;
        
        struct iphdr* ip = (struct iphdr*)(data + sizeof(struct ether_header));
        uint8_t protocol = ip->protocol;
        
        if (config.expression == "tcp" && protocol == IPPROTO_TCP) return true;
        if (config.expression == "udp" && protocol == IPPROTO_UDP) return true;
        if (config.expression == "icmp" && protocol == IPPROTO_ICMP) return true;
        if (config.expression == "ip" && protocol != 0) return true;
    }
    
    return false;
}

bool PacketFilter::applyIPRangeFilter(const Packet& packet, const FilterConfig& config) {
    if (config.expression.empty()) return true;
    
    const uint8_t* data = packet.data();
    size_t length = packet.length();
    
    if (length < sizeof(struct ether_header) + sizeof(struct iphdr)) return false;
    
    struct ether_header* eth = (struct ether_header*)data;
    uint16_t ethType = ntohs(eth->ether_type);
    
    if (ethType == ETHERTYPE_IP) {
        struct iphdr* ip = (struct iphdr*)(data + sizeof(struct ether_header));
        uint32_t srcIP = ntohl(ip->saddr);
        uint32_t dstIP = ntohl(ip->daddr);
        
        std::vector<uint8_t> srcIPBytes = {(uint8_t)(srcIP >> 24), (uint8_t)(srcIP >> 16), 
                                          (uint8_t)(srcIP >> 8), (uint8_t)srcIP};
        std::vector<uint8_t> dstIPBytes = {(uint8_t)(dstIP >> 24), (uint8_t)(dstIP >> 16), 
                                          (uint8_t)(dstIP >> 8), (uint8_t)dstIP};
        
        if (isIPInRange(srcIPBytes, config.expression) || 
            isIPInRange(dstIPBytes, config.expression)) {
            return true;
        }
    }
    
    return false;
}

bool PacketFilter::applyPortRangeFilter(const Packet& packet, const FilterConfig& config) {
    if (config.expression.empty()) return true;
    
    const uint8_t* data = packet.data();
    size_t length = packet.length();
    
    if (length < sizeof(struct ether_header) + sizeof(struct iphdr)) return false;
    
    struct ether_header* eth = (struct ether_header*)data;
    uint16_t ethType = ntohs(eth->ether_type);
    
    if (ethType == ETHERTYPE_IP) {
        struct iphdr* ip = (struct iphdr*)(data + sizeof(struct ether_header));
        uint8_t protocol = ip->protocol;
        
        if (protocol == IPPROTO_TCP && length >= sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct tcphdr)) {
            struct tcphdr* tcp = (struct tcphdr*)(data + sizeof(struct ether_header) + sizeof(struct iphdr));
            uint16_t srcPort = ntohs(tcp->source);
            uint16_t dstPort = ntohs(tcp->dest);
            
            if (isPortInRange(srcPort, config.expression) || 
                isPortInRange(dstPort, config.expression)) {
                return true;
            }
        } else if (protocol == IPPROTO_UDP && length >= sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct udphdr)) {
            struct udphdr* udp = (struct udphdr*)(data + sizeof(struct ether_header) + sizeof(struct iphdr));
            uint16_t srcPort = ntohs(udp->source);
            uint16_t dstPort = ntohs(udp->dest);
            
            if (isPortInRange(srcPort, config.expression) || 
                isPortInRange(dstPort, config.expression)) {
                return true;
            }
        }
    }
    
    return false;
}

bool PacketFilter::applyPayloadFilter(const Packet& packet, const FilterConfig& config) {
    if (config.expression.empty()) return true;
    
    const uint8_t* data = packet.data();
    size_t length = packet.length();
    
    if (length < sizeof(struct ether_header) + sizeof(struct iphdr)) return false;
    
    struct ether_header* eth = (struct ether_header*)data;
    uint16_t ethType = ntohs(eth->ether_type);
    
    if (ethType == ETHERTYPE_IP) {
        struct iphdr* ip = (struct iphdr*)(data + sizeof(struct ether_header));
        size_t ipHeaderLen = (ip->ihl & 0x0F) * 4;
        size_t payloadOffset = sizeof(struct ether_header) + ipHeaderLen;
        
        if (length > payloadOffset) {
            size_t payloadLength = length - payloadOffset;
            const uint8_t* payload = data + payloadOffset;
            
            std::string payloadStr(reinterpret_cast<const char*>(payload), 
                                 std::min(payloadLength, size_t(100)));
            
            try {
                std::regex pattern(config.expression);
                return std::regex_search(payloadStr, pattern);
            } catch (const std::regex_error&) {
                return false;
            }
        }
    }
    
    return false;
}

bool PacketFilter::applyCustomFilter(const Packet& packet, const FilterEntry& entry) {
    if (entry.customFunc) {
        return entry.customFunc(packet);
    }
    return true;
}

std::vector<uint8_t> PacketFilter::parseIPAddress(const std::string& ipStr) {
    std::vector<uint8_t> result;
    std::stringstream ss(ipStr);
    std::string token;
    
    while (std::getline(ss, token, '.')) {
        result.push_back(static_cast<uint8_t>(std::stoi(token)));
    }
    
    return result;
}

bool PacketFilter::isIPInRange(const std::vector<uint8_t>& ip, const std::string& range) {
    if (range.find('/') != std::string::npos) {
        size_t pos = range.find('/');
        std::string networkStr = range.substr(0, pos);
        int prefixLen = std::stoi(range.substr(pos + 1));
        
        auto network = parseIPAddress(networkStr);
        if (network.size() != 4) return false;
        
        uint32_t networkAddr = (network[0] << 24) | (network[1] << 16) | (network[2] << 8) | network[3];
        uint32_t ipAddr = (ip[0] << 24) | (ip[1] << 16) | (ip[2] << 8) | ip[3];
        
        uint32_t mask = (0xFFFFFFFF << (32 - prefixLen)) & 0xFFFFFFFF;
        return (networkAddr & mask) == (ipAddr & mask);
    } else {
        auto rangeIP = parseIPAddress(range);
        return ip == rangeIP;
    }
}

bool PacketFilter::isPortInRange(uint16_t port, const std::string& range) {
    if (range.find('-') != std::string::npos) {
        size_t pos = range.find('-');
        uint16_t startPort = static_cast<uint16_t>(std::stoi(range.substr(0, pos)));
        uint16_t endPort = static_cast<uint16_t>(std::stoi(range.substr(pos + 1)));
        return port >= startPort && port <= endPort;
    } else {
        uint16_t targetPort = static_cast<uint16_t>(std::stoi(range));
        return port == targetPort;
    }
}

void PacketFilter::updateStats(const std::string& filterName, bool passed, std::chrono::microseconds time) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    
    stats_.packetsProcessed++;
    if (passed) {
        stats_.packetsPassed++;
    } else {
        stats_.packetsDropped++;
    }
    stats_.totalProcessingTime += time;
    stats_.filterCounts[filterName]++;
}

} // namespace beatrice 