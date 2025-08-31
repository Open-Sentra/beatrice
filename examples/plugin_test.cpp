#include "beatrice/PluginManager.hpp"
#include "beatrice/Logger.hpp"
#include "beatrice/Config.hpp"
#include "beatrice/Packet.hpp"
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>

int main() {
    try {
        // Initialize logging
        beatrice::Logger::get().initialize("plugin_test", "", 1024*1024, 5);
        
        BEATRICE_INFO("Starting plugin test");
        
        // Initialize configuration
        auto& config = beatrice::Config::get();
        auto result = config.initialize("./config.json");
        if (!result.isSuccess()) {
            BEATRICE_WARN("Failed to initialize configuration, using defaults");
        }
        
        // Create plugin manager
        auto pluginMgr = std::make_unique<beatrice::PluginManager>();
        
        // Set configuration
        pluginMgr->setMaxPlugins(5);
        
        // Load the simple plugin
        std::string pluginPath = "./examples/libsimple_plugin.so";
        if (!pluginMgr->loadPlugin(pluginPath)) {
            BEATRICE_ERROR("Failed to load plugin: {}", pluginPath);
            return 1;
        }
        
        BEATRICE_INFO("Plugin loaded successfully");
        
        // Get plugin information
        auto pluginNames = pluginMgr->getLoadedPluginNames();
        BEATRICE_INFO("Loaded plugins: {}", pluginNames.size());
        for (const auto& name : pluginNames) {
            BEATRICE_INFO("  - {}", name);
        }
        
        // Create test packets
        std::vector<beatrice::Packet> testPackets;
        
        // TCP packet
        {
            std::vector<uint8_t> tcpData = {
                0x00, 0x11, 0x22, 0x33, 0x44, 0x55,  // Destination MAC
                0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb,  // Source MAC
                0x08, 0x00,                           // EtherType (IPv4)
                0x45, 0x00, 0x00, 0x28, 0x00, 0x00, 0x40, 0x00,  // IP header
                0x40, 0x06, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x01,  // IP: 10.0.0.1
                0x0a, 0x00, 0x00, 0x02, 0x00, 0x50, 0x00, 0x51,  // IP: 10.0.0.2, Ports: 80->81
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // TCP header
                0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // TCP flags
            };
            
            auto data = std::make_shared<uint8_t[]>(tcpData.size());
            std::copy(tcpData.begin(), tcpData.end(), data.get());
            
            beatrice::Packet packet(data, tcpData.size());
            packet.metadata().interface = "eth0";
            testPackets.push_back(std::move(packet));
        }
        
        // UDP packet
        {
            std::vector<uint8_t> udpData = {
                0x00, 0x11, 0x22, 0x33, 0x44, 0x55,  // Destination MAC
                0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb,  // Source MAC
                0x08, 0x00,                           // EtherType (IPv4)
                0x45, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x40, 0x00,  // IP header
                0x40, 0x11, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x03,  // IP: 10.0.0.3, Protocol: UDP
                0x0a, 0x00, 0x00, 0x04, 0x00, 0x35, 0x00, 0x36,  // IP: 10.0.0.4, Ports: 53->54
                0x00, 0x08, 0x00, 0x00                               // UDP header
            };
            
            auto data = std::make_shared<uint8_t[]>(udpData.size());
            std::copy(udpData.begin(), udpData.end(), data.get());
            
            beatrice::Packet packet(data, udpData.size());
            packet.metadata().interface = "eth0";
            testPackets.push_back(std::move(packet));
        }
        
        // ICMP packet
        {
            std::vector<uint8_t> icmpData = {
                0x00, 0x11, 0x22, 0x33, 0x44, 0x55,  // Destination MAC
                0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb,  // Source MAC
                0x08, 0x00,                           // EtherType (IPv4)
                0x45, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x40, 0x00,  // IP header
                0x40, 0x01, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x05,  // IP: 10.0.0.5, Protocol: ICMP
                0x0a, 0x00, 0x00, 0x06, 0x08, 0x00, 0x00, 0x00,  // IP: 10.0.0.6, ICMP type: echo request
                0x00, 0x01, 0x00, 0x01                               // ICMP header
            };
            
            auto data = std::make_shared<uint8_t[]>(icmpData.size());
            std::copy(icmpData.begin(), icmpData.end(), data.get());
            
            beatrice::Packet packet(data, icmpData.size());
            packet.metadata().interface = "eth0";
            testPackets.push_back(std::move(packet));
        }
        
        BEATRICE_INFO("Created {} test packets", testPackets.size());
        
        // Process packets through plugins
        BEATRICE_INFO("Processing test packets through plugins");
        for (size_t i = 0; i < testPackets.size(); ++i) {
            BEATRICE_DEBUG("Processing packet {}", i + 1);
            pluginMgr->processPacket(testPackets[i]);
            
            // Small delay to see the output
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Get plugin statistics
        BEATRICE_INFO("Plugin processing complete");
        BEATRICE_INFO("Plugin count: {}", pluginMgr->getPluginCount());
        
        // Unload plugin
        BEATRICE_INFO("Unloading plugin");
        pluginMgr->unloadPlugin("SimplePlugin");
        
        BEATRICE_INFO("Plugin test completed successfully");
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Plugin test failed: " << e.what() << std::endl;
        return 1;
    }
} 