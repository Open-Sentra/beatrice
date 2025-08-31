#ifndef BEATRICE_PARSER_BUILTIN_PROTOCOLS_HPP
#define BEATRICE_PARSER_BUILTIN_PROTOCOLS_HPP

#include "FieldDefinition.hpp"
#include <vector>

namespace beatrice {
namespace parser {

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

} // namespace parser
} // namespace beatrice

#endif // BEATRICE_PARSER_BUILTIN_PROTOCOLS_HPP 