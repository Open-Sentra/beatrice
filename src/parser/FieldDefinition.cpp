#include "parser/FieldDefinition.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <arpa/inet.h>

namespace beatrice {
namespace parser {

void ProtocolDefinition::addField(const FieldDefinition& field) {
    fields.push_back(field);
    fieldIndexMap[field.name] = fields.size() - 1;
}

const FieldDefinition* ProtocolDefinition::getField(const std::string& name) const {
    auto it = fieldIndexMap.find(name);
    if (it != fieldIndexMap.end() && it->second < fields.size()) {
        return &fields[it->second];
    }
    return nullptr;
}

bool ProtocolDefinition::hasField(const std::string& name) const {
    return fieldIndexMap.find(name) != fieldIndexMap.end();
}

size_t ProtocolDefinition::getFieldCount() const {
    return fields.size();
}

size_t ProtocolDefinition::getTotalLength() const {
    if (fields.empty()) return 0;
    
    size_t maxOffset = 0;
    size_t maxLength = 0;
    
    for (const auto& field : fields) {
        size_t fieldEnd = field.offset + field.length;
        if (fieldEnd > maxOffset + maxLength) {
            maxOffset = field.offset;
            maxLength = field.length;
        }
    }
    
    return maxOffset + maxLength;
}

FieldDefinition FieldFactory::createUInt8Field(const std::string& name, size_t offset, 
                                             bool required, const std::string& desc) {
    FieldDefinition field(name, offset, 1, FieldType::UINT8, desc, Endianness::NETWORK, required);
    return field;
}

FieldDefinition FieldFactory::createUInt16Field(const std::string& name, size_t offset, 
                                              Endianness endian, bool required, const std::string& desc) {
    FieldDefinition field(name, offset, 2, FieldType::UINT16, desc, endian, required);
    return field;
}

FieldDefinition FieldFactory::createUInt32Field(const std::string& name, size_t offset, 
                                              Endianness endian, bool required, const std::string& desc) {
    FieldDefinition field(name, offset, 4, FieldType::UINT32, desc, endian, required);
    return field;
}

FieldDefinition FieldFactory::createUInt64Field(const std::string& name, size_t offset, 
                                              Endianness endian, bool required, const std::string& desc) {
    FieldDefinition field(name, offset, 8, FieldType::UINT64, desc, endian, required);
    return field;
}

FieldDefinition FieldFactory::createInt8Field(const std::string& name, size_t offset, 
                                            bool required, const std::string& desc) {
    FieldDefinition field(name, offset, 1, FieldType::INT8, desc, Endianness::NETWORK, required);
    return field;
}

FieldDefinition FieldFactory::createInt16Field(const std::string& name, size_t offset, 
                                             Endianness endian, bool required, const std::string& desc) {
    FieldDefinition field(name, offset, 2, FieldType::INT16, desc, endian, required);
    return field;
}

FieldDefinition FieldFactory::createInt32Field(const std::string& name, size_t offset, 
                                             Endianness endian, bool required, const std::string& desc) {
    FieldDefinition field(name, offset, 4, FieldType::INT32, desc, endian, required);
    return field;
}

FieldDefinition FieldFactory::createInt64Field(const std::string& name, size_t offset, 
                                             Endianness endian, bool required, const std::string& desc) {
    FieldDefinition field(name, offset, 8, FieldType::INT64, desc, endian, required);
    return field;
}

FieldDefinition FieldFactory::createFloat32Field(const std::string& name, size_t offset, 
                                               Endianness endian, bool required, const std::string& desc) {
    FieldDefinition field(name, offset, 4, FieldType::FLOAT32, desc, endian, required);
    return field;
}

FieldDefinition FieldFactory::createFloat64Field(const std::string& name, size_t offset, 
                                               Endianness endian, bool required, const std::string& desc) {
    FieldDefinition field(name, offset, 8, FieldType::FLOAT64, desc, endian, required);
    return field;
}

FieldDefinition FieldFactory::createBytesField(const std::string& name, size_t offset, 
                                             size_t length, bool required, const std::string& desc) {
    FieldDefinition field(name, offset, length, FieldType::BYTES, desc, Endianness::NETWORK, required);
    return field;
}

FieldDefinition FieldFactory::createStringField(const std::string& name, size_t offset, 
                                              size_t length, bool required, const std::string& desc) {
    FieldDefinition field(name, offset, length, FieldType::STRING, desc, Endianness::NETWORK, required);
    return field;
}

FieldDefinition FieldFactory::createBooleanField(const std::string& name, size_t offset, 
                                               bool required, const std::string& desc) {
    FieldDefinition field(name, offset, 1, FieldType::BOOLEAN, desc, Endianness::NETWORK, required);
    return field;
}

FieldDefinition FieldFactory::createMacAddressField(const std::string& name, size_t offset, 
                                                  bool required, const std::string& desc) {
    FieldDefinition field(name, offset, 6, FieldType::MAC_ADDRESS, desc, Endianness::NETWORK, required);
    return field;
}

FieldDefinition FieldFactory::createIPv4AddressField(const std::string& name, size_t offset, 
                                                   bool required, const std::string& desc) {
    FieldDefinition field(name, offset, 4, FieldType::IPV4_ADDRESS, desc, Endianness::NETWORK, required);
    return field;
}

FieldDefinition FieldFactory::createIPv6AddressField(const std::string& name, size_t offset, 
                                                   bool required, const std::string& desc) {
    FieldDefinition field(name, offset, 16, FieldType::IPV6_ADDRESS, desc, Endianness::NETWORK, required);
    return field;
}

FieldDefinition FieldFactory::createTimestampField(const std::string& name, size_t offset, 
                                                 size_t length, Endianness endian, bool required, const std::string& desc) {
    FieldDefinition field(name, offset, length, FieldType::TIMESTAMP, desc, endian, required);
    return field;
}

FieldDefinition FieldFactory::createCustomField(const std::string& name, size_t offset, 
                                              size_t length, 
                                              std::function<std::string(const std::vector<uint8_t>&)> formatter,
                                              std::function<std::vector<uint8_t>(const std::string&)> parser,
                                              bool required, const std::string& desc) {
    FieldDefinition field(name, offset, length, FieldType::CUSTOM, desc, Endianness::NETWORK, required);
    field.formatter = formatter;
    field.parser = parser;
    return field;
}

} // namespace parser
} // namespace beatrice 