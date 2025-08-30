#include "beatrice/BeatriceContext.hpp"
#include "beatrice/AF_PacketBackend.hpp"
#include "beatrice/PluginManager.hpp"
#include "beatrice/Logger.hpp"
#include <iostream>
#include <memory>
#include <chrono>

int main() {
    std::cout << "=== Beatrice AF_PACKET Backend Example ===" << std::endl;
    
    beatrice::Logger::get().initialize("af_packet_example", "", 1024*1024, 5);
    
    auto backend = std::make_unique<beatrice::AF_PacketBackend>();
    auto pluginMgr = std::make_unique<beatrice::PluginManager>();
    
    std::cout << "\n1. Backend Information:" << std::endl;
    std::cout << "  Name: " << backend->getName() << std::endl;
    std::cout << "  Version: " << backend->getVersion() << std::endl;
    
    std::cout << "\n2. Available Features:" << std::endl;
    for (const auto& feature : backend->getSupportedFeatures()) {
        std::cout << "  ✓ " << feature << std::endl;
    }
    
    std::cout << "\n3. Configuring AF_PACKET Backend..." << std::endl;
    
    // Set AF_PACKET specific options
    auto promiscResult = backend->setPromiscuousMode(true);
    if (promiscResult.isSuccess()) {
        std::cout << "  ✓ Promiscuous mode enabled" << std::endl;
    } else {
        std::cout << "  ✗ Failed to enable promiscuous mode: " << promiscResult.getErrorMessage() << std::endl;
    }
    
    auto bufferResult = backend->setBufferSize(131072); // 128KB
    if (bufferResult.isSuccess()) {
        std::cout << "  ✓ Buffer size set to 128KB" << std::endl;
    } else {
        std::cout << "  ✗ Failed to set buffer size: " << bufferResult.getErrorMessage() << std::endl;
    }
    
    auto blockingResult = backend->setBlockingMode(false);
    if (blockingResult.isSuccess()) {
        std::cout << "  ✓ Non-blocking mode enabled" << std::endl;
    } else {
        std::cout << "  ✗ Failed to set non-blocking mode: " << blockingResult.getErrorMessage() << std::endl;
    }
    
    std::cout << "\n4. Backend Configuration:" << std::endl;
    std::cout << "  Promiscuous Mode: " << (backend->isPromiscuousMode() ? "Enabled" : "Disabled") << std::endl;
    std::cout << "  Buffer Size: " << backend->getBufferSize() << " bytes" << std::endl;
    std::cout << "  Blocking Mode: " << (backend->isBlockingMode() ? "Enabled" : "Disabled") << std::endl;
    
    std::cout << "\n5. Configuring Backend..." << std::endl;
    beatrice::ICaptureBackend::Config config;
    config.interface = "lo"; // Use loopback interface for testing
    config.bufferSize = 2048;
    config.numBuffers = 2048;
    config.batchSize = 32;
    config.promiscuous = true;
    config.enableTimestamping = true;
    config.enableZeroCopy = false; // AF_PACKET doesn't support zero-copy
    
    std::cout << "  Interface: " << config.interface << std::endl;
    std::cout << "  Buffer Size: " << config.bufferSize << std::endl;
    std::cout << "  Num Buffers: " << config.numBuffers << std::endl;
    std::cout << "  Batch Size: " << config.batchSize << std::endl;
    
    std::cout << "\n6. Initializing AF_PACKET Backend..." << std::endl;
    auto initResult = backend->initialize(config);
    if (initResult.isSuccess()) {
        std::cout << "  ✓ AF_PACKET backend initialized successfully" << std::endl;
        std::cout << "  Backend Healthy: " << (backend->isHealthy() ? "Yes" : "No") << std::endl;
    } else {
        std::cout << "  ✗ Failed to initialize AF_PACKET backend: " << initResult.getErrorMessage() << std::endl;
        std::cout << "  This is expected if running without root privileges or interface not available" << std::endl;
        return 1;
    }
    
    std::cout << "\n7. Starting AF_PACKET Backend..." << std::endl;
    auto startResult = backend->start();
    if (startResult.isSuccess()) {
        std::cout << "  ✓ AF_PACKET backend started successfully" << std::endl;
    } else {
        std::cout << "  ✗ Failed to start AF_PACKET backend: " << startResult.getErrorMessage() << std::endl;
        return 1;
    }
    
    std::cout << "\n8. Health Check..." << std::endl;
    auto healthResult = backend->healthCheck();
    if (healthResult.isSuccess()) {
        std::cout << "  ✓ Health check passed" << std::endl;
    } else {
        std::cout << "  ✗ Health check failed: " << healthResult.getErrorMessage() << std::endl;
    }
    
    std::cout << "\n9. Running AF_PACKET Backend..." << std::endl;
    std::cout << "  Press Ctrl+C to stop..." << std::endl;
    
    try {
        while (true) {
            auto packets = backend->getPackets(10, std::chrono::milliseconds(1000));
            if (!packets.empty()) {
                std::cout << "  Captured " << packets.size() << " packets" << std::endl;
            }
            
            auto stats = backend->getStatistics();
            if (stats.packetsCaptured > 0) {
                std::cout << "  Total packets: " << stats.packetsCaptured 
                         << ", Total bytes: " << stats.bytesCaptured << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    } catch (const std::exception& e) {
        std::cout << "\nStopping AF_PACKET backend..." << std::endl;
    }
    
    std::cout << "\n10. Stopping AF_PACKET Backend..." << std::endl;
    backend->stop();
    
    std::cout << "\n=== AF_PACKET Backend Example Completed ===" << std::endl;
    std::cout << "Note: AF_PACKET backend provides raw packet capture using Linux socket API." << std::endl;
    
    return 0;
} 