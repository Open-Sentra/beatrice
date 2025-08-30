#include "beatrice/BeatriceContext.hpp"
#include "beatrice/PMDBackend.hpp"
#include "beatrice/PluginManager.hpp"
#include "beatrice/Logger.hpp"
#include <iostream>
#include <memory>
#include <map>

int main() {
    std::cout << "=== Beatrice PMD Backend Example ===" << std::endl;
    
    beatrice::Logger::get().initialize("pmd_example", "", 1024*1024, 5);
    
    auto backend = std::make_unique<beatrice::PMDBackend>();
    auto pluginMgr = std::make_unique<beatrice::PluginManager>();
    
    std::cout << "\n1. Backend Information:" << std::endl;
    std::cout << "  Name: " << backend->getName() << std::endl;
    std::cout << "  Version: " << backend->getVersion() << std::endl;
    
    std::cout << "\n2. Available PMDs:" << std::endl;
    for (const auto& pmd : backend->getAvailablePMDs()) {
        std::cout << "  ✓ " << pmd << std::endl;
    }
    
    std::cout << "\n3. Setting PMD Type to net_tap..." << std::endl;
    auto pmdResult = backend->setPMDType("net_tap");
    if (pmdResult.isSuccess()) {
        std::cout << "  ✓ PMD type set successfully" << std::endl;
    } else {
        std::cout << "  ✗ Failed to set PMD type: " << pmdResult.getErrorMessage() << std::endl;
        return 1;
    }
    
    std::cout << "\n4. Setting DPDK Arguments..." << std::endl;
    std::vector<std::string> dpdkArgs = {
        "-l", "0-3",
        "-n", "4",
        "--file-prefix", "beatrice_pmd"
    };
    
    auto argsResult = backend->setPMDArgs(dpdkArgs);
    if (argsResult.isSuccess()) {
        std::cout << "  ✓ DPDK arguments set successfully" << std::endl;
    } else {
        std::cout << "  ✗ Failed to set DPDK arguments: " << argsResult.getErrorMessage() << std::endl;
        return 1;
    }
    
    std::cout << "\n5. Adding Virtual TAP Device..." << std::endl;
    std::map<std::string, std::string> tapParams = {
        {"iface", "dpdk_tap0"},
        {"mac", "00:11:22:33:44:55"}
    };
    
    auto vdevResult = backend->addVirtualDevice("net_tap", tapParams);
    if (vdevResult.isSuccess()) {
        std::cout << "  ✓ Virtual TAP device added successfully" << std::endl;
    } else {
        std::cout << "  ✗ Failed to add virtual device: " << vdevResult.getErrorMessage() << std::endl;
        return 1;
    }
    
    std::cout << "\n6. Configuring Backend..." << std::endl;
    beatrice::ICaptureBackend::Config config;
    config.interface = "dpdk_tap0";
    config.bufferSize = 2048;
    config.numBuffers = 2048;
    config.batchSize = 32;
    config.promiscuous = true;
    config.enableTimestamping = true;
    config.enableZeroCopy = true;
    
    std::cout << "  Interface: " << config.interface << std::endl;
    std::cout << "  Buffer Size: " << config.bufferSize << std::endl;
    std::cout << "  Num Buffers: " << config.numBuffers << std::endl;
    std::cout << "  Batch Size: " << config.batchSize << std::endl;
    
    std::cout << "\n7. Initializing PMD Backend..." << std::endl;
    auto initResult = backend->initialize(config);
    if (initResult.isSuccess()) {
        std::cout << "  ✓ PMD backend initialized successfully" << std::endl;
        std::cout << "  DPDK Initialized: " << (backend->isHealthy() ? "Yes" : "No") << std::endl;
    } else {
        std::cout << "  ✗ Failed to initialize PMD backend: " << initResult.getErrorMessage() << std::endl;
        std::cout << "  This is expected if DPDK is not fully configured or no DPDK ports are available" << std::endl;
        return 1;
    }
    
    std::cout << "\n8. Starting PMD Backend..." << std::endl;
    auto startResult = backend->start();
    if (startResult.isSuccess()) {
        std::cout << "  ✓ PMD backend started successfully" << std::endl;
    } else {
        std::cout << "  ✗ Failed to start PMD backend: " << startResult.getErrorMessage() << std::endl;
        return 1;
    }
    
    std::cout << "\n9. Available DPDK Ports:" << std::endl;
    for (const auto& port : backend->getAvailablePorts()) {
        std::cout << "  ✓ " << port << std::endl;
    }
    
    std::cout << "\n10. Health Check..." << std::endl;
    auto healthResult = backend->healthCheck();
    if (healthResult.isSuccess()) {
        std::cout << "  ✓ Health check passed" << std::endl;
    } else {
        std::cout << "  ✗ Health check failed: " << healthResult.getErrorMessage() << std::endl;
    }
    
    std::cout << "\n11. Running PMD Backend..." << std::endl;
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
        std::cout << "\nStopping PMD backend..." << std::endl;
    }
    
    std::cout << "\n12. Stopping PMD Backend..." << std::endl;
    backend->stop();
    
    std::cout << "\n=== PMD Backend Example Completed ===" << std::endl;
    std::cout << "Note: PMD backend provides virtual network interfaces for testing without physical NICs." << std::endl;
    
    return 0;
} 