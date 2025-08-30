#include "beatrice/IPacketPlugin.hpp"
#include "beatrice/Logger.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

class SimplePlugin : public beatrice::IPacketPlugin {
public:
    SimplePlugin() : enabled_(true), processedCount_(0), errorCount_(0) {
        BEATRICE_DEBUG("SimplePlugin created");
    }
    
    ~SimplePlugin() override {
        BEATRICE_DEBUG("SimplePlugin destroyed");
    }
    
    // Plugin lifecycle
    void onStart() override {
        BEATRICE_INFO("SimplePlugin started");
    }
    
    void onStop() override {
        BEATRICE_INFO("SimplePlugin stopped. Processed {} packets, {} errors", 
                     processedCount_, errorCount_);
    }
    
    // Packet processing
    void onPacket(beatrice::Packet& packet) override {
        try {
            if (!enabled_) {
                return;
            }
            
            processedCount_++;
            
            // Basic packet analysis
            std::stringstream ss;
            ss << "Packet #" << processedCount_ << " (" << packet.size() << " bytes)";
            
            if (packet.isIPv4()) {
                ss << " IPv4";
                if (packet.isTCP()) {
                    ss << "/TCP " << packet.metadata().source_ip << ":" << packet.metadata().source_port
                       << " -> " << packet.metadata().destination_ip << ":" << packet.metadata().destination_port;
                } else if (packet.isUDP()) {
                    ss << "/UDP " << packet.metadata().source_ip << ":" << packet.metadata().source_port
                       << " -> " << packet.metadata().destination_ip << ":" << packet.metadata().destination_port;
                } else if (packet.isICMP()) {
                    ss << "/ICMP " << packet.metadata().source_ip << " -> " << packet.metadata().destination_ip;
                }
            } else if (packet.isIPv6()) {
                ss << " IPv6";
                if (packet.isTCP()) {
                    ss << "/TCP " << packet.metadata().source_ip << ":" << packet.metadata().source_port
                       << " -> " << packet.metadata().destination_ip << ":" << packet.metadata().destination_port;
                } else if (packet.isUDP()) {
                    ss << "/UDP " << packet.metadata().source_ip << ":" << packet.metadata().source_port
                       << " -> " << packet.metadata().destination_ip << ":" << packet.metadata().destination_port;
                }
            }
            
            // Print first few bytes as hex
            if (packet.size() > 0) {
                ss << " [";
                size_t bytesToShow = std::min(packet.size(), size_t(16));
                for (size_t i = 0; i < bytesToShow; ++i) {
                    if (i > 0) ss << " ";
                    ss << std::hex << std::setw(2) << std::setfill('0') 
                       << static_cast<int>(packet.data()[i]);
                }
                if (packet.size() > 16) ss << "...";
                ss << "]";
            }
            
            BEATRICE_INFO("{}", ss.str());
            
            // Update statistics
            updateStatistics(packet);
            
        } catch (const std::exception& e) {
            errorCount_++;
            BEATRICE_ERROR("Exception processing packet: {}", e.what());
        }
    }
    
    // Plugin information
    std::string getName() const override {
        return "SimplePlugin";
    }
    
    std::string getVersion() const override {
        return "1.0.0";
    }
    
    std::string getDescription() const override {
        return "A simple packet analysis plugin that logs packet information";
    }
    
    // Plugin configuration
    bool isEnabled() const override {
        return enabled_;
    }
    
    void setEnabled(bool enabled) override {
        enabled_ = enabled;
        BEATRICE_INFO("SimplePlugin {} {}", enabled ? "enabled" : "disabled");
    }
    
    // Plugin statistics
    uint64_t getProcessedPacketCount() const override {
        return processedCount_;
    }
    
    uint64_t getErrorCount() const override {
        return errorCount_;
    }
    
    void resetStatistics() override {
        processedCount_ = 0;
        errorCount_ = 0;
        BEATRICE_INFO("SimplePlugin statistics reset");
    }

private:
    bool enabled_;
    uint64_t processedCount_;
    uint64_t errorCount_;
    
    void updateStatistics(const beatrice::Packet& packet) {
        // Update protocol-specific statistics
        if (packet.isTCP()) {
            tcpCount_++;
        } else if (packet.isUDP()) {
            udpCount_++;
        } else if (packet.isICMP()) {
            icmpCount_++;
        }
        
        // Update size statistics
        if (packet.size() < 64) {
            smallPackets_++;
        } else if (packet.size() < 512) {
            mediumPackets_++;
        } else {
            largePackets_++;
        }
    }
    
    // Additional statistics
    uint64_t tcpCount_{0};
    uint64_t udpCount_{0};
    uint64_t icmpCount_{0};
    uint64_t smallPackets_{0};
    uint64_t mediumPackets_{0};
    uint64_t largePackets_{0};
};

// Plugin factory function
extern "C" beatrice::IPacketPlugin* createPlugin() {
    return new SimplePlugin();
}

// Plugin cleanup function
extern "C" void destroyPlugin(beatrice::IPacketPlugin* plugin) {
    delete plugin;
}
