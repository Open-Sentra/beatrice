#ifndef BEATRICE_PARSER_PROTOCOL_PARSER_HPP
#define BEATRICE_PARSER_PROTOCOL_PARSER_HPP

#include "FieldDefinition.hpp"
#include "ParserResult.hpp"
#include "ProtocolRegistry.hpp"
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <atomic>

namespace beatrice {
namespace parser {

class ProtocolParser {
public:
    struct ParserConfig {
        bool enableValidation;
        bool enableChecksumValidation;
        bool enableFieldConstraints;
        bool enableCustomValidators;
        bool enablePerformanceMetrics;
        bool enableFieldCaching;
        size_t maxFieldCacheSize;
        size_t maxValidationErrors;
        std::chrono::microseconds maxParseTime;
        std::function<void(const std::string&)> errorCallback;
        std::function<void(const std::string&)> warningCallback;
        std::function<void(const std::string&)> infoCallback;
        
        ParserConfig() : enableValidation(true), enableChecksumValidation(true), 
                        enableFieldConstraints(true), enableCustomValidators(true),
                        enablePerformanceMetrics(true), enableFieldCaching(true),
                        maxFieldCacheSize(1000), maxValidationErrors(100),
                        maxParseTime(std::chrono::microseconds(1000)) {}
    };
    
    struct ParserStats {
        uint64_t totalPacketsParsed = 0;
        uint64_t successfulParses = 0;
        uint64_t failedParses = 0;
        uint64_t validationErrors = 0;
        uint64_t checksumErrors = 0;
        std::chrono::microseconds totalParseTime{0};
        std::chrono::microseconds totalValidationTime{0};
        std::chrono::microseconds averageParseTime{0};
        std::chrono::microseconds averageValidationTime{0};
        std::chrono::microseconds minParseTime{std::chrono::microseconds::max()};
        std::chrono::microseconds maxParseTime{0};
        std::unordered_map<std::string, uint64_t> protocolUsageCount;
        std::unordered_map<std::string, uint64_t> fieldUsageCount;
    };
    
    static std::unique_ptr<ProtocolParser> create(const ProtocolParser::ParserConfig& config = ProtocolParser::ParserConfig{});
    static std::unique_ptr<ProtocolParser> createWithProtocols(const std::vector<std::string>& protocolNames, 
                                                             const ProtocolParser::ParserConfig& config = ProtocolParser::ParserConfig{});
    
    ProtocolParser(const ProtocolParser::ParserConfig& config = ProtocolParser::ParserConfig{});
    ~ProtocolParser();
    
    bool registerProtocol(const ProtocolDefinition& protocol);
    bool unregisterProtocol(const std::string& name);
    bool hasProtocol(const std::string& name) const;
    
    ParseResult parsePacket(const std::vector<uint8_t>& packet, const std::string& protocolName = "");
    ParseResult parsePacket(const std::vector<uint8_t>& packet, const ProtocolDefinition& protocol);
    std::vector<ParseResult> parsePacketMultipleProtocols(const std::vector<uint8_t>& packet);
    
    bool validatePacket(const std::vector<uint8_t>& packet, const std::string& protocolName);
    bool validatePacket(const std::vector<uint8_t>& packet, const ProtocolDefinition& protocol);
    
    std::string formatPacket(const ParseResult& result, const std::string& format = "json");
    std::vector<uint8_t> serializePacket(const ParseResult& result);
    
    void setConfig(const ProtocolParser::ParserConfig& config);
    const ProtocolParser::ParserConfig& getConfig() const;
    
    ParserStats getStats() const;
    void resetStats();
    void clearCache();
    
    std::vector<std::string> getSupportedFormats() const;
    std::vector<std::string> getSupportedProtocols() const;
    
    bool addCustomValidator(const std::string& protocolName, 
                           std::function<bool(const std::vector<uint8_t>&, const ParseResult&)> validator);
    bool addCustomFormatter(const std::string& protocolName, 
                           std::function<std::string(const ParseResult&)> formatter);
    
    void enableProfiling(bool enable);
    bool isProfilingEnabled() const;
    
private:
    ParserConfig config_;
    ParserStats stats_;
    std::atomic<bool> profilingEnabled_{false};
    
    std::unordered_map<std::string, ProtocolDefinition> protocols_;
    std::unordered_map<std::string, std::function<bool(const std::vector<uint8_t>&, const ParseResult&)>> customValidators_;
    std::unordered_map<std::string, std::function<std::string(const ParseResult&)>> customFormatters_;
    std::unordered_map<std::string, std::vector<FieldValue>> fieldCache_;
    
    mutable std::shared_mutex parserMutex_;
    mutable std::mutex statsMutex_;
    mutable std::mutex cacheMutex_;
    
    ParseResult parsePacketInternal(const std::vector<uint8_t>& packet, const ProtocolDefinition& protocol);
    FieldValue extractField(const std::vector<uint8_t>& packet, const FieldDefinition& field);
    
    template<typename T>
    T extractValue(const std::vector<uint8_t>& packet, size_t offset, size_t length, Endianness endian);
    
    bool validateField(const FieldValue& value, const FieldDefinition& field);
    bool validateChecksum(const std::vector<uint8_t>& packet, const ProtocolDefinition& protocol);
    
    void updateStats(const ParseResult& result, std::chrono::microseconds parseTime, 
                    std::chrono::microseconds validationTime);
    void logError(const std::string& message);
    void logWarning(const std::string& message);
    void logInfo(const std::string& message);
    
    std::string formatAsJson(const ParseResult& result) const;
    std::string formatAsXml(const ParseResult& result) const;
    std::string formatAsCsv(const ParseResult& result) const;
    std::string formatAsHumanReadable(const ParseResult& result) const;
    
    void cacheFieldValue(const std::string& key, const std::vector<FieldValue>& values);
    std::optional<std::vector<FieldValue>> getCachedFieldValues(const std::string& key);
    void cleanupCache();
};

class ParserBuilder {
public:
    ParserBuilder();
    
    ParserBuilder& withValidation(bool enable);
    ParserBuilder& withChecksumValidation(bool enable);
    ParserBuilder& withFieldConstraints(bool enable);
    ParserBuilder& withCustomValidators(bool enable);
    ParserBuilder& withPerformanceMetrics(bool enable);
    ParserBuilder& withFieldCaching(bool enable);
    ParserBuilder& withMaxFieldCacheSize(size_t size);
    ParserBuilder& withMaxValidationErrors(size_t max);
    ParserBuilder& withMaxParseTime(std::chrono::microseconds time);
    ParserBuilder& withErrorCallback(std::function<void(const std::string&)> callback);
    ParserBuilder& withWarningCallback(std::function<void(const std::string&)> callback);
    ParserBuilder& withInfoCallback(std::function<void(const std::string&)> callback);
    ParserBuilder& withProtocol(const ProtocolDefinition& protocol);
    ParserBuilder& withProtocols(const std::vector<ProtocolDefinition>& protocols);
    ParserBuilder& withBuiltinProtocols();
    
    std::unique_ptr<ProtocolParser> build();
    
private:
    ProtocolParser::ParserConfig config_;
    std::vector<ProtocolDefinition> protocols_;
};

} // namespace parser
} // namespace beatrice

#endif // BEATRICE_PARSER_PROTOCOL_PARSER_HPP 