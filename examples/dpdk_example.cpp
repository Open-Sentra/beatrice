#include "beatrice/BeatriceContext.hpp"
#include "beatrice/DPDKBackend.hpp"
#include "beatrice/PluginManager.hpp"
#include "beatrice/Logger.hpp"
#include <iostream>
#include <memory>

int main() {
    beatrice::Logger::get().initialize("dpdk_example", "", 1024*1024, 5);
    
    auto backend = std::make_unique<beatrice::DPDKBackend>();
    auto pluginMgr = std::make_unique<beatrice::PluginManager>();
    
    beatrice::ICaptureBackend::Config config;
    config.interface = "0000:01:00.0";
    config.bufferSize = 2048;
    config.numBuffers = 2048;
    config.batchSize = 32;
    config.promiscuous = true;
    config.enableTimestamping = true;
    config.enableZeroCopy = true;
    
    std::vector<std::string> dpdkArgs = {
        "-l", "0-3",
        "-n", "4",
        "--file-prefix", "beatrice"
    };
    
    backend->setDPDKArgs(dpdkArgs);
    
    beatrice::BeatriceContext context(std::move(backend), std::move(pluginMgr));
    
    if (context.initialize()) {
        std::cout << "DPDK backend initialized successfully" << std::endl;
        context.run();
    } else {
        std::cerr << "Failed to initialize DPDK backend" << std::endl;
        return 1;
    }
    
    return 0;
} 