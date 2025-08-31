#include "parser/ParserResult.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace beatrice {
namespace parser {

std::string FieldValue::toString() const {
    if (!valid) return "INVALID";
    
    switch (type) {
        case FieldValueType::UINT8:
            return std::to_string(std::get<uint8_t>(value));
        case FieldValueType::UINT16:
            return std::to_string(std::get<uint16_t>(value));
        case FieldValueType::UINT32:
            return std::to_string(std::get<uint32_t>(value));
        case FieldValueType::UINT64:
            return std::to_string(std::get<uint64_t>(value));
        case FieldValueType::INT8:
            return std::to_string(std::get<int8_t>(value));
        case FieldValueType::INT16:
            return std::to_string(std::get<int16_t>(value));
        case FieldValueType::INT32:
            return std::to_string(std::get<int32_t>(value));
        case FieldValueType::INT64:
            return std::to_string(std::get<int64_t>(value));
        case FieldValueType::FLOAT32:
            return std::to_string(std::get<float>(value));
        case FieldValueType::FLOAT64:
            return std::to_string(std::get<double>(value));
        case FieldValueType::BYTES:
            return "[" + std::to_string(std::get<std::vector<uint8_t>>(value).size()) + " bytes]";
        case FieldValueType::STRING:
            return std::get<std::string>(value);
        case FieldValueType::BOOLEAN:
            return std::get<bool>(value) ? "true" : "false";
        case FieldValueType::MAC_ADDRESS:
        case FieldValueType::IPV4_ADDRESS:
        case FieldValueType::IPV6_ADDRESS:
        case FieldValueType::TIMESTAMP:
        case FieldValueType::CUSTOM:
            return formatted.empty() ? "formatted" : formatted;
        default:
            return "unknown";
    }
}

std::string FieldValue::toHexString() const {
    if (!valid) return "INVALID";
    
    if (type == FieldValueType::BYTES) {
        auto bytes = std::get<std::vector<uint8_t>>(value);
        std::stringstream ss;
        for (size_t i = 0; i < std::min(bytes.size(), size_t(16)); ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]) << " ";
        }
        if (bytes.size() > 16) ss << "...";
        return ss.str();
    }
    
    return rawHex;
}

std::string FieldValue::toJsonString() const {
    if (!valid) return "null";
    
    std::stringstream ss;
    ss << "{";
    ss << "\"type\":\"" << static_cast<int>(type) << "\",";
    ss << "\"value\":";
    
    switch (type) {
        case FieldValueType::UINT8:
        case FieldValueType::UINT16:
        case FieldValueType::UINT32:
        case FieldValueType::UINT64:
        case FieldValueType::INT8:
        case FieldValueType::INT16:
        case FieldValueType::INT32:
        case FieldValueType::INT64:
        case FieldValueType::FLOAT32:
        case FieldValueType::FLOAT64:
        case FieldValueType::BOOLEAN:
            ss << toString();
            break;
        case FieldValueType::STRING:
            ss << "\"" << std::get<std::string>(value) << "\"";
            break;
        case FieldValueType::BYTES:
            ss << "\"" << toHexString() << "\"";
            break;
        default:
            ss << "\"" << toString() << "\"";
            break;
    }
    
    if (!rawHex.empty()) {
        ss << ",\"raw_hex\":\"" << rawHex << "\"";
    }
    
    if (!formatted.empty()) {
        ss << ",\"formatted\":\"" << formatted << "\"";
    }
    
    ss << ",\"parse_time\":" << parseTime.count();
    ss << "}";
    
    return ss.str();
}

size_t ParseResult::getValidationErrorCount() const {
    size_t count = 0;
    for (const auto& result : validationResults) {
        if (!result.valid) count++;
    }
    return count;
}

std::string ParseResult::getFieldString(const std::string& name) const {
    auto it = fields.find(name);
    if (it != fields.end() && it->second.valid) {
        return it->second.toString();
    }
    return "";
}

std::vector<uint8_t> ParseResult::getFieldBytes(const std::string& name) const {
    auto it = fields.find(name);
    if (it != fields.end() && it->second.valid && it->second.type == FieldValueType::BYTES) {
        return it->second.get<std::vector<uint8_t>>();
    }
    return {};
}

uint64_t ParseResult::getFieldUInt(const std::string& name) const {
    auto it = fields.find(name);
    if (it != fields.end() && it->second.valid) {
        switch (it->second.type) {
            case FieldValueType::UINT8:
                return it->second.get<uint8_t>();
            case FieldValueType::UINT16:
                return it->second.get<uint16_t>();
            case FieldValueType::UINT32:
                return it->second.get<uint32_t>();
            case FieldValueType::UINT64:
                return it->second.get<uint64_t>();
            case FieldValueType::INT8:
                return static_cast<uint64_t>(it->second.get<int8_t>());
            case FieldValueType::INT16:
                return static_cast<uint64_t>(it->second.get<int16_t>());
            case FieldValueType::INT32:
                return static_cast<uint64_t>(it->second.get<int32_t>());
            case FieldValueType::INT64:
                return static_cast<uint64_t>(it->second.get<int64_t>());
            default:
                break;
        }
    }
    return 0;
}

int64_t ParseResult::getFieldInt(const std::string& name) const {
    auto it = fields.find(name);
    if (it != fields.end() && it->second.valid) {
        switch (it->second.type) {
            case FieldValueType::UINT8:
                return static_cast<int64_t>(it->second.get<uint8_t>());
            case FieldValueType::UINT16:
                return static_cast<int64_t>(it->second.get<uint16_t>());
            case FieldValueType::UINT32:
                return static_cast<int64_t>(it->second.get<uint32_t>());
            case FieldValueType::UINT64:
                return static_cast<int64_t>(it->second.get<uint64_t>());
            case FieldValueType::INT8:
                return it->second.get<int8_t>();
            case FieldValueType::INT16:
                return it->second.get<int16_t>();
            case FieldValueType::INT32:
                return it->second.get<int32_t>();
            case FieldValueType::INT64:
                return it->second.get<int64_t>();
            default:
                break;
        }
    }
    return 0;
}

double ParseResult::getFieldFloat(const std::string& name) const {
    auto it = fields.find(name);
    if (it != fields.end() && it->second.valid) {
        switch (it->second.type) {
            case FieldValueType::FLOAT32:
                return static_cast<double>(it->second.get<float>());
            case FieldValueType::FLOAT64:
                return it->second.get<double>();
            default:
                break;
        }
    }
    return 0.0;
}

bool ParseResult::getFieldBool(const std::string& name) const {
    auto it = fields.find(name);
    if (it != fields.end() && it->second.valid && it->second.type == FieldValueType::BOOLEAN) {
        return it->second.get<bool>();
    }
    return false;
}

std::string ParseResult::toJsonString() const {
    std::stringstream ss;
    ss << "{";
    ss << "\"status\":" << static_cast<int>(status) << ",";
    ss << "\"protocol_name\":\"" << protocolName << "\",";
    ss << "\"protocol_version\":\"" << protocolVersion << "\",";
    ss << "\"packet_length\":" << packetLength << ",";
    ss << "\"parsed_bytes\":" << parsedBytes << ",";
    ss << "\"total_parse_time\":" << totalParseTime.count() << ",";
    ss << "\"total_validation_time\":" << totalValidationTime.count() << ",";
    
    ss << "\"fields\":{";
    bool first = true;
    for (const auto& [name, value] : fields) {
        if (!first) ss << ",";
        ss << "\"" << name << "\":" << value.toJsonString();
        first = false;
    }
    ss << "},";
    
    ss << "\"validation_results\":[";
    first = true;
    for (const auto& result : validationResults) {
        if (!first) ss << ",";
        ss << "{";
        ss << "\"field_name\":\"" << result.fieldName << "\",";
        ss << "\"valid\":" << (result.valid ? "true" : "false") << ",";
        ss << "\"error_message\":\"" << result.errorMessage << "\",";
        ss << "\"validation_time\":" << result.validationTime.count();
        ss << "}";
        first = false;
    }
    ss << "]";
    
    if (!errorMessage.empty()) {
        ss << ",\"error_message\":\"" << errorMessage << "\"";
    }
    
    ss << "}";
    return ss.str();
}

std::string ParseResult::toXmlString() const {
    std::stringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    ss << "<parse_result>\n";
    ss << "  <status>" << static_cast<int>(status) << "</status>\n";
    ss << "  <protocol_name>" << protocolName << "</protocol_name>\n";
    ss << "  <protocol_version>" << protocolVersion << "</protocol_version>\n";
    ss << "  <packet_length>" << packetLength << "</packet_length>\n";
    ss << "  <parsed_bytes>" << parsedBytes << "</parsed_bytes>\n";
    ss << "  <total_parse_time>" << totalParseTime.count() << "</total_parse_time>\n";
    ss << "  <total_validation_time>" << totalValidationTime.count() << "</total_validation_time>\n";
    
    ss << "  <fields>\n";
    for (const auto& [name, value] : fields) {
        ss << "    <field name=\"" << name << "\">\n";
        ss << "      <value>" << value.toString() << "</value>\n";
        ss << "      <type>" << static_cast<int>(value.type) << "</type>\n";
        if (!value.rawHex.empty()) {
            ss << "      <raw_hex>" << value.rawHex << "</raw_hex>\n";
        }
        ss << "    </field>\n";
    }
    ss << "  </fields>\n";
    
    if (!validationResults.empty()) {
        ss << "  <validation_results>\n";
        for (const auto& result : validationResults) {
            ss << "    <result field=\"" << result.fieldName << "\" valid=\"" << (result.valid ? "true" : "false") << "\">\n";
            if (!result.errorMessage.empty()) {
                ss << "      <error>" << result.errorMessage << "</error>\n";
            }
            ss << "    </result>\n";
        }
        ss << "  </validation_results>\n";
    }
    
    if (!errorMessage.empty()) {
        ss << "  <error_message>" << errorMessage << "</error_message>\n";
    }
    
    ss << "</parse_result>";
    return ss.str();
}

std::string ParseResult::toCsvString() const {
    std::stringstream ss;
    ss << "Field,Value,Type,Valid,ParseTime\n";
    
    for (const auto& [name, value] : fields) {
        ss << name << ",";
        ss << value.toString() << ",";
        ss << static_cast<int>(value.type) << ",";
        ss << (value.valid ? "true" : "false") << ",";
        ss << value.parseTime.count() << "\n";
    }
    
    return ss.str();
}

std::string ParseResult::toHumanReadableString() const {
    std::stringstream ss;
    ss << "Protocol: " << protocolName << " v" << protocolVersion << "\n";
    ss << "Status: " << (isSuccess() ? "SUCCESS" : "FAILED") << "\n";
    ss << "Packet Length: " << packetLength << " bytes\n";
    ss << "Parsed Bytes: " << parsedBytes << " bytes\n";
    ss << "Parse Time: " << totalParseTime.count() << " μs\n";
    ss << "Validation Time: " << totalValidationTime.count() << " μs\n\n";
    
    ss << "Fields:\n";
    for (const auto& [name, value] : fields) {
        ss << "  " << name << ": " << value.toString();
        if (!value.formatted.empty()) {
            ss << " (" << value.formatted << ")";
        }
        ss << "\n";
    }
    
    if (!validationResults.empty()) {
        ss << "\nValidation Results:\n";
        for (const auto& result : validationResults) {
            ss << "  " << result.fieldName << ": " << (result.valid ? "PASS" : "FAIL");
            if (!result.errorMessage.empty()) {
                ss << " - " << result.errorMessage;
            }
            ss << "\n";
        }
    }
    
    if (!errorMessage.empty()) {
        ss << "\nError: " << errorMessage << "\n";
    }
    
    return ss.str();
}

void ParseResult::addField(const std::string& name, const FieldValue& value) {
    fields[name] = value;
}

void ParseResult::addValidationResult(const ValidationResult& result) {
    validationResults.push_back(result);
}

void ParseResult::setError(ParseStatus status, const std::string& message) {
    this->status = status;
    this->errorMessage = message;
}

void ParseResult::clear() {
    status = ParseStatus::SUCCESS;
    protocolName.clear();
    protocolVersion.clear();
    fields.clear();
    validationResults.clear();
    errorMessage.clear();
    totalParseTime = std::chrono::microseconds(0);
    totalValidationTime = std::chrono::microseconds(0);
    packetLength = 0;
    parsedBytes = 0;
    rawData.clear();
}

ParseResultBuilder::ParseResultBuilder() {
    result_ = ParseResult{};
}

ParseResultBuilder& ParseResultBuilder::setProtocol(const std::string& name, const std::string& version) {
    result_.protocolName = name;
    result_.protocolVersion = version;
    return *this;
}

ParseResultBuilder& ParseResultBuilder::addField(const std::string& name, const FieldValue& value) {
    result_.addField(name, value);
    return *this;
}

ParseResultBuilder& ParseResultBuilder::addValidationResult(const ValidationResult& result) {
    result_.addValidationResult(result);
    return *this;
}

ParseResultBuilder& ParseResultBuilder::setError(ParseStatus status, const std::string& message) {
    result_.setError(status, message);
    return *this;
}

ParseResultBuilder& ParseResultBuilder::setPacketInfo(size_t length, size_t parsed) {
    result_.packetLength = length;
    result_.parsedBytes = parsed;
    return *this;
}

ParseResultBuilder& ParseResultBuilder::setRawData(const std::vector<uint8_t>& data) {
    result_.rawData = data;
    return *this;
}

ParseResultBuilder& ParseResultBuilder::setTiming(std::chrono::microseconds parseTime, std::chrono::microseconds validationTime) {
    result_.totalParseTime = parseTime;
    result_.totalValidationTime = validationTime;
    return *this;
}

ParseResult ParseResultBuilder::build() const {
    return result_;
}

void ParseResultBuilder::reset() {
    result_ = ParseResult{};
}

} // namespace parser
} // namespace beatrice 