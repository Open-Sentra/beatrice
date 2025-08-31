#ifndef BEATRICE_PARSER_FIELD_DEFINITION_HPP
#define BEATRICE_PARSER_FIELD_DEFINITION_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <variant>
#include <optional>
#include <unordered_map>

namespace beatrice {
namespace parser {

enum class FieldType : uint8_t {
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

enum class Endianness : uint8_t {
    LITTLE = 0,
    BIG = 1,
    NETWORK = 2,
    HOST = 3
};

enum class ValidationRule : uint8_t {
    NONE = 0,
    RANGE = 1,
    ENUM = 2,
    PATTERN = 3,
    CHECKSUM = 4,
    CUSTOM = 5
};

struct FieldConstraint {
    std::variant<int64_t, uint64_t, double, std::string> minValue;
    std::variant<int64_t, uint64_t, double, std::string> maxValue;
    std::vector<std::variant<int64_t, uint64_t, double, std::string>> allowedValues;
    std::string pattern;
    std::function<bool(const std::vector<uint8_t>&)> customValidator;
};

struct FieldDefinition {
    std::string name;
    size_t offset;
    size_t length;
    FieldType type;
    Endianness endianness;
    bool required;
    std::string description;
    std::optional<FieldConstraint> constraint;
    std::function<std::string(const std::vector<uint8_t>&)> formatter;
    std::function<std::vector<uint8_t>(const std::string&)> parser;
    
    FieldDefinition() = default;
    
    FieldDefinition(const std::string& n, size_t off, size_t len, FieldType t, 
                   Endianness e = Endianness::NETWORK, bool req = true)
        : name(n), offset(off), length(len), type(t), endianness(e), required(req) {}
    
    FieldDefinition(const std::string& n, size_t off, size_t len, FieldType t, 
                   const std::string& desc, Endianness e = Endianness::NETWORK, bool req = true)
        : name(n), offset(off), length(len), type(t), endianness(e), required(req), description(desc) {}
};

struct ProtocolDefinition {
    std::string name;
    std::string version;
    std::string description;
    std::vector<FieldDefinition> fields;
    std::unordered_map<std::string, size_t> fieldIndexMap;
    std::function<bool(const std::vector<uint8_t>&)> validator;
    std::function<std::string(const std::vector<uint8_t>&)> formatter;
    
    ProtocolDefinition() = default;
    
    ProtocolDefinition(const std::string& n, const std::string& v = "1.0")
        : name(n), version(v) {}
    
    void addField(const FieldDefinition& field);
    const FieldDefinition* getField(const std::string& name) const;
    bool hasField(const std::string& name) const;
    size_t getFieldCount() const;
    size_t getTotalLength() const;
};

class FieldFactory {
public:
    static FieldDefinition createUInt8Field(const std::string& name, size_t offset, 
                                          bool required = true, const std::string& desc = "");
    static FieldDefinition createUInt16Field(const std::string& name, size_t offset, 
                                           Endianness endian = Endianness::NETWORK, 
                                           bool required = true, const std::string& desc = "");
    static FieldDefinition createUInt32Field(const std::string& name, size_t offset, 
                                           Endianness endian = Endianness::NETWORK, 
                                           bool required = true, const std::string& desc = "");
    static FieldDefinition createUInt64Field(const std::string& name, size_t offset, 
                                           Endianness endian = Endianness::NETWORK, 
                                           bool required = true, const std::string& desc = "");
    static FieldDefinition createInt8Field(const std::string& name, size_t offset, 
                                         bool required = true, const std::string& desc = "");
    static FieldDefinition createInt16Field(const std::string& name, size_t offset, 
                                          Endianness endian = Endianness::NETWORK, 
                                          bool required = true, const std::string& desc = "");
    static FieldDefinition createInt32Field(const std::string& name, size_t offset, 
                                          Endianness endian = Endianness::NETWORK, 
                                          bool required = true, const std::string& desc = "");
    static FieldDefinition createInt64Field(const std::string& name, size_t offset, 
                                          Endianness endian = Endianness::NETWORK, 
                                          bool required = true, const std::string& desc = "");
    static FieldDefinition createFloat32Field(const std::string& name, size_t offset, 
                                            Endianness endian = Endianness::NETWORK, 
                                            bool required = true, const std::string& desc = "");
    static FieldDefinition createFloat64Field(const std::string& name, size_t offset, 
                                            Endianness endian = Endianness::NETWORK, 
                                            bool required = true, const std::string& desc = "");
    static FieldDefinition createBytesField(const std::string& name, size_t offset, 
                                          size_t length, bool required = true, const std::string& desc = "");
    static FieldDefinition createStringField(const std::string& name, size_t offset, 
                                           size_t length, bool required = true, const std::string& desc = "");
    static FieldDefinition createBooleanField(const std::string& name, size_t offset, 
                                            bool required = true, const std::string& desc = "");
    static FieldDefinition createMacAddressField(const std::string& name, size_t offset, 
                                               bool required = true, const std::string& desc = "");
    static FieldDefinition createIPv4AddressField(const std::string& name, size_t offset, 
                                                bool required = true, const std::string& desc = "");
    static FieldDefinition createIPv6AddressField(const std::string& name, size_t offset, 
                                                bool required = true, const std::string& desc = "");
    static FieldDefinition createTimestampField(const std::string& name, size_t offset, 
                                              size_t length, Endianness endian = Endianness::NETWORK, 
                                              bool required = true, const std::string& desc = "");
    static FieldDefinition createCustomField(const std::string& name, size_t offset, 
                                           size_t length, 
                                           std::function<std::string(const std::vector<uint8_t>&)> formatter,
                                           std::function<std::vector<uint8_t>(const std::string&)> parser,
                                           bool required = true, const std::string& desc = "");
};

} // namespace parser
} // namespace beatrice

#endif // BEATRICE_PARSER_FIELD_DEFINITION_HPP 