#ifndef BEATRICE_PARSER_PROTOCOL_REGISTRY_HPP
#define BEATRICE_PARSER_PROTOCOL_REGISTRY_HPP

#include "FieldDefinition.hpp"
#include "ParserResult.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <shared_mutex>

namespace beatrice {
namespace parser {

class ProtocolRegistry {
public:
    static ProtocolRegistry& getInstance();
    
    bool registerProtocol(const ProtocolDefinition& protocol);
    bool unregisterProtocol(const std::string& name);
    bool hasProtocol(const std::string& name) const;
    const ProtocolDefinition* getProtocol(const std::string& name) const;
    std::vector<std::string> getRegisteredProtocols() const;
    size_t getProtocolCount() const;
    
    bool registerProtocolFactory(const std::string& name, 
                               std::function<ProtocolDefinition()> factory);
    ProtocolDefinition createProtocol(const std::string& name) const;
    
    void clear();
    void loadBuiltinProtocols();
    
    struct RegistryStats {
        size_t totalProtocols;
        size_t customProtocols;
        size_t builtinProtocols;
        std::chrono::microseconds lastUpdateTime;
        std::unordered_map<std::string, size_t> usageCount;
    };
    
    RegistryStats getStats() const;
    
private:
    ProtocolRegistry() = default;
    ~ProtocolRegistry() = default;
    ProtocolRegistry(const ProtocolRegistry&) = delete;
    ProtocolRegistry& operator=(const ProtocolRegistry&) = delete;
    
    mutable std::shared_mutex registryMutex_;
    std::unordered_map<std::string, ProtocolDefinition> protocols_;
    std::unordered_map<std::string, std::function<ProtocolDefinition()>> factories_;
    std::unordered_map<std::string, size_t> usageCount_;
    std::chrono::steady_clock::time_point lastUpdateTime_;
    
    void initializeBuiltinProtocols();
};

class BuiltinProtocols {
public:
    static ProtocolDefinition createEthernetProtocol();
    static ProtocolDefinition createIPv4Protocol();
    static ProtocolDefinition createIPv6Protocol();
    static ProtocolDefinition createTCPProtocol();
    static ProtocolDefinition createUDPProtocol();
    static ProtocolDefinition createICMPProtocol();
    static ProtocolDefinition createHTTPRequestProtocol();
    static ProtocolDefinition createHTTPResponseProtocol();
    static ProtocolDefinition createDNSProtocol();
    static ProtocolDefinition createARPProtocol();
    static ProtocolDefinition createVLANProtocol();
    static ProtocolDefinition createMPLSProtocol();
    
    static std::vector<ProtocolDefinition> getAllProtocols();
    
private:
    static void addCommonIPFields(ProtocolDefinition& protocol);
    static void addCommonTCPFields(ProtocolDefinition& protocol);
    static void addCommonUDPFields(ProtocolDefinition& protocol);
};

class ProtocolDetector {
public:
    struct DetectionResult {
        std::string protocolName;
        double confidence;
        std::string reason;
        std::chrono::microseconds detectionTime;
    };
    
    static DetectionResult detectProtocol(const std::vector<uint8_t>& packet);
    static std::vector<DetectionResult> detectMultipleProtocols(const std::vector<uint8_t>& packet);
    
    static bool isEthernet(const std::vector<uint8_t>& packet);
    static bool isIPv4(const std::vector<uint8_t>& packet);
    static bool isIPv6(const std::vector<uint8_t>& packet);
    static bool isTCP(const std::vector<uint8_t>& packet);
    static bool isUDP(const std::vector<uint8_t>& packet);
    static bool isICMP(const std::vector<uint8_t>& packet);
    static bool isHTTP(const std::vector<uint8_t>& packet);
    static bool isDNS(const std::vector<uint8_t>& packet);
    static bool isARP(const std::vector<uint8_t>& packet);
    
private:
    static double calculateConfidence(const std::vector<uint8_t>& packet, const ProtocolDefinition& protocol);
    static bool validateChecksum(const std::vector<uint8_t>& packet, const ProtocolDefinition& protocol);
};

} // namespace parser
} // namespace beatrice

#endif // BEATRICE_PARSER_PROTOCOL_REGISTRY_HPP 