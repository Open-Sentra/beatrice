#include "parser/ProtocolParser.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace beatrice {
namespace parser {

std::unique_ptr<ProtocolParser> ProtocolParser::create(const ProtocolParser::ParserConfig& config) {
    return std::make_unique<ProtocolParser>(config);
}

std::unique_ptr<ProtocolParser> ProtocolParser::createWithProtocols(const std::vector<std::string>& protocolNames, 
                                                                  const ProtocolParser::ParserConfig& config) {
    auto parser = std::make_unique<ProtocolParser>(config);
    
    auto& registry = ProtocolRegistry::getInstance();
    for (const auto& name : protocolNames) {
        if (registry.hasProtocol(name)) {
            auto protocol = registry.getProtocol(name);
            if (protocol) {
                parser->registerProtocol(*protocol);
            }
        }
    }
    
    return parser;
}

ProtocolParser::ProtocolParser(const ProtocolParser::ParserConfig& config) : config_(config) {
    if (config_.enablePerformanceMetrics) {
        profilingEnabled_ = true;
    }
}

ProtocolParser::~ProtocolParser() {
    clearCache();
}

bool ProtocolParser::registerProtocol(const ProtocolDefinition& protocol) {
    std::unique_lock<std::shared_mutex> lock(parserMutex_);
    
    if (protocols_.find(protocol.name) != protocols_.end()) {
        return false;
    }
    
    protocols_[protocol.name] = protocol;
    return true;
}

bool ProtocolParser::unregisterProtocol(const std::string& name) {
    std::unique_lock<std::shared_mutex> lock(parserMutex_);
    
    auto it = protocols_.find(name);
    if (it == protocols_.end()) {
        return false;
    }
    
    protocols_.erase(it);
    return true;
}

bool ProtocolParser::hasProtocol(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(parserMutex_);
    return protocols_.find(name) != protocols_.end();
}

ParseResult ProtocolParser::parsePacket(const std::vector<uint8_t>& packet, const std::string& protocolName) {
    if (protocolName.empty()) {
        return parsePacketMultipleProtocols(packet)[0];
    }
    
    std::shared_lock<std::shared_mutex> lock(parserMutex_);
    
    auto it = protocols_.find(protocolName);
    if (it == protocols_.end()) {
        ParseResultBuilder builder;
        builder.setError(ParseStatus::PROTOCOL_NOT_FOUND, "Protocol not found: " + protocolName);
        return builder.build();
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    auto result = parsePacketInternal(packet, it->second);
    auto endTime = std::chrono::high_resolution_clock::now();
    
    auto parseTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    if (config_.enablePerformanceMetrics) {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        updateStats(result, parseTime, std::chrono::microseconds(0));
    }
    
    return result;
}

ParseResult ProtocolParser::parsePacket(const std::vector<uint8_t>& packet, const ProtocolDefinition& protocol) {
    auto startTime = std::chrono::high_resolution_clock::now();
    auto result = parsePacketInternal(packet, protocol);
    auto endTime = std::chrono::high_resolution_clock::now();
    
    auto parseTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    if (config_.enablePerformanceMetrics) {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        updateStats(result, parseTime, std::chrono::microseconds(0));
    }
    
    return result;
}

std::vector<ParseResult> ProtocolParser::parsePacketMultipleProtocols(const std::vector<uint8_t>& packet) {
    std::vector<ParseResult> results;
    
    std::shared_lock<std::shared_mutex> lock(parserMutex_);
    
    for (const auto& [name, protocol] : protocols_) {
        auto startTime = std::chrono::high_resolution_clock::now();
        auto result = parsePacketInternal(packet, protocol);
        auto endTime = std::chrono::high_resolution_clock::now();
        
        auto parseTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        if (config_.enablePerformanceMetrics) {
            std::lock_guard<std::mutex> statsLock(statsMutex_);
            updateStats(result, parseTime, std::chrono::microseconds(0));
        }
        
        results.push_back(result);
    }
    
    return results;
}

bool ProtocolParser::validatePacket(const std::vector<uint8_t>& packet, const std::string& protocolName) {
    auto result = parsePacket(packet, protocolName);
    return result.isSuccess();
}

bool ProtocolParser::validatePacket(const std::vector<uint8_t>& packet, const ProtocolDefinition& protocol) {
    auto result = parsePacket(packet, protocol);
    return result.isSuccess();
}

std::string ProtocolParser::formatPacket(const ParseResult& result, const std::string& format) {
    if (format == "json") {
        return formatAsJson(result);
    } else if (format == "xml") {
        return formatAsXml(result);
    } else if (format == "csv") {
        return formatAsCsv(result);
    } else if (format == "human") {
        return formatAsHumanReadable(result);
    }
    
    return formatAsJson(result);
}

std::vector<uint8_t> ProtocolParser::serializePacket(const ParseResult& result) {
    return result.rawData;
}

void ProtocolParser::setConfig(const ProtocolParser::ParserConfig& config) {
    config_ = config;
    if (config_.enablePerformanceMetrics) {
        profilingEnabled_ = true;
    }
}

const ProtocolParser::ParserConfig& ProtocolParser::getConfig() const {
    return config_;
}

ProtocolParser::ParserStats ProtocolParser::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void ProtocolParser::resetStats() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_ = ParserStats{};
}

void ProtocolParser::clearCache() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    fieldCache_.clear();
}

std::vector<std::string> ProtocolParser::getSupportedFormats() const {
    return {"json", "xml", "csv", "human"};
}

std::vector<std::string> ProtocolParser::getSupportedProtocols() const {
    std::shared_lock<std::shared_mutex> lock(parserMutex_);
    
    std::vector<std::string> names;
    names.reserve(protocols_.size());
    
    for (const auto& [name, _] : protocols_) {
        names.push_back(name);
    }
    
    return names;
}

bool ProtocolParser::addCustomValidator(const std::string& protocolName, 
                                       std::function<bool(const std::vector<uint8_t>&, const ParseResult&)> validator) {
    std::unique_lock<std::shared_mutex> lock(parserMutex_);
    
    if (protocols_.find(protocolName) == protocols_.end()) {
        return false;
    }
    
    customValidators_[protocolName] = validator;
    return true;
}

bool ProtocolParser::addCustomFormatter(const std::string& protocolName, 
                                       std::function<std::string(const ParseResult&)> formatter) {
    std::unique_lock<std::shared_mutex> lock(parserMutex_);
    
    if (protocols_.find(protocolName) == protocols_.end()) {
        return false;
    }
    
    customFormatters_[protocolName] = formatter;
    return true;
}

void ProtocolParser::enableProfiling(bool enable) {
    profilingEnabled_ = enable;
}

bool ProtocolParser::isProfilingEnabled() const {
    return profilingEnabled_;
}

ParseResult ProtocolParser::parsePacketInternal(const std::vector<uint8_t>& packet, const ProtocolDefinition& protocol) {
    ParseResultBuilder builder;
    builder.setProtocol(protocol.name, protocol.version);
    builder.setPacketInfo(packet.size(), 0);
    builder.setRawData(packet);
    
    if (packet.size() < protocol.getTotalLength()) {
        builder.setError(ParseStatus::PACKET_TOO_SHORT, "Packet too short for protocol");
        return builder.build();
    }
    
    size_t parsedBytes = 0;
    
    for (const auto& field : protocol.fields) {
        if (field.offset + field.length > packet.size()) {
            continue;
        }
        
        auto startTime = std::chrono::high_resolution_clock::now();
        auto fieldValue = extractField(packet, field);
        auto endTime = std::chrono::high_resolution_clock::now();
        
        auto parseTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        fieldValue.parseTime = parseTime;
        
        builder.addField(field.name, fieldValue);
        
        if (config_.enableValidation && !validateField(fieldValue, field)) {
            ValidationResult validationResult;
            validationResult.fieldName = field.name;
            validationResult.valid = false;
            validationResult.errorMessage = "Field validation failed";
            validationResult.validationTime = parseTime;
            builder.addValidationResult(validationResult);
        }
        
        parsedBytes = std::max(parsedBytes, field.offset + field.length);
    }
    
    builder.setPacketInfo(packet.size(), parsedBytes);
    
    if (config_.enableChecksumValidation && !validateChecksum(packet, protocol)) {
        builder.setError(ParseStatus::CHECKSUM_ERROR, "Checksum validation failed");
    }
    
    return builder.build();
}

FieldValue ProtocolParser::extractField(const std::vector<uint8_t>& packet, const FieldDefinition& field) {
    FieldValue value;
    value.valid = false;
    
    if (field.offset + field.length > packet.size()) {
        return value;
    }
    
    std::vector<uint8_t> fieldData(packet.begin() + field.offset, 
                                   packet.begin() + field.offset + field.length);
    
    value.rawHex = bytesToHex(fieldData);
    value.valid = true;
    
    switch (field.type) {
        case FieldType::UINT8:
            value.type = FieldValueType::UINT8;
            value.value = extractValue<uint8_t>(packet, field.offset, field.length, field.endianness);
            break;
        case FieldType::UINT16:
            value.type = FieldValueType::UINT16;
            value.value = extractValue<uint16_t>(packet, field.offset, field.length, field.endianness);
            break;
        case FieldType::UINT32:
            value.type = FieldValueType::UINT32;
            value.value = extractValue<uint32_t>(packet, field.offset, field.length, field.endianness);
            break;
        case FieldType::UINT64:
            value.type = FieldValueType::UINT64;
            value.value = extractValue<uint64_t>(packet, field.offset, field.length, field.endianness);
            break;
        case FieldType::INT8:
            value.type = FieldValueType::INT8;
            value.value = extractValue<int8_t>(packet, field.offset, field.length, field.endianness);
            break;
        case FieldType::INT16:
            value.type = FieldValueType::INT16;
            value.value = extractValue<int16_t>(packet, field.offset, field.length, field.endianness);
            break;
        case FieldType::INT32:
            value.type = FieldValueType::INT32;
            value.value = extractValue<int32_t>(packet, field.offset, field.length, field.endianness);
            break;
        case FieldType::INT64:
            value.type = FieldValueType::INT64;
            value.value = extractValue<int64_t>(packet, field.offset, field.length, field.endianness);
            break;
        case FieldType::FLOAT32:
            value.type = FieldValueType::FLOAT32;
            value.value = extractValue<float>(packet, field.offset, field.length, field.endianness);
            break;
        case FieldType::FLOAT64:
            value.type = FieldValueType::FLOAT64;
            value.value = extractValue<double>(packet, field.offset, field.length, field.endianness);
            break;
        case FieldType::BYTES:
            value.type = FieldValueType::BYTES;
            value.value = fieldData;
            break;
        case FieldType::STRING:
            value.type = FieldValueType::STRING;
            value.value = std::string(fieldData.begin(), fieldData.end());
            break;
        case FieldType::BOOLEAN:
            value.type = FieldValueType::BOOLEAN;
            value.value = (fieldData[0] != 0);
            break;
        case FieldType::MAC_ADDRESS:
            value.type = FieldValueType::MAC_ADDRESS;
            value.value = fieldData;
            value.formatted = formatMacAddress(fieldData);
            break;
        case FieldType::IPV4_ADDRESS:
            value.type = FieldValueType::IPV4_ADDRESS;
            value.value = fieldData;
            value.formatted = formatIPv4Address(fieldData);
            break;
        case FieldType::IPV6_ADDRESS:
            value.type = FieldValueType::IPV6_ADDRESS;
            value.value = fieldData;
            value.formatted = formatIPv6Address(fieldData);
            break;
        case FieldType::TIMESTAMP:
            value.type = FieldValueType::TIMESTAMP;
            value.value = extractValue<uint64_t>(packet, field.offset, field.length, field.endianness);
            value.formatted = formatTimestamp(std::get<uint64_t>(value.value));
            break;
        case FieldType::CUSTOM:
            value.type = FieldValueType::CUSTOM;
            value.value = fieldData;
            if (field.formatter) {
                value.formatted = field.formatter(fieldData);
            }
            break;
    }
    
    return value;
}

template<typename T>
T ProtocolParser::extractValue(const std::vector<uint8_t>& packet, size_t offset, size_t length, Endianness endian) {
    if (offset + length > packet.size()) {
        return T{};
    }
    
    if constexpr (std::is_floating_point_v<T>) {
        // For floating point, we need to handle differently
        if (length == 4 && std::is_same_v<T, float>) {
            uint32_t raw = 0;
            if (endian == Endianness::LITTLE) {
                raw = (packet[offset] << 0) | (packet[offset + 1] << 8) | 
                      (packet[offset + 2] << 16) | (packet[offset + 3] << 24);
            } else {
                raw = (packet[offset] << 24) | (packet[offset + 1] << 16) | 
                      (packet[offset + 2] << 8) | (packet[offset + 3] << 0);
            }
            return *reinterpret_cast<float*>(&raw);
        } else if (length == 8 && std::is_same_v<T, double>) {
            uint64_t raw = 0;
            if (endian == Endianness::LITTLE) {
                for (size_t i = 0; i < 8; ++i) {
                    raw |= static_cast<uint64_t>(packet[offset + i]) << (i * 8);
                }
            } else {
                for (size_t i = 0; i < 8; ++i) {
                    raw |= static_cast<uint64_t>(packet[offset + 7 - i]) << (i * 8);
                }
            }
            return *reinterpret_cast<double*>(&raw);
        }
        return T{};
    } else {
        // For integer types
        T value = 0;
        
        if (endian == Endianness::LITTLE) {
            for (size_t i = 0; i < length; ++i) {
                value |= static_cast<T>(packet[offset + i]) << (i * 8);
            }
        } else {
            for (size_t i = 0; i < length; ++i) {
                value |= static_cast<T>(packet[offset + length - 1 - i]) << (i * 8);
            }
        }
        
        return value;
    }
}

bool ProtocolParser::validateField(const FieldValue& value, const FieldDefinition& field) {
    if (!value.valid) {
        return false;
    }
    
    if (field.constraint) {
        const auto& constraint = field.constraint.value();
        
        // Basic constraint validation
        if (constraint.minValue.index() != std::variant_npos && constraint.maxValue.index() != std::variant_npos) {
            if (value.type == FieldValueType::UINT8 || value.type == FieldValueType::UINT16 ||
                value.type == FieldValueType::UINT32 || value.type == FieldValueType::UINT64) {
                uint64_t numValue = std::visit([](const auto& v) -> uint64_t {
                    if constexpr (std::is_arithmetic_v<std::decay_t<decltype(v)>>) {
                        return static_cast<uint64_t>(v);
                    }
                    return 0;
                }, value.value);
                
                // Simple range check
                if (constraint.minValue.index() == 1) { // uint64_t
                    uint64_t minVal = std::get<uint64_t>(constraint.minValue);
                    if (numValue < minVal) return false;
                }
                if (constraint.maxValue.index() == 1) { // uint64_t
                    uint64_t maxVal = std::get<uint64_t>(constraint.maxValue);
                    if (numValue > maxVal) return false;
                }
            }
        }
        
        // Pattern validation
        if (!constraint.pattern.empty()) {
            if (value.toString().find(constraint.pattern) == std::string::npos) {
                return false;
            }
        }
    }
    
    return true;
}

bool ProtocolParser::validateChecksum(const std::vector<uint8_t>& packet, const ProtocolDefinition& protocol) {
    // Basic checksum validation - can be extended
    return true;
}

void ProtocolParser::updateStats(const ParseResult& result, std::chrono::microseconds parseTime, 
                                std::chrono::microseconds validationTime) {
    stats_.totalPacketsParsed++;
    
    if (result.isSuccess()) {
        stats_.successfulParses++;
    } else {
        stats_.failedParses++;
    }
    
    stats_.totalParseTime += parseTime;
    stats_.totalValidationTime += validationTime;
    
    if (parseTime < stats_.minParseTime) {
        stats_.minParseTime = parseTime;
    }
    if (parseTime > stats_.maxParseTime) {
        stats_.maxParseTime = parseTime;
    }
    
    stats_.averageParseTime = std::chrono::microseconds(stats_.totalParseTime.count() / stats_.successfulParses);
    stats_.averageValidationTime = std::chrono::microseconds(stats_.totalValidationTime.count() / stats_.successfulParses);
    
    stats_.protocolUsageCount[result.protocolName]++;
}

void ProtocolParser::logError(const std::string& message) {
    if (config_.errorCallback) {
        config_.errorCallback(message);
    }
}

void ProtocolParser::logWarning(const std::string& message) {
    if (config_.warningCallback) {
        config_.warningCallback(message);
    }
}

void ProtocolParser::logInfo(const std::string& message) {
    if (config_.infoCallback) {
        config_.infoCallback(message);
    }
}

std::string ProtocolParser::formatAsJson(const ParseResult& result) const {
    return result.toJsonString();
}

std::string ProtocolParser::formatAsXml(const ParseResult& result) const {
    return result.toXmlString();
}

std::string ProtocolParser::formatAsCsv(const ParseResult& result) const {
    return result.toCsvString();
}

std::string ProtocolParser::formatAsHumanReadable(const ParseResult& result) const {
    return result.toHumanReadableString();
}

void ProtocolParser::cacheFieldValue(const std::string& key, const std::vector<FieldValue>& values) {
    if (!config_.enableFieldCaching) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    if (fieldCache_.size() >= config_.maxFieldCacheSize) {
        cleanupCache();
    }
    
    fieldCache_[key] = values;
}

std::optional<std::vector<FieldValue>> ProtocolParser::getCachedFieldValues(const std::string& key) {
    if (!config_.enableFieldCaching) {
        return std::nullopt;
    }
    
    std::lock_guard<std::mutex> lock(cacheMutex_);
    
    auto it = fieldCache_.find(key);
    if (it != fieldCache_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

void ProtocolParser::cleanupCache() {
    if (fieldCache_.size() <= config_.maxFieldCacheSize / 2) {
        return;
    }
    
    std::vector<std::string> keysToRemove;
    keysToRemove.reserve(fieldCache_.size() / 2);
    
    for (const auto& [key, _] : fieldCache_) {
        keysToRemove.push_back(key);
        if (keysToRemove.size() >= fieldCache_.size() / 2) {
            break;
        }
    }
    
    for (const auto& key : keysToRemove) {
        fieldCache_.erase(key);
    }
}

std::string ProtocolParser::bytesToHex(const std::vector<uint8_t>& bytes) const {
    std::stringstream ss;
    for (uint8_t byte : bytes) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return ss.str();
}

std::string ProtocolParser::formatMacAddress(const std::vector<uint8_t>& bytes) const {
    if (bytes.size() != 6) return "invalid";
    
    std::stringstream ss;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i > 0) ss << ":";
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

std::string ProtocolParser::formatIPv4Address(const std::vector<uint8_t>& bytes) const {
    if (bytes.size() != 4) return "invalid";
    
    std::stringstream ss;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i > 0) ss << ".";
        ss << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

std::string ProtocolParser::formatIPv6Address(const std::vector<uint8_t>& bytes) const {
    if (bytes.size() != 16) return "invalid";
    
    std::stringstream ss;
    for (size_t i = 0; i < bytes.size(); i += 2) {
        if (i > 0) ss << ":";
        uint16_t value = (bytes[i] << 8) | bytes[i + 1];
        ss << std::hex << value;
    }
    return ss.str();
}

std::string ProtocolParser::formatTimestamp(uint64_t timestamp) const {
    auto timePoint = std::chrono::system_clock::from_time_t(timestamp);
    auto timeT = std::chrono::system_clock::to_time_t(timePoint);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

ParserBuilder::ParserBuilder() {
    config_ = ProtocolParser::ParserConfig{};
}

ParserBuilder& ParserBuilder::withValidation(bool enable) {
    config_.enableValidation = enable;
    return *this;
}

ParserBuilder& ParserBuilder::withChecksumValidation(bool enable) {
    config_.enableChecksumValidation = enable;
    return *this;
}

ParserBuilder& ParserBuilder::withFieldConstraints(bool enable) {
    config_.enableFieldConstraints = enable;
    return *this;
}

ParserBuilder& ParserBuilder::withCustomValidators(bool enable) {
    config_.enableCustomValidators = enable;
    return *this;
}

ParserBuilder& ParserBuilder::withPerformanceMetrics(bool enable) {
    config_.enablePerformanceMetrics = enable;
    return *this;
}

ParserBuilder& ParserBuilder::withFieldCaching(bool enable) {
    config_.enableFieldCaching = enable;
    return *this;
}

ParserBuilder& ParserBuilder::withMaxFieldCacheSize(size_t size) {
    config_.maxFieldCacheSize = size;
    return *this;
}

ParserBuilder& ParserBuilder::withMaxValidationErrors(size_t max) {
    config_.maxValidationErrors = max;
    return *this;
}

ParserBuilder& ParserBuilder::withMaxParseTime(std::chrono::microseconds time) {
    config_.maxParseTime = time;
    return *this;
}

ParserBuilder& ParserBuilder::withErrorCallback(std::function<void(const std::string&)> callback) {
    config_.errorCallback = callback;
    return *this;
}

ParserBuilder& ParserBuilder::withWarningCallback(std::function<void(const std::string&)> callback) {
    config_.warningCallback = callback;
    return *this;
}

ParserBuilder& ParserBuilder::withInfoCallback(std::function<void(const std::string&)> callback) {
    config_.infoCallback = callback;
    return *this;
}

ParserBuilder& ParserBuilder::withProtocol(const ProtocolDefinition& protocol) {
    protocols_.push_back(protocol);
    return *this;
}

ParserBuilder& ParserBuilder::withProtocols(const std::vector<ProtocolDefinition>& protocols) {
    protocols_.insert(protocols_.end(), protocols.begin(), protocols.end());
    return *this;
}

ParserBuilder& ParserBuilder::withBuiltinProtocols() {
    auto& registry = ProtocolRegistry::getInstance();
    auto builtins = registry.getRegisteredProtocols();
    
    for (const auto& name : builtins) {
        auto protocol = registry.getProtocol(name);
        if (protocol) {
            protocols_.push_back(*protocol);
        }
    }
    
    return *this;
}

std::unique_ptr<ProtocolParser> ParserBuilder::build() {
    auto parser = std::make_unique<ProtocolParser>(config_);
    
    for (const auto& protocol : protocols_) {
        parser->registerProtocol(protocol);
    }
    
    return parser;
}

} // namespace parser
} // namespace beatrice 