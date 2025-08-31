#include <parser/ProtocolParser.hpp>
#include <parser/FieldDefinition.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <cstdint> // Required for uint8_t and uint16_t
#include <iomanip>  // Required for std::hex and std::dec
#include <algorithm> // Required for std::min

int main() {
    try {
        std::cout << "=== Beatrice Protocol Parser Example ===" << std::endl;
        
        auto parser = beatrice::parser::ProtocolParser::create();
        
        std::cout << "\n1. Creating custom protocol..." << std::endl;
        
        beatrice::parser::ProtocolDefinition customProtocol("CUSTOM_PROTO", "1.0");
        customProtocol.addField(beatrice::parser::FieldFactory::createUInt32Field("header", 0, beatrice::parser::Endianness::NETWORK, true, "Protocol header"));
        customProtocol.addField(beatrice::parser::FieldFactory::createUInt8Field("version", 4, true, "Protocol version"));
        customProtocol.addField(beatrice::parser::FieldFactory::createUInt16Field("length", 5, beatrice::parser::Endianness::NETWORK, true, "Data length"));
        customProtocol.addField(beatrice::parser::FieldFactory::createBytesField("data", 7, 10, "Payload data"));
        
        parser->registerProtocol(customProtocol);
        
        std::cout << "Custom protocol registered: " << customProtocol.name << " v" << customProtocol.version << std::endl;
        std::cout << "Fields: " << customProtocol.getFieldCount() << std::endl;
        
        std::cout << "\n2. Creating test packet..." << std::endl;
        
        std::vector<uint8_t> testPacket = {
            0x12, 0x34, 0x56, 0x78,  // Header: 0x12345678
            0x01,                     // Version: 1
            0x00, 0x0A,              // Length: 10
            0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44  // Data
        };
        
        std::cout << "Packet size: " << testPacket.size() << " bytes" << std::endl;
        
        std::cout << "\n3. Parsing packet..." << std::endl;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        auto result = parser->parsePacket(testPacket, "CUSTOM_PROTO");
        auto endTime = std::chrono::high_resolution_clock::now();
        
        auto parseTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        if (result.isSuccess()) {
            std::cout << "Parse successful!" << std::endl;
            std::cout << "Protocol: " << result.protocolName << " v" << result.protocolVersion << std::endl;
            std::cout << "Fields parsed: " << result.getFieldCount() << std::endl;
            std::cout << "Parse time: " << parseTime.count() << " μs" << std::endl;
            
            std::cout << "\n4. Field values:" << std::endl;
            
            if (result.hasField("header")) {
                uint32_t header = result.getFieldUInt("header");
                std::cout << "  Header: 0x" << std::hex << header << std::dec << std::endl;
            }
            
            if (result.hasField("version")) {
                uint8_t version = static_cast<uint8_t>(result.getFieldUInt("version"));
                std::cout << "  Version: " << static_cast<int>(version) << std::endl;
            }
            
            if (result.hasField("length")) {
                uint16_t length = static_cast<uint16_t>(result.getFieldUInt("length"));
                std::cout << "  Length: " << length << std::endl;
            }
            
            if (result.hasField("data")) {
                auto data = result.getFieldBytes("data");
                std::cout << "  Data: ";
                for (size_t i = 0; i < std::min(data.size(), size_t(8)); ++i) {
                    std::cout << std::hex << std::setw(2) << std::setfill('0') 
                             << static_cast<int>(data[i]) << " ";
                }
                if (data.size() > 8) std::cout << "...";
                std::cout << std::dec << std::endl;
            }
            
            std::cout << "\n5. JSON output:" << std::endl;
            std::cout << result.toJsonString() << std::endl;
            
        } else {
            std::cout << "Parse failed: " << result.errorMessage << std::endl;
        }
        
        std::cout << "\n6. Parser statistics:" << std::endl;
        auto stats = parser->getStats();
        std::cout << "  Total packets: " << stats.totalPacketsParsed << std::endl;
        std::cout << "  Successful: " << stats.successfulParses << std::endl;
        std::cout << "  Failed: " << stats.failedParses << std::endl;
        std::cout << "  Average parse time: " << stats.averageParseTime.count() << " μs" << std::endl;
        
        std::cout << "\n=== Example completed successfully! ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 