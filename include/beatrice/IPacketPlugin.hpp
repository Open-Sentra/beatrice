#ifndef BEATRICE_IPACKETPLUGIN_HPP
#define BEATRICE_IPACKETPLUGIN_HPP

#include "Packet.hpp"
#include <string>

namespace beatrice {

class IPacketPlugin {
public:
    virtual ~IPacketPlugin() = default;
    
    // Plugin lifecycle
    virtual void onStart() = 0;
    virtual void onStop() = 0;
    
    // Packet processing
    virtual void onPacket(Packet& packet) = 0;
    
    // Plugin information
    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual std::string getDescription() const = 0;
    
    // Plugin configuration
    virtual bool isEnabled() const = 0;
    virtual void setEnabled(bool enabled) = 0;
    
    // Plugin statistics
    virtual uint64_t getProcessedPacketCount() const = 0;
    virtual uint64_t getErrorCount() const = 0;
    virtual void resetStatistics() = 0;
};

} // namespace beatrice

#endif // BEATRICE_IPACKETPLUGIN_HPP
