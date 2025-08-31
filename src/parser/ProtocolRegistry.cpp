#include "parser/ProtocolRegistry.hpp"
#include <algorithm>
#include <chrono>

namespace beatrice {
namespace parser {

ProtocolRegistry& ProtocolRegistry::getInstance() {
    static ProtocolRegistry instance;
    return instance;
}

bool ProtocolRegistry::registerProtocol(const ProtocolDefinition& protocol) {
    std::unique_lock<std::shared_mutex> lock(registryMutex_);
    
    if (protocols_.find(protocol.name) != protocols_.end()) {
        return false;
    }
    
    protocols_[protocol.name] = protocol;
    usageCount_[protocol.name] = 0;
    lastUpdateTime_ = std::chrono::steady_clock::now();
    
    return true;
}

bool ProtocolRegistry::unregisterProtocol(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(registryMutex_);
    
    auto it = protocols_.find(name);
    if (it == protocols_.end()) {
        return false;
    }
    
    protocols_.erase(it);
    usageCount_.erase(name);
    lastUpdateTime_ = std::chrono::steady_clock::now();
    
    return true;
}

bool ProtocolRegistry::hasProtocol(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(registryMutex_);
    return protocols_.find(name) != protocols_.end();
}

const ProtocolDefinition* ProtocolRegistry::getProtocol(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(registryMutex_);
    
    auto it = protocols_.find(name);
    if (it != protocols_.end()) {
        usageCount_[name]++;
        return &it->second;
    }
    
    return nullptr;
}

std::vector<std::string> ProtocolRegistry::getRegisteredProtocols() const {
    std::shared_lock<std::shared_mutex> lock(registryMutex_);
    
    std::vector<std::string> names;
    names.reserve(protocols_.size());
    
    for (const auto& [name, _] : protocols_) {
        names.push_back(name);
    }
    
    return names;
}

size_t ProtocolRegistry::getProtocolCount() const {
    std::shared_lock<std::shared_mutex> lock(registryMutex_);
    return protocols_.size();
}

bool ProtocolRegistry::registerProtocolFactory(const std::string& name, 
                                             std::function<ProtocolDefinition()> factory) {
    std::unique_lock<std::shared_mutex> lock(registryMutex_);
    
    if (factories_.find(name) != factories_.end()) {
        return false;
    }
    
    factories_[name] = factory;
    lastUpdateTime_ = std::chrono::steady_clock::now();
    
    return true;
}

ProtocolDefinition ProtocolRegistry::createProtocol(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(registryMutex_);
    
    auto it = factories_.find(name);
    if (it != factories_.end()) {
        return it->second();
    }
    
    return ProtocolDefinition{};
}

void ProtocolRegistry::clear() {
    std::unique_lock<std::shared_mutex> lock(registryMutex_);
    
    protocols_.clear();
    factories_.clear();
    usageCount_.clear();
    lastUpdateTime_ = std::chrono::steady_clock::now();
}

void ProtocolRegistry::loadBuiltinProtocols() {
    std::unique_lock<std::shared_mutex> lock(registryMutex_);
    initializeBuiltinProtocols();
}

void ProtocolRegistry::initializeBuiltinProtocols() {
    auto ethernet = BuiltinProtocols::createEthernetProtocol();
    auto ipv4 = BuiltinProtocols::createIPv4Protocol();
    auto tcp = BuiltinProtocols::createTCPProtocol();
    auto udp = BuiltinProtocols::createUDPProtocol();
    auto icmp = BuiltinProtocols::createICMPProtocol();
    
    protocols_["ethernet"] = ethernet;
    protocols_["ipv4"] = ipv4;
    protocols_["tcp"] = tcp;
    protocols_["udp"] = udp;
    protocols_["icmp"] = icmp;
    
    for (const auto& [name, _] : protocols_) {
        usageCount_[name] = 0;
    }
    
    lastUpdateTime_ = std::chrono::steady_clock::now();
}

ProtocolRegistry::RegistryStats ProtocolRegistry::getStats() const {
    std::shared_lock<std::shared_mutex> lock(registryMutex_);
    
    RegistryStats stats;
    stats.totalProtocols = protocols_.size();
    stats.builtinProtocols = 5;
    stats.customProtocols = stats.totalProtocols - stats.builtinProtocols;
    stats.lastUpdateTime = std::chrono::duration_cast<std::chrono::microseconds>(
        lastUpdateTime_.time_since_epoch());
    stats.usageCount = usageCount_;
    
    return stats;
}

ProtocolDefinition BuiltinProtocols::createEthernetProtocol() {
    ProtocolDefinition protocol("ethernet", "2.0");
    protocol.description = "Ethernet II Frame";
    
    protocol.addField(FieldFactory::createBytesField("destination_mac", 0, 6, true, "Destination MAC Address"));
    protocol.addField(FieldFactory::createBytesField("source_mac", 6, 6, true, "Source MAC Address"));
    protocol.addField(FieldFactory::createUInt16Field("ethertype", 12, Endianness::NETWORK, true, "EtherType"));
    
    return protocol;
}

ProtocolDefinition BuiltinProtocols::createIPv4Protocol() {
    ProtocolDefinition protocol("ipv4", "4.0");
    protocol.description = "Internet Protocol Version 4";
    
    protocol.addField(FieldFactory::createUInt8Field("version", 0, true, "IP Version"));
    protocol.addField(FieldFactory::createUInt8Field("ihl", 0, true, "Header Length"));
    protocol.addField(FieldFactory::createUInt8Field("tos", 1, true, "Type of Service"));
    protocol.addField(FieldFactory::createUInt16Field("total_length", 2, Endianness::NETWORK, true, "Total Length"));
    protocol.addField(FieldFactory::createUInt16Field("identification", 4, Endianness::NETWORK, true, "Identification"));
    protocol.addField(FieldFactory::createUInt16Field("flags", 6, Endianness::NETWORK, true, "Flags"));
    protocol.addField(FieldFactory::createUInt8Field("ttl", 8, true, "Time to Live"));
    protocol.addField(FieldFactory::createUInt8Field("protocol", 9, true, "Protocol"));
    protocol.addField(FieldFactory::createUInt16Field("checksum", 10, Endianness::NETWORK, true, "Header Checksum"));
    protocol.addField(FieldFactory::createIPv4AddressField("source_ip", 12, true, "Source IP Address"));
    protocol.addField(FieldFactory::createIPv4AddressField("destination_ip", 16, true, "Destination IP Address"));
    
    return protocol;
}

ProtocolDefinition BuiltinProtocols::createIPv6Protocol() {
    ProtocolDefinition protocol("ipv6", "6.0");
    protocol.description = "Internet Protocol Version 6";
    
    protocol.addField(FieldFactory::createUInt32Field("version_traffic_class_flow_label", 0, Endianness::NETWORK, true, "Version, Traffic Class, Flow Label"));
    protocol.addField(FieldFactory::createUInt16Field("payload_length", 4, Endianness::NETWORK, true, "Payload Length"));
    protocol.addField(FieldFactory::createUInt8Field("next_header", 6, true, "Next Header"));
    protocol.addField(FieldFactory::createUInt8Field("hop_limit", 7, true, "Hop Limit"));
    protocol.addField(FieldFactory::createIPv6AddressField("source_ip", 8, true, "Source IP Address"));
    protocol.addField(FieldFactory::createIPv6AddressField("destination_ip", 24, true, "Destination IP Address"));
    
    return protocol;
}

ProtocolDefinition BuiltinProtocols::createTCPProtocol() {
    ProtocolDefinition protocol("tcp", "1.0");
    protocol.description = "Transmission Control Protocol";
    
    protocol.addField(FieldFactory::createUInt16Field("source_port", 0, Endianness::NETWORK, true, "Source Port"));
    protocol.addField(FieldFactory::createUInt16Field("destination_port", 2, Endianness::NETWORK, true, "Destination Port"));
    protocol.addField(FieldFactory::createUInt32Field("sequence_number", 4, Endianness::NETWORK, true, "Sequence Number"));
    protocol.addField(FieldFactory::createUInt32Field("acknowledgment_number", 8, Endianness::NETWORK, true, "Acknowledgment Number"));
    protocol.addField(FieldFactory::createUInt8Field("data_offset", 12, true, "Data Offset"));
    protocol.addField(FieldFactory::createUInt8Field("flags", 13, true, "Flags"));
    protocol.addField(FieldFactory::createUInt16Field("window_size", 14, Endianness::NETWORK, true, "Window Size"));
    protocol.addField(FieldFactory::createUInt16Field("checksum", 16, Endianness::NETWORK, true, "Checksum"));
    protocol.addField(FieldFactory::createUInt16Field("urgent_pointer", 18, Endianness::NETWORK, true, "Urgent Pointer"));
    
    return protocol;
}

ProtocolDefinition BuiltinProtocols::createUDPProtocol() {
    ProtocolDefinition protocol("udp", "1.0");
    protocol.description = "User Datagram Protocol";
    
    protocol.addField(FieldFactory::createUInt16Field("source_port", 0, Endianness::NETWORK, true, "Source Port"));
    protocol.addField(FieldFactory::createUInt16Field("destination_port", 2, Endianness::NETWORK, true, "Destination Port"));
    protocol.addField(FieldFactory::createUInt16Field("length", 4, Endianness::NETWORK, true, "Length"));
    protocol.addField(FieldFactory::createUInt16Field("checksum", 6, Endianness::NETWORK, true, "Checksum"));
    
    return protocol;
}

ProtocolDefinition BuiltinProtocols::createICMPProtocol() {
    ProtocolDefinition protocol("icmp", "1.0");
    protocol.description = "Internet Control Message Protocol";
    
    protocol.addField(FieldFactory::createUInt8Field("type", 0, true, "Type"));
    protocol.addField(FieldFactory::createUInt8Field("code", 1, true, "Code"));
    protocol.addField(FieldFactory::createUInt16Field("checksum", 2, Endianness::NETWORK, true, "Checksum"));
    protocol.addField(FieldFactory::createUInt16Field("identifier", 4, Endianness::NETWORK, true, "Identifier"));
    protocol.addField(FieldFactory::createUInt16Field("sequence_number", 6, Endianness::NETWORK, true, "Sequence Number"));
    
    return protocol;
}

ProtocolDefinition BuiltinProtocols::createHTTPRequestProtocol() {
    ProtocolDefinition protocol("http_request", "1.1");
    protocol.description = "HTTP Request";
    
    protocol.addField(FieldFactory::createStringField("method", 0, 10, true, "HTTP Method"));
    protocol.addField(FieldFactory::createStringField("uri", 10, 100, true, "Request URI"));
    protocol.addField(FieldFactory::createStringField("version", 110, 10, true, "HTTP Version"));
    
    return protocol;
}

ProtocolDefinition BuiltinProtocols::createHTTPResponseProtocol() {
    ProtocolDefinition protocol("http_response", "1.1");
    protocol.description = "HTTP Response";
    
    protocol.addField(FieldFactory::createStringField("version", 0, 10, true, "HTTP Version"));
    protocol.addField(FieldFactory::createUInt16Field("status_code", 10, Endianness::NETWORK, true, "Status Code"));
    protocol.addField(FieldFactory::createStringField("reason_phrase", 12, 50, true, "Reason Phrase"));
    
    return protocol;
}

ProtocolDefinition BuiltinProtocols::createDNSProtocol() {
    ProtocolDefinition protocol("dns", "1.0");
    protocol.description = "Domain Name System";
    
    protocol.addField(FieldFactory::createUInt16Field("transaction_id", 0, Endianness::NETWORK, true, "Transaction ID"));
    protocol.addField(FieldFactory::createUInt16Field("flags", 2, Endianness::NETWORK, true, "Flags"));
    protocol.addField(FieldFactory::createUInt16Field("questions", 4, Endianness::NETWORK, true, "Questions"));
    protocol.addField(FieldFactory::createUInt16Field("answers", 6, Endianness::NETWORK, true, "Answers"));
    protocol.addField(FieldFactory::createUInt16Field("authority", 8, Endianness::NETWORK, true, "Authority"));
    protocol.addField(FieldFactory::createUInt16Field("additional", 10, Endianness::NETWORK, true, "Additional"));
    
    return protocol;
}

ProtocolDefinition BuiltinProtocols::createARPProtocol() {
    ProtocolDefinition protocol("arp", "1.0");
    protocol.description = "Address Resolution Protocol";
    
    protocol.addField(FieldFactory::createUInt16Field("hardware_type", 0, Endianness::NETWORK, true, "Hardware Type"));
    protocol.addField(FieldFactory::createUInt16Field("protocol_type", 2, Endianness::NETWORK, true, "Protocol Type"));
    protocol.addField(FieldFactory::createUInt8Field("hardware_size", 4, true, "Hardware Size"));
    protocol.addField(FieldFactory::createUInt8Field("protocol_size", 5, true, "Protocol Size"));
    protocol.addField(FieldFactory::createUInt16Field("opcode", 6, Endianness::NETWORK, true, "Opcode"));
    protocol.addField(FieldFactory::createBytesField("sender_mac", 8, 6, true, "Sender MAC Address"));
    protocol.addField(FieldFactory::createIPv4AddressField("sender_ip", 14, true, "Sender IP Address"));
    protocol.addField(FieldFactory::createBytesField("target_mac", 18, 6, true, "Target MAC Address"));
    protocol.addField(FieldFactory::createIPv4AddressField("target_ip", 24, true, "Target IP Address"));
    
    return protocol;
}

ProtocolDefinition BuiltinProtocols::createVLANProtocol() {
    ProtocolDefinition protocol("vlan", "1.0");
    protocol.description = "IEEE 802.1Q VLAN Tagging";
    
    protocol.addField(FieldFactory::createUInt16Field("tpid", 0, Endianness::NETWORK, true, "Tag Protocol Identifier"));
    protocol.addField(FieldFactory::createUInt16Field("tci", 2, Endianness::NETWORK, true, "Tag Control Information"));
    
    return protocol;
}

ProtocolDefinition BuiltinProtocols::createMPLSProtocol() {
    ProtocolDefinition protocol("mpls", "1.0");
    protocol.description = "Multiprotocol Label Switching";
    
    protocol.addField(FieldFactory::createUInt32Field("label", 0, Endianness::NETWORK, true, "MPLS Label"));
    
    return protocol;
}

std::vector<ProtocolDefinition> BuiltinProtocols::getAllProtocols() {
    std::vector<ProtocolDefinition> protocols;
    
    protocols.push_back(createEthernetProtocol());
    protocols.push_back(createIPv4Protocol());
    protocols.push_back(createIPv6Protocol());
    protocols.push_back(createTCPProtocol());
    protocols.push_back(createUDPProtocol());
    protocols.push_back(createICMPProtocol());
    protocols.push_back(createHTTPRequestProtocol());
    protocols.push_back(createHTTPResponseProtocol());
    protocols.push_back(createDNSProtocol());
    protocols.push_back(createARPProtocol());
    protocols.push_back(createVLANProtocol());
    protocols.push_back(createMPLSProtocol());
    
    return protocols;
}

void BuiltinProtocols::addCommonIPFields(ProtocolDefinition& protocol) {
    protocol.addField(FieldFactory::createUInt8Field("version", 0, true, "IP Version"));
    protocol.addField(FieldFactory::createUInt8Field("ihl", 0, true, "Header Length"));
    protocol.addField(FieldFactory::createUInt8Field("tos", 1, true, "Type of Service"));
    protocol.addField(FieldFactory::createUInt16Field("total_length", 2, Endianness::NETWORK, true, "Total Length"));
    protocol.addField(FieldFactory::createUInt8Field("ttl", 8, true, "Time to Live"));
    protocol.addField(FieldFactory::createUInt8Field("protocol", 9, true, "Protocol"));
    protocol.addField(FieldFactory::createUInt16Field("checksum", 10, Endianness::NETWORK, true, "Header Checksum"));
}

void BuiltinProtocols::addCommonTCPFields(ProtocolDefinition& protocol) {
    protocol.addField(FieldFactory::createUInt16Field("source_port", 0, Endianness::NETWORK, true, "Source Port"));
    protocol.addField(FieldFactory::createUInt16Field("destination_port", 2, Endianness::NETWORK, true, "Destination Port"));
    protocol.addField(FieldFactory::createUInt32Field("sequence_number", 4, Endianness::NETWORK, true, "Sequence Number"));
    protocol.addField(FieldFactory::createUInt32Field("acknowledgment_number", 8, Endianness::NETWORK, true, "Acknowledgment Number"));
    protocol.addField(FieldFactory::createUInt16Field("window_size", 14, Endianness::NETWORK, true, "Window Size"));
    protocol.addField(FieldFactory::createUInt16Field("checksum", 16, Endianness::NETWORK, true, "Checksum"));
}

void BuiltinProtocols::addCommonUDPFields(ProtocolDefinition& protocol) {
    protocol.addField(FieldFactory::createUInt16Field("source_port", 0, Endianness::NETWORK, true, "Source Port"));
    protocol.addField(FieldFactory::createUInt16Field("destination_port", 2, Endianness::NETWORK, true, "Destination Port"));
    protocol.addField(FieldFactory::createUInt16Field("length", 4, Endianness::NETWORK, true, "Length"));
    protocol.addField(FieldFactory::createUInt16Field("checksum", 6, Endianness::NETWORK, true, "Checksum"));
}

ProtocolDetector::DetectionResult ProtocolDetector::detectProtocol(const std::vector<uint8_t>& packet) {
    DetectionResult result;
    result.detectionTime = std::chrono::microseconds(0);
    
    if (packet.size() < 14) {
        result.protocolName = "unknown";
        result.confidence = 0.0;
        result.reason = "Packet too short";
        return result;
    }
    
    if (isEthernet(packet)) {
        result.protocolName = "ethernet";
        result.confidence = 0.95;
        result.reason = "Valid Ethernet frame";
        
        if (packet.size() >= 34) {
            uint8_t protocol = packet[23];
            if (protocol == 6) {
                result.protocolName = "tcp";
                result.confidence = 0.98;
                result.reason = "Ethernet + IPv4 + TCP";
            } else if (protocol == 17) {
                result.protocolName = "udp";
                result.confidence = 0.98;
                result.reason = "Ethernet + IPv4 + UDP";
            } else if (protocol == 1) {
                result.protocolName = "icmp";
                result.confidence = 0.98;
                result.reason = "Ethernet + IPv4 + ICMP";
            }
        }
    }
    
    return result;
}

std::vector<ProtocolDetector::DetectionResult> ProtocolDetector::detectMultipleProtocols(const std::vector<uint8_t>& packet) {
    std::vector<DetectionResult> results;
    
    results.push_back(detectProtocol(packet));
    
    if (packet.size() >= 34) {
        uint8_t protocol = packet[23];
        
        if (protocol == 6) {
            DetectionResult tcpResult;
            tcpResult.protocolName = "tcp";
            tcpResult.confidence = 0.98;
            tcpResult.reason = "TCP over IPv4";
            tcpResult.detectionTime = std::chrono::microseconds(0);
            results.push_back(tcpResult);
        } else if (protocol == 17) {
            DetectionResult udpResult;
            udpResult.protocolName = "udp";
            udpResult.confidence = 0.98;
            udpResult.reason = "UDP over IPv4";
            udpResult.detectionTime = std::chrono::microseconds(0);
            results.push_back(udpResult);
        }
    }
    
    return results;
}

bool ProtocolDetector::isEthernet(const std::vector<uint8_t>& packet) {
    if (packet.size() < 14) return false;
    
    uint16_t ethertype = (packet[12] << 8) | packet[13];
    return ethertype == 0x0800 || ethertype == 0x0806 || ethertype == 0x86DD;
}

bool ProtocolDetector::isIPv4(const std::vector<uint8_t>& packet) {
    if (packet.size() < 34) return false;
    
    uint8_t version = packet[0] >> 4;
    return version == 4;
}

bool ProtocolDetector::isIPv6(const std::vector<uint8_t>& packet) {
    if (packet.size() < 54) return false;
    
    uint8_t version = packet[0] >> 4;
    return version == 6;
}

bool ProtocolDetector::isTCP(const std::vector<uint8_t>& packet) {
    if (!isIPv4(packet)) return false;
    
    uint8_t protocol = packet[23];
    return protocol == 6;
}

bool ProtocolDetector::isUDP(const std::vector<uint8_t>& packet) {
    if (!isIPv4(packet)) return false;
    
    uint8_t protocol = packet[23];
    return protocol == 17;
}

bool ProtocolDetector::isICMP(const std::vector<uint8_t>& packet) {
    if (!isIPv4(packet)) return false;
    
    uint8_t protocol = packet[23];
    return protocol == 1;
}

bool ProtocolDetector::isHTTP(const std::vector<uint8_t>& packet) {
    if (!isTCP(packet)) return false;
    
    if (packet.size() < 54) return false;
    
    uint16_t destPort = (packet[36] << 8) | packet[37];
    uint16_t srcPort = (packet[34] << 8) | packet[35];
    
    return destPort == 80 || srcPort == 80;
}

bool ProtocolDetector::isDNS(const std::vector<uint8_t>& packet) {
    if (!isUDP(packet)) return false;
    
    if (packet.size() < 42) return false;
    
    uint16_t destPort = (packet[36] << 8) | packet[37];
    uint16_t srcPort = (packet[34] << 8) | packet[35];
    
    return destPort == 53 || srcPort == 53;
}

bool ProtocolDetector::isARP(const std::vector<uint8_t>& packet) {
    if (packet.size() < 28) return false;
    
    uint16_t ethertype = (packet[12] << 8) | packet[13];
    return ethertype == 0x0806;
}

double ProtocolDetector::calculateConfidence(const std::vector<uint8_t>& packet, const ProtocolDefinition& protocol) {
    if (packet.size() < protocol.getTotalLength()) {
        return 0.0;
    }
    
    double confidence = 0.5;
    
    for (const auto& field : protocol.fields) {
        if (field.offset + field.length <= packet.size()) {
            confidence += 0.1;
        }
    }
    
    return std::min(confidence, 1.0);
}

bool ProtocolDetector::validateChecksum(const std::vector<uint8_t>& packet, const ProtocolDefinition& protocol) {
    return true;
}

} // namespace parser
} // namespace beatrice 