#include "beatrice/PluginManager.hpp"
#include "beatrice/Logger.hpp"
#include "beatrice/Config.hpp"
#include "beatrice/Packet.hpp"
#include "beatrice/Metrics.hpp"
#include <iostream>
#include <memory>
#include <chrono>
#include <random>
#include <iomanip>

int main() {
    try {
        // Initialize logging
        beatrice::Logger::initialize("performance_test", "", beatrice::LogLevel::INFO);
        
        BEATRICE_INFO("Starting performance test");
        
        // Initialize configuration and metrics
        if (!beatrice::Config::initialize()) {
            BEATRICE_WARN("Failed to initialize configuration, using defaults");
        }
        
        auto& metrics = beatrice::MetricsRegistry::get();
        
        // Create plugin manager
        auto pluginMgr = std::make_unique<beatrice::PluginManager>();
        
        // Load the simple plugin
        std::string pluginPath = "./examples/libsimple_plugin.so";
        if (!pluginMgr->loadPlugin(pluginPath)) {
            BEATRICE_ERROR("Failed to load plugin: {}", pluginPath);
            return 1;
        }
        
        BEATRICE_INFO("Plugin loaded successfully");
        
        // Performance test parameters
        const size_t numPackets = 100000;
        const size_t batchSize = 1000;
        const size_t numBatches = numPackets / batchSize;
        
        BEATRICE_INFO("Performance test parameters:");
        BEATRICE_INFO("  Total packets: {}", numPackets);
        BEATRICE_INFO("  Batch size: {}", batchSize);
        BEATRICE_INFO("  Number of batches: {}", numBatches);
        
        // Create performance metrics
        auto processingTime = metrics.createHistogram("performance_processing_time", "Packet processing time");
        auto throughput = metrics.createGauge("performance_throughput", "Packets per second");
        auto totalPackets = metrics.createCounter("performance_total_packets", "Total packets processed");
        
        // Generate test packets
        std::vector<beatrice::Packet> testPackets;
        testPackets.reserve(batchSize);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> sizeDist(64, 1500);
        std::uniform_int_distribution<> protocolDist(1, 17);
        
        BEATRICE_INFO("Generating test packets...");
        for (size_t i = 0; i < batchSize; ++i) {
            size_t packetSize = sizeDist(gen);
            std::vector<uint8_t> packetData(packetSize);
            
            // Fill with random data
            std::uniform_int_distribution<> byteDist(0, 255);
            for (size_t j = 0; j < packetSize; ++j) {
                packetData[j] = static_cast<uint8_t>(byteDist(gen));
            }
            
            // Set up basic packet structure (simplified)
            if (packetSize >= 14) {
                // Set EtherType to IPv4
                packetData[12] = 0x08;
                packetData[13] = 0x00;
            }
            
            if (packetSize >= 34) {
                // Set IP protocol
                packetData[23] = static_cast<uint8_t>(protocolDist(gen));
            }
            
            auto data = std::make_shared<uint8_t[]>(packetSize);
            std::copy(packetData.begin(), packetData.end(), data.get());
            
            beatrice::Packet packet(data, packetSize);
            packet.metadata().interface = "eth0";
            testPackets.push_back(std::move(packet));
        }
        
        BEATRICE_INFO("Generated {} test packets", testPackets.size());
        
        // Warm-up run
        BEATRICE_INFO("Running warm-up...");
        for (const auto& packet : testPackets) {
            pluginMgr->processPacket(const_cast<beatrice::Packet&>(packet));
        }
        
        // Performance test
        BEATRICE_INFO("Starting performance test...");
        auto startTime = std::chrono::high_resolution_clock::now();
        
        for (size_t batch = 0; batch < numBatches; ++batch) {
            auto batchStartTime = std::chrono::high_resolution_clock::now();
            
            for (const auto& packet : testPackets) {
                pluginMgr->processPacket(const_cast<beatrice::Packet&>(packet));
            }
            
            auto batchEndTime = std::chrono::high_resolution_clock::now();
            auto batchDuration = std::chrono::duration_cast<std::chrono::microseconds>(batchEndTime - batchStartTime);
            
            processingTime->observe(batchDuration.count());
            totalPackets->increment(batchSize);
            
            if ((batch + 1) % 10 == 0) {
                BEATRICE_INFO("Processed batch {}/{} in {} μs", batch + 1, numBatches, batchDuration.count());
            }
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        // Calculate results
        double totalSeconds = totalDuration.count() / 1000000.0;
        double packetsPerSecond = numPackets / totalSeconds;
        double mbps = (numPackets * 1000.0) / (totalSeconds * 1024 * 1024); // Assuming average packet size
        
        // Update throughput metric
        throughput->set(packetsPerSecond);
        
        // Print results
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "PERFORMANCE TEST RESULTS\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "Total packets processed: " << numPackets << "\n";
        std::cout << "Total time: " << std::fixed << std::setprecision(3) << totalSeconds << " seconds\n";
        std::cout << "Throughput: " << std::fixed << std::setprecision(2) << packetsPerSecond << " packets/sec\n";
        std::cout << "Bandwidth: " << std::fixed << std::setprecision(2) << mbps << " Mbps\n";
        std::cout << "Average processing time: " << std::fixed << std::setprecision(3) 
                  << (totalDuration.count() / numPackets) << " μs per packet\n";
        std::cout << "Batch size: " << batchSize << "\n";
        std::cout << "Number of batches: " << numBatches << "\n";
        std::cout << std::string(60, '=') << "\n";
        
        // Print metrics
        BEATRICE_INFO("Performance test completed");
        BEATRICE_INFO("Total packets: {}", totalPackets->getValue());
        BEATRICE_INFO("Throughput: {:.2f} packets/sec", throughput->getValue());
        BEATRICE_INFO("Average processing time: {:.3f} μs", processingTime->getSum() / processingTime->getCount());
        
        // Export metrics
        std::cout << "\nMetrics (Prometheus format):\n";
        std::cout << metrics.exportPrometheus() << "\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Performance test failed: " << e.what() << std::endl;
        return 1;
    }
} 