#ifndef BEATRICE_PARSER_PARSER_RESULT_HPP
#define BEATRICE_PARSER_PARSER_RESULT_HPP

#include "FieldDefinition.hpp"
#include <variant>
#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include "parser/FieldDefinition.hpp"

namespace beatrice {
namespace parser {

enum class ParseStatus : uint8_t {
    SUCCESS = 0,
    INVALID_PROTOCOL = 1,
    INVALID_LENGTH = 2,
    FIELD_NOT_FOUND = 3,
    VALIDATION_FAILED = 4,
    ENDIANNESS_ERROR = 5,
    CHECKSUM_ERROR = 6,
    CUSTOM_ERROR = 7,
    PROTOCOL_NOT_FOUND = 8,
    PACKET_TOO_SHORT = 9
};

enum class FieldValueType : uint8_t {
    UINT8 = 0,
    UINT16 = 1,
    UINT32 = 2,
    UINT64 = 3,
    INT8 = 4,
    INT16 = 5,
    INT32 = 6,
    INT64 = 7,
    FLOAT32 = 8,
    FLOAT64 = 9,
    BYTES = 10,
    STRING = 11,
    BOOLEAN = 12,
    MAC_ADDRESS = 13,
    IPV4_ADDRESS = 14,
    IPV6_ADDRESS = 15,
    TIMESTAMP = 16,
    CUSTOM = 17
};

struct FieldValue {
    std::variant<uint8_t, uint16_t, uint32_t, uint64_t, 
                 int8_t, int16_t, int32_t, int64_t,
                 float, double, std::vector<uint8_t>, std::string, bool> value;
    FieldValueType type;
    std::string rawHex;
    std::string formatted;
    bool valid;
    std::string errorMessage;
    std::chrono::microseconds parseTime;
    
    FieldValue() : type(FieldValueType::UINT8), valid(false), parseTime(0) {}
    
    template<typename T>
    FieldValue(T val, FieldValueType t) : value(val), type(t), valid(true), parseTime(0) {}
    
    template<typename T>
    T get() const {
        return std::get<T>(value);
    }
    
    template<typename T>
    T getOrDefault(T defaultValue) const {
        if (valid && std::holds_alternative<T>(value)) {
            return std::get<T>(value);
        }
        return defaultValue;
    }
    
    std::string toString() const;
    std::string toHexString() const;
    std::string toJsonString() const;
};

struct ValidationResult {
    bool valid;
    std::string fieldName;
    std::string errorMessage;
    std::string expectedValue;
    std::string actualValue;
    ValidationRule rule;
    std::chrono::microseconds validationTime;
    
    ValidationResult() : valid(true), rule(ValidationRule::NONE), validationTime(0) {}
    
    ValidationResult(bool v, const std::string& field, const std::string& error = "")
        : valid(v), fieldName(field), errorMessage(error), rule(ValidationRule::NONE), validationTime(0) {}
};

struct ParseResult {
    ParseStatus status;
    std::string protocolName;
    std::string protocolVersion;
    std::unordered_map<std::string, FieldValue> fields;
    std::vector<ValidationResult> validationResults;
    std::string errorMessage;
    std::chrono::microseconds totalParseTime;
    std::chrono::microseconds totalValidationTime;
    size_t packetLength;
    size_t parsedBytes;
    std::vector<uint8_t> rawData;
    
    ParseResult() : status(ParseStatus::SUCCESS), totalParseTime(0), totalValidationTime(0), 
                   packetLength(0), parsedBytes(0) {}
    
    bool isSuccess() const { return status == ParseStatus::SUCCESS; }
    bool hasField(const std::string& name) const { return fields.find(name) != fields.end(); }
    size_t getFieldCount() const { return fields.size(); }
    size_t getValidationErrorCount() const;
    
    template<typename T>
    T getField(const std::string& name) const {
        auto it = fields.find(name);
        if (it != fields.end() && it->second.valid) {
            return it->second.get<T>();
        }
        throw std::runtime_error("Field not found or invalid: " + name);
    }
    
    template<typename T>
    T getFieldOrDefault(const std::string& name, T defaultValue) const {
        auto it = fields.find(name);
        if (it != fields.end() && it->second.valid) {
            return it->second.getOrDefault<T>(defaultValue);
        }
        return defaultValue;
    }
    
    std::string getFieldString(const std::string& name) const;
    std::vector<uint8_t> getFieldBytes(const std::string& name) const;
    uint64_t getFieldUInt(const std::string& name) const;
    int64_t getFieldInt(const std::string& name) const;
    double getFieldFloat(const std::string& name) const;
    bool getFieldBool(const std::string& name) const;
    
    std::string toJsonString() const;
    std::string toXmlString() const;
    std::string toCsvString() const;
    std::string toHumanReadableString() const;
    
    void addField(const std::string& name, const FieldValue& value);
    void addValidationResult(const ValidationResult& result);
    void setError(ParseStatus status, const std::string& message);
    void clear();
};

class ParseResultBuilder {
public:
    ParseResultBuilder();
    
    ParseResultBuilder& setProtocol(const std::string& name, const std::string& version = "1.0");
    ParseResultBuilder& addField(const std::string& name, const FieldValue& value);
    ParseResultBuilder& addValidationResult(const ValidationResult& result);
    ParseResultBuilder& setError(ParseStatus status, const std::string& message);
    ParseResultBuilder& setPacketInfo(size_t length, size_t parsed);
    ParseResultBuilder& setRawData(const std::vector<uint8_t>& data);
    ParseResultBuilder& setTiming(std::chrono::microseconds parseTime, std::chrono::microseconds validationTime);
    
    ParseResult build() const;
    void reset();
    
private:
    ParseResult result_;
};

} // namespace parser
} // namespace beatrice

#endif // BEATRICE_PARSER_PARSER_RESULT_HPP 