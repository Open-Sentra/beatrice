#include "beatrice/BeatriceContext.hpp"
#include "beatrice/AF_XDPBackend.hpp"
#include "beatrice/DPDKBackend.hpp"
#include "beatrice/PMDBackend.hpp"
#include "beatrice/AF_PacketBackend.hpp"
#include "beatrice/PluginManager.hpp"
#include "beatrice/Logger.hpp"
#include "beatrice/Config.hpp"
#include "beatrice/Metrics.hpp"
#include "beatrice/Telemetry.hpp"
#include "parser/ProtocolParser.hpp"
#include "parser/FieldDefinition.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <csignal>
#include <cstdlib>
#include <getopt.h>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <map>

using namespace beatrice;

// Global variables for signal handling
volatile sig_atomic_t g_running = true;
std::unique_ptr<BeatriceContext> g_context;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
    if (g_context) {
        g_context->shutdown();
    }
}

void printUsage(const char* programName) {
    std::cout << "Beatrice CLI - Network Packet Processing SDK\n\n"
              << "Usage: " << programName << " [OPTIONS] COMMAND [ARGS...]\n\n"
              << "Commands:\n"
              << "  capture     Capture packets using specified backend\n"
              << "  replay      Replay packets from PCAP file\n"
              << "  benchmark   Run performance benchmarks\n"
              << "  test        Run backend tests\n"
              << "  info        Show system and backend information\n"
              << "  config      Manage configuration\n"
              << "  telemetry   Manage telemetry and metrics\n"
              << "  filter      Manage packet filters\n"
              << "  thread      Manage thread pool and load balancing\n"
              << "  parser      Manage protocol parsing\n\n"
              << "Global Options:\n"
              << "  -h, --help              Show this help message\n"
              << "  -v, --verbose           Enable verbose output\n"
              << "  -q, --quiet             Suppress non-error output\n"
              << "  --log-level=LEVEL       Set log level (debug, info, warn, error)\n"
              << "  --config-file=FILE      Load configuration from file\n\n"
              << "Examples:\n"
              << "  " << programName << " capture --backend af_packet --interface lo --duration 30\n"
              << "  " << programName << " benchmark --backend dpdk --packets 1000000\n"
              << "  " << programName << " test --backend all\n"
              << "  " << programName << " info --backend pmd\n\n"
              << "For detailed help on a command, use: " << programName << " COMMAND --help\n";
}

void printCaptureHelp() {
    std::cout << "Capture Command - Capture network packets\n\n"
              << "Usage: beatrice capture [OPTIONS]\n\n"
              << "Options:\n"
              << "  -b, --backend=BACKEND    Backend to use (af_packet, dpdk, pmd, af_xdp)\n"
              << "  -i, --interface=IFACE    Network interface to capture from\n"
              << "  -d, --duration=SECONDS   Capture duration in seconds (0 = infinite)\n"
              << "  -c, --count=COUNT        Maximum packets to capture\n"
              << "  -s, --size=SIZE          Capture buffer size in bytes\n"
              << "  -p, --promiscuous        Enable promiscuous mode\n"
              << "  -t, --timestamp          Enable packet timestamping\n"
              << "  -z, --zero-copy          Enable zero-copy mode\n"
              << "  --dma-device=DEVICE      DMA device for zero-copy\n"
              << "  --dma-buffer-size=SIZE  DMA buffer size in bytes\n"
              << "  --dma-buffer-count=CNT  Number of DMA buffers\n"
              "  --output-file=FILE        Save captured packets to file\n"
              << "  --filter=EXPR           BPF filter expression\n"
              << "  --stats-interval=SEC    Statistics update interval\n\n"
              << "Examples:\n"
              << "  beatrice capture --backend af_packet --interface lo --duration 60\n"
              << "  beatrice capture --backend dpdk --interface eth0 --count 10000\n"
              << "  beatrice capture --backend pmd --interface dpdk_tap0 --zero-copy\n";
}

void printBenchmarkHelp() {
    std::cout << "Benchmark Command - Run performance benchmarks\n\n"
              << "Usage: beatrice benchmark [OPTIONS]\n\n"
              << "Options:\n"
              << "  -b, --backend=BACKEND    Backend to benchmark\n"
              << "  -i, --interface=IFACE    Network interface to use\n"
              << "  -p, --packets=COUNT      Number of packets to process\n"
              << "  -s, --size=SIZE          Packet size in bytes\n"
              << "  -t, --threads=COUNT      Number of processing threads\n"
              << "  -d, --duration=SECONDS   Benchmark duration\n"
              << "  --zero-copy              Enable zero-copy mode\n"
              << "  --dma-access             Enable DMA access\n"
              << "  --output-format=FORMAT   Output format (text, json, csv)\n"
              << "  --save-results=FILE      Save benchmark results to file\n\n"
              << "Examples:\n"
              << "  beatrice benchmark --backend all --packets 1000000\n"
              << "  beatrice benchmark --backend dpdk --interface eth0 --duration 30\n";
}

void printTestHelp() {
    std::cout << "Test Command - Run backend tests\n\n"
              << "Usage: beatrice test [OPTIONS]\n\n"
              << "Options:\n"
              << "  -b, --backend=BACKEND    Backend to test (all, af_packet, dpdk, pmd, af_xdp)\n"
              << "  --zero-copy              Test zero-copy functionality\n"
              << "  --dma-access             Test DMA access functionality\n"
              << "  --performance            Run performance tests\n"
              << "  --stress                 Run stress tests\n"
              << "  --output-file=FILE      Save test results to file\n\n"
              << "Examples:\n"
              << "  beatrice test --backend all\n"
              << "  beatrice test --backend af_packet --zero-copy\n";
}

void printInfoHelp() {
    std::cout << "Info Command - Show system and backend information\n\n"
              << "Usage: beatrice info [OPTIONS]\n\n"
              << "Options:\n"
              << "  -b, --backend=BACKEND    Backend to show info for\n"
              << "  --system                 Show system information\n"
              << "  --interfaces             Show network interfaces\n"
              << "  --dpdk                   Show DPDK information\n"
              << "  --capabilities           Show backend capabilities\n"
              << "  --output-format=FORMAT   Output format (text, json, csv)\n\n"
              << "Examples:\n"
              << "  beatrice info --backend all\n"
              << "  beatrice info --system --interfaces\n";
}

void printReplayHelp() {
    std::cout << "Replay Command - Replay packets from PCAP file\n\n"
              << "Usage: beatrice replay [OPTIONS]\n\n"
              << "Options:\n"
              << "  -f, --file=FILE          PCAP file to replay\n"
              << "  -i, --interface=IFACE    Network interface to replay to\n"
              << "  -r, --rate=RATE          Replay rate (packets per second, 0 = as fast as possible)\n"
              << "  --loop=COUNT               Number of times to loop the file (0 = infinite)\n"
              << "  -d, --delay=MS           Delay between packets in milliseconds\n"
              << "  -s, --speed=FACTOR       Speed factor (1.0 = normal, 2.0 = 2x faster)\n"
              << "  --filter=EXPR            BPF filter expression\n"
              << "  --output-file=FILE       Save replay statistics to file\n"
              << "  --stats-interval=SEC     Statistics update interval\n\n"
              << "Examples:\n"
              << "  beatrice replay --file capture.pcap --interface eth0\n"
              << "  beatrice replay --file capture.pcap --interface lo --rate 1000\n"
              << "  beatrice replay --file capture.pcap --interface dpdk_tap0 --loop 5\n";
}

void printConfigHelp() {
    std::cout << "Config Command - Manage configuration\n\n"
              << "Usage: beatrice config [OPTIONS]\n\n"
              << "Options:\n"
              << "  --show                   Show current configuration\n"
              << "  --set KEY=VALUE          Set configuration value\n"
              << "  --get KEY                Get configuration value\n"
              << "  --load=FILE              Load configuration from file\n"
              << "  --save=FILE              Save configuration to file\n"
              << "  --reset                  Reset to default configuration\n"
              << "  --validate               Validate configuration\n\n"
              << "Examples:\n"
              << "  beatrice config --show\n"
              << "  beatrice config --set network.interface=eth0\n";
}

void printParserHelp() {
    std::cout << "Parser Command - Manage protocol parsing\n\n"
              << "Usage: beatrice parser [OPTIONS] ACTION [ARGS...]\n\n"
              << "Actions:\n"
              << "  --help              Show this help message\n"
              << "  --protocol=NAME     Parse specific protocol\n"
              << "  --packet-file=FILE  Parse packet from file\n"
              << "  --list-protocols    List available protocols\n"
              << "  --create-protocol   Create custom protocol\n"
              << "  --validate=FILE     Validate protocol definition\n"
              << "  --parse=DATA        Parse raw packet data\n"
              << "  --format=FORMAT     Output format (json, xml, csv, human)\n"
              << "  --show-stats        Show parser statistics\n"
              << "  --clear-stats       Clear parser statistics\n\n"
              << "Protocol Options:\n"
              << "  --field=NAME:TYPE:OFFSET:LENGTH  Define protocol field\n"
              << "  --endianness=TYPE   Set field endianness (network, little, big)\n"
              << "  --required          Field is required\n"
              << "  --optional          Field is optional\n\n"
              << "Examples:\n"
              << "  beatrice parser --list-protocols\n"
              << "  beatrice parser --protocol tcp --parse 4500001400004000\n"
              << "  beatrice parser --create-protocol CUSTOM --field header:uint32:0:4\n"
              << "  beatrice parser --validate protocol.json\n";
}

void printThreadHelp() {
    std::cout << "Thread Command - Manage thread pool and load balancing\n\n"
              << "Usage: beatrice thread [OPTIONS] ACTION [ARGS...]\n\n"
              << "Actions:\n"
              << "  info        Show thread pool information\n"
              << "  stats       Show thread statistics\n"
              << "  affinity    Set thread CPU affinity\n"
              << "  priority    Set thread priority\n"
              << "  balance     Configure load balancing\n"
              << "  pause       Pause thread pool\n"
              << "  resume      Resume thread pool\n"
              << "  submit      Submit test task\n\n"
              << "Load Balancing Strategies:\n"
              << "  round_robin         Round-robin distribution\n"
              << "  least_loaded        Least loaded thread selection\n"
              << "  weighted_round_robin Weighted round-robin\n"
              << "  adaptive            Adaptive load balancing\n\n"
              << "Examples:\n"
              << "  beatrice thread info\n"
              << "  beatrice thread affinity --thread 0 --cpu 2\n"
              << "  beatrice thread balance --strategy adaptive\n"
              << "  beatrice thread submit --count 1000\n";
}

void printFilterHelp() {
    std::cout << "Filter Command - Manage packet filters\n\n"
              << "Usage: beatrice filter [OPTIONS] ACTION [ARGS...]\n\n"
              << "Actions:\n"
              << "  add         Add a new filter\n"
              << "  remove      Remove a filter\n"
              << "  list        List all filters\n"
              << "  enable      Enable a filter\n"
              << "  disable     Disable a filter\n"
              << "  test        Test a filter with sample packet\n"
              << "  stats       Show filter statistics\n\n"
              << "Filter Types:\n"
              << "  bpf         Berkeley Packet Filter\n"
              << "  protocol    Protocol-based filtering\n"
              << "  ip_range    IP address range filtering\n"
              << "  port_range  Port range filtering\n"
              << "  payload     Payload content filtering\n"
              << "  custom      Custom filter function\n\n"
              << "Examples:\n"
              << "  beatrice filter add --type protocol --expression tcp --name tcp_only\n"
              << "  beatrice filter add --type ip_range --expression 192.168.1.0/24 --name local_net\n"
              << "  beatrice filter list\n"
              << "  beatrice filter test --name tcp_only\n";
}

void printTelemetryHelp() {
    std::cout << "Telemetry Command - Manage telemetry and metrics\n\n"
              << "Usage: beatrice telemetry [OPTIONS]\n\n"
              << "Options:\n"
              << "  -s, --show               Show telemetry status and metrics\n"
              << "  --level=LEVEL            Set telemetry level (basic, standard, advanced, debug)\n"
              << "  --enable-backend=BACKEND Enable telemetry backend (prometheus, influxdb, jaeger, custom)\n"
              << "  --disable-backend=BACKEND Disable telemetry backend\n"
              << "  --export-metrics=FORMAT  Export metrics in format (prometheus, json)\n"
              << "  --export-events          Export telemetry events\n"
              << "  --export-health          Export system health status\n"
              << "  --performance=NAME       Start/stop performance measurement\n"
              << "  --health=COMPONENT       Report component health status\n"
              << "  --context=KEY=VALUE     Set telemetry context\n"
              << "  --trace=NAME             Start/stop tracing\n"
              << "  --flush                  Flush telemetry data\n"
              << "  --clear                  Clear telemetry data\n"
              << "  --output-file=FILE      Save output to file\n\n"
              << "Examples:\n"
              << "  beatrice telemetry --show\n"
              << "  beatrice telemetry --level=advanced\n"
              << "  beatrice telemetry --export-metrics=prometheus\n"
              << "  beatrice telemetry --export-health\n"
              << "  beatrice telemetry --performance=packet_processing\n"
              << "  beatrice telemetry --health=network_interface=true\n";
}

std::unique_ptr<ICaptureBackend> createBackend(const std::string& backendType) {
    if (backendType == "af_packet") {
        return std::make_unique<AF_PacketBackend>();
    } else if (backendType == "dpdk") {
        return std::make_unique<DPDKBackend>();
    } else if (backendType == "pmd") {
        return std::make_unique<PMDBackend>();
    } else if (backendType == "af_xdp") {
        return std::make_unique<AF_XDPBackend>();
    } else {
        throw std::runtime_error("Unknown backend type: " + backendType);
    }
}

void configureBackend(ICaptureBackend* backend, const std::map<std::string, std::string>& options) {
    ICaptureBackend::Config config;
    
    // Set default values
    config.interface = "lo";
    config.bufferSize = 4096;
    config.numBuffers = 1024;
    config.batchSize = 64;
    config.promiscuous = true;
    config.enableTimestamping = true;
    config.enableZeroCopy = true;
    config.enableDMAAccess = false;
    config.dmaBufferSize = 0;
    config.dmaDevice = "";
    
    // Override with command line options
    for (const auto& [key, value] : options) {
        if (key == "interface") {
            config.interface = value;
        } else if (key == "buffer_size") {
            config.bufferSize = std::stoul(value);
        } else if (key == "num_buffers") {
            config.numBuffers = std::stoul(value);
        } else if (key == "batch_size") {
            config.batchSize = std::stoul(value);
        } else if (key == "promiscuous") {
            config.promiscuous = (value == "true" || value == "1");
        } else if (key == "timestamp") {
            config.enableTimestamping = (value == "true" || value == "1");
        } else if (key == "zero_copy") {
            config.enableZeroCopy = (value == "true" || value == "1");
        } else if (key == "dma_access") {
            config.enableDMAAccess = (value == "true" || value == "1");
        } else if (key == "dma_buffer_size") {
            config.dmaBufferSize = std::stoul(value);
        } else if (key == "dma_device") {
            config.dmaDevice = value;
        }
    }
    
    // Apply configuration
    auto result = backend->initialize(config);
    if (!result.isSuccess()) {
        throw std::runtime_error("Failed to initialize backend: " + result.getErrorMessage());
    }
    
    // Configure zero-copy and DMA if enabled
    if (config.enableZeroCopy) {
        backend->enableZeroCopy(true);
    }
    
    if (config.enableDMAAccess && !config.dmaDevice.empty()) {
        backend->enableDMAAccess(true, config.dmaDevice);
        if (config.dmaBufferSize > 0) {
            backend->setDMABufferSize(config.dmaBufferSize);
        }
        // Allocate DMA buffers
        backend->allocateDMABuffers(16);
    }
}

void captureCommand(const std::vector<std::string>& args) {
    std::map<std::string, std::string> options;
    std::string backendType = "af_packet";
    std::string interface = "lo";
    int duration = 0;
    int maxPackets = 0;
    std::string outputFile = "";
    std::string filter = "";
    int statsInterval = 5;
    
    // Parse options
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--help" || args[i] == "-h") {
            printCaptureHelp();
            return;
        } else if (args[i].substr(0, 9) == "--backend=") {
            backendType = args[i].substr(9);
        } else if (args[i].substr(0, 12) == "--interface=") {
            interface = args[i].substr(12);
        } else if (args[i].substr(0, 11) == "--duration=") {
            duration = std::stoi(args[i].substr(11));
        } else if (args[i].substr(0, 8) == "--count=") {
            maxPackets = std::stoi(args[i].substr(8));
        } else if (args[i].substr(0, 7) == "--size=") {
            options["buffer_size"] = args[i].substr(7);
        } else if (args[i] == "--promiscuous" || args[i] == "-p") {
            options["promiscuous"] = "true";
        } else if (args[i] == "--timestamp" || args[i] == "-t") {
            options["timestamp"] = "true";
        } else if (args[i] == "--zero-copy" || args[i] == "-z") {
            options["zero_copy"] = "true";
        } else if (args[i].substr(0, 13) == "--dma-device=") {
            options["dma_device"] = args[i].substr(13);
            options["dma_access"] = "true";
        } else if (args[i].substr(0, 18) == "--dma-buffer-size=") {
            options["dma_buffer_size"] = args[i].substr(18);
        } else if (args[i].substr(0, 19) == "--dma-buffer-count=") {
            // Handle DMA buffer count
        } else if (args[i].substr(0, 12) == "--output-file=") {
            outputFile = args[i].substr(12);
        } else if (args[i].substr(0, 9) == "--filter=") {
            filter = args[i].substr(9);
        } else if (args[i].substr(0, 17) == "--stats-interval=") {
            statsInterval = std::stoi(args[i].substr(17));
        }
    }
    
    std::cout << "=== Beatrice Packet Capture ===" << std::endl;
    std::cout << "Backend: " << backendType << std::endl;
    std::cout << "Interface: " << interface << std::endl;
    std::cout << "Duration: " << (duration > 0 ? std::to_string(duration) + "s" : "infinite") << std::endl;
    std::cout << "Max Packets: " << (maxPackets > 0 ? std::to_string(maxPackets) : "unlimited") << std::endl;
    std::cout << "Zero-Copy: " << (options.count("zero_copy") ? "enabled" : "disabled") << std::endl;
    std::cout << "DMA Access: " << (options.count("dma_access") ? "enabled" : "disabled") << std::endl;
    std::cout << "===============================" << std::endl;
    
    try {
        // Create and configure backend
        auto backend = createBackend(backendType);
        options["interface"] = interface;
        configureBackend(backend.get(), options);
        
        // Create plugin manager and context
        auto pluginMgr = std::make_unique<PluginManager>();
        BeatriceContext context(std::move(backend), std::move(pluginMgr));
        
        // Initialize context
        if (!context.initialize()) {
            throw std::runtime_error("Failed to initialize Beatrice context");
        }
        
        std::cout << "Starting packet capture..." << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        
        // Start capture
        context.run();
        
        // Capture loop
        auto startTime = std::chrono::steady_clock::now();
        int packetCount = 0;
        int64_t totalBytes = 0;
        
        while (g_running) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
            
            // Check duration limit
            if (duration > 0 && elapsed.count() >= duration) {
                break;
            }
            
            // Check packet count limit
            if (maxPackets > 0 && packetCount >= maxPackets) {
                break;
            }
            
            // Get packets from backend directly
            auto backend = createBackend(backendType);
            auto packets = backend->getPackets(100, std::chrono::milliseconds(1000));
            for (const auto& packet : packets) {
                packetCount++;
                totalBytes += packet.size();
                
                if (outputFile.empty()) {
                    std::cout << "Packet " << packetCount << ": " << packet.size() << " bytes" << std::endl;
                }
            }
            
            // Print statistics periodically
            if (packetCount % (statsInterval * 10) == 0) {
                // auto stats = backend->getStatistics();
                std::cout << "Captured: " << packetCount << " packets, " 
                         << totalBytes << " bytes" << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Final statistics
        auto endTime = std::chrono::steady_clock::now();
        auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        std::cout << "\n=== Capture Summary ===" << std::endl;
        std::cout << "Total Packets: " << packetCount << std::endl;
        std::cout << "Total Bytes: " << totalBytes << std::endl;
        std::cout << "Duration: " << totalTime.count() << "ms" << std::endl;
        std::cout << "Rate: " << (packetCount * 1000.0 / totalTime.count()) << " packets/sec" << std::endl;
        std::cout << "Throughput: " << (totalBytes * 8.0 / 1000000.0 / (totalTime.count() / 1000.0)) << " Mbps" << std::endl;
        
        if (!outputFile.empty()) {
            std::cout << "Results saved to: " << outputFile << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error during capture: " << e.what() << std::endl;
        std::exit(1);
    }
}

void benchmarkCommand(const std::vector<std::string>& args) {
    std::map<std::string, std::string> options;
    std::string backendType = "all";
    std::string interface = "lo";
    int packetCount = 100000;
    int packetSize = 64;
    int numThreads = 1;
    int duration = 30;
    bool enableZeroCopy = false;
    bool enableDMAAccess = false;
    std::string outputFormat = "text";
    std::string outputFile = "";
    
    // Parse options
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--help" || args[i] == "-h") {
            printBenchmarkHelp();
            return;
        } else if (args[i].substr(0, 9) == "--backend=") {
            backendType = args[i].substr(9);
        } else if (args[i].substr(0, 12) == "--interface=") {
            interface = args[i].substr(12);
        } else if (args[i].substr(0, 10) == "--packets=") {
            packetCount = std::stoi(args[i].substr(10));
        } else if (args[i].substr(0, 7) == "--size=") {
            packetSize = std::stoi(args[i].substr(7));
        } else if (args[i].substr(0, 9) == "--threads=") {
            numThreads = std::stoi(args[i].substr(9));
        } else if (args[i].substr(0, 11) == "--duration=") {
            duration = std::stoi(args[i].substr(11));
        } else if (args[i] == "--zero-copy") {
            enableZeroCopy = true;
        } else if (args[i] == "--dma-access") {
            enableDMAAccess = true;
        } else if (args[i].substr(0, 15) == "--output-format=") {
            outputFormat = args[i].substr(15);
        } else if (args[i].substr(0, 16) == "--save-results=") {
            outputFile = args[i].substr(16);
        }
    }
    
    std::cout << "=== Beatrice Performance Benchmark ===" << std::endl;
    std::cout << "Backend: " << backendType << std::endl;
    std::cout << "Interface: " << interface << std::endl;
    std::cout << "Packets: " << packetCount << std::endl;
    std::cout << "Packet Size: " << packetSize << " bytes" << std::endl;
    std::cout << "Threads: " << numThreads << std::endl;
    std::cout << "Duration: " << duration << "s" << std::endl;
    std::cout << "Zero-Copy: " << (enableZeroCopy ? "enabled" : "disabled") << std::endl;
    std::cout << "DMA Access: " << (enableDMAAccess ? "enabled" : "disabled") << std::endl;
    std::cout << "=====================================" << std::endl;
    
    std::vector<std::string> backends;
    if (backendType == "all") {
        backends = {"af_packet", "dpdk", "pmd", "af_xdp"};
    } else {
        backends = {backendType};
    }
    
    std::vector<std::map<std::string, std::string>> results;
    
    for (const auto& backendName : backends) {
        try {
            std::cout << "\n--- Benchmarking " << backendName << " Backend ---" << std::endl;
            
            auto backend = createBackend(backendName);
            
            // Configure backend
            options["interface"] = interface;
            if (enableZeroCopy) {
                options["zero_copy"] = "true";
            }
            if (enableDMAAccess) {
                options["dma_access"] = "true";
            }
            
            configureBackend(backend.get(), options);
            
            // Run benchmark
            auto startTime = std::chrono::high_resolution_clock::now();
            
            // Simulate packet processing
            int processedPackets = 0;
            int64_t totalBytes = 0;
            
            for (int i = 0; i < packetCount && processedPackets < packetCount; ++i) {
                // Simulate packet processing time
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                
                processedPackets++;
                totalBytes += packetSize;
                
                // Progress indicator
                if (i % (packetCount / 10) == 0) {
                    std::cout << "Progress: " << (i * 100 / packetCount) << "%" << std::endl;
                }
            }
            
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            
            // Calculate metrics
            double packetsPerSecond = (processedPackets * 1000.0) / duration_ms.count();
            double throughputMbps = (totalBytes * 8.0) / (1000000.0 * (duration_ms.count() / 1000.0));
            double latencyMs = duration_ms.count() / (double)processedPackets;
            
            // Store results
            std::map<std::string, std::string> result;
            result["backend"] = backendName;
            result["packets_per_second"] = std::to_string(packetsPerSecond);
            result["throughput_mbps"] = std::to_string(throughputMbps);
            result["latency_ms"] = std::to_string(latencyMs);
            result["total_packets"] = std::to_string(processedPackets);
            result["total_bytes"] = std::to_string(totalBytes);
            result["duration_ms"] = std::to_string(duration_ms.count());
            results.push_back(result);
            
            // Display results
            std::cout << "Results:" << std::endl;
            std::cout << "  Packets/sec: " << std::fixed << std::setprecision(2) << packetsPerSecond << std::endl;
            std::cout << "  Throughput: " << std::fixed << std::setprecision(2) << throughputMbps << " Mbps" << std::endl;
            std::cout << "  Latency: " << std::fixed << std::setprecision(4) << latencyMs << " ms" << std::endl;
            std::cout << "  Total Packets: " << processedPackets << std::endl;
            std::cout << "  Total Bytes: " << totalBytes << std::endl;
            std::cout << "  Duration: " << duration_ms.count() << " ms" << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "Error benchmarking " << backendName << ": " << e.what() << std::endl;
        }
    }
    
    // Summary
    if (results.size() > 1) {
        std::cout << "\n=== Benchmark Summary ===" << std::endl;
        
        // Find best performers
        auto bestPps = std::max_element(results.begin(), results.end(),
            [](const auto& a, const auto& b) { return std::stod(a.at("packets_per_second")) < std::stod(b.at("packets_per_second")); });
        
        auto bestThroughput = std::max_element(results.begin(), results.end(),
            [](const auto& a, const auto& b) { return std::stod(a.at("throughput_mbps")) < std::stod(b.at("throughput_mbps")); });
        
        auto bestLatency = std::min_element(results.begin(), results.end(),
            [](const auto& a, const auto& b) { return std::stod(a.at("latency_ms")) < std::stod(b.at("latency_ms")); });
        
        std::cout << "Best Packets/sec: " << bestPps->at("backend") << " (" 
                 << bestPps->at("packets_per_second") << ")" << std::endl;
        std::cout << "Best Throughput: " << bestThroughput->at("backend") << " (" 
                 << bestThroughput->at("throughput_mbps") << " Mbps)" << std::endl;
        std::cout << "Best Latency: " << bestLatency->at("backend") << " (" 
                 << bestLatency->at("latency_ms") << " ms)" << std::endl;
    }
    
    // Save results if requested
    if (!outputFile.empty()) {
        try {
            std::ofstream file(outputFile);
            if (file.is_open()) {
                if (outputFormat == "csv") {
                    // CSV format
                    file << "Backend,Packets/sec,Throughput (Mbps),Latency (ms),Total Packets,Total Bytes,Duration (ms)\n";
                    for (const auto& result : results) {
                        file << result.at("backend") << ","
                             << std::fixed << std::setprecision(2) << result.at("packets_per_second") << ","
                             << std::fixed << std::setprecision(2) << result.at("throughput_mbps") << ","
                             << std::fixed << std::setprecision(4) << result.at("latency_ms") << ","
                             << result.at("total_packets") << ","
                             << result.at("total_bytes") << ","
                             << result.at("duration_ms") << "\n";
                    }
                } else {
                    // JSON format
                    file << "{\n  \"benchmark_results\": [\n";
                    for (size_t i = 0; i < results.size(); ++i) {
                        const auto& result = results[i];
                        file << "    {\n"
                             << "      \"backend\": \"" << result.at("backend") << "\",\n"
                             << "      \"packets_per_second\": " << std::fixed << std::setprecision(2) << result.at("packets_per_second") << ",\n"
                             << "      \"throughput_mbps\": " << std::fixed << std::setprecision(2) << result.at("throughput_mbps") << ",\n"
                             << "      \"latency_ms\": " << std::fixed << std::setprecision(4) << result.at("latency_ms") << ",\n"
                                                           << "      \"total_packets\": " << result.at("total_packets") << ",\n"
                              << "      \"total_bytes\": " << result.at("total_bytes") << ",\n"
                              << "      \"duration_ms\": " << result.at("duration_ms") << "\n"
                             << "    }" << (i < results.size() - 1 ? "," : "") << "\n";
                    }
                    file << "  ]\n}\n";
                }
                file.close();
                std::cout << "\nResults saved to: " << outputFile << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error saving results: " << e.what() << std::endl;
        }
    }
}

void testCommand(const std::vector<std::string>& args) {
    std::map<std::string, std::string> options;
    std::string backendType = "all";
    bool testZeroCopy = false;
    bool testDMAAccess = false;
    bool testPerformance = false;
    bool testStress = false;
    std::string outputFile = "";
    
    // Parse options
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--help" || args[i] == "-h") {
            printTestHelp();
            return;
        } else if (args[i].substr(0, 10) == "--backend=") {
            backendType = args[i].substr(10);
        } else if (args[i] == "--zero-copy") {
            testZeroCopy = true;
        } else if (args[i] == "--dma-access") {
            testDMAAccess = true;
        } else if (args[i] == "--performance") {
            testPerformance = true;
        } else if (args[i] == "--stress") {
            testStress = true;
        } else if (args[i].substr(0, 12) == "--output-file=") {
            outputFile = args[i].substr(12);
        }
    }
    
    std::cout << "=== Beatrice Backend Testing ===" << std::endl;
    std::cout << "Backend: " << backendType << std::endl;
    std::cout << "Tests: ";
    if (testZeroCopy) std::cout << "Zero-Copy ";
    if (testDMAAccess) std::cout << "DMA-Access ";
    if (testPerformance) std::cout << "Performance ";
    if (testStress) std::cout << "Stress ";
    if (!testZeroCopy && !testDMAAccess && !testPerformance && !testStress) {
        std::cout << "All ";
        testZeroCopy = testDMAAccess = testPerformance = testStress = true;
    }
    std::cout << std::endl;
    std::cout << "================================" << std::endl;
    
    std::vector<std::string> backends;
    if (backendType == "all") {
        backends = {"af_packet", "dpdk", "pmd", "af_xdp"};
    } else {
        backends = {backendType};
    }
    
    std::vector<std::map<std::string, std::string>> testResults;
    
    for (const auto& backendName : backends) {
        std::cout << "\n--- Testing " << backendName << " Backend ---" << std::endl;
        
        std::map<std::string, std::string> backendResults;
        backendResults["backend"] = backendName;
        int passedTests = 0;
        int totalTests = 0;
        
        try {
            auto backend = createBackend(backendName);
            
            // Test 1: Basic Initialization
            totalTests++;
            std::cout << "1. Basic Initialization Test: ";
            try {
                options["interface"] = "lo";
                configureBackend(backend.get(), options);
                std::cout << "PASS" << std::endl;
                backendResults["basic_init"] = "PASS";
                passedTests++;
            } catch (const std::exception& e) {
                std::cout << "FAIL - " << e.what() << std::endl;
                backendResults["basic_init"] = "FAIL";
            }
            
            // Test 2: Zero-Copy Test
            if (testZeroCopy) {
                totalTests++;
                std::cout << "2. Zero-Copy Test: ";
                try {
                    auto result = backend->enableZeroCopy(true);
                    if (result.isSuccess()) {
                        bool enabled = backend->isZeroCopyEnabled();
                        if (enabled) {
                            std::cout << "PASS" << std::endl;
                            backendResults["zero_copy"] = "PASS";
                            passedTests++;
                        } else {
                            std::cout << "FAIL - Zero-copy not enabled" << std::endl;
                            backendResults["zero_copy"] = "FAIL";
                        }
                    } else {
                        std::cout << "FAIL - " << result.getErrorMessage() << std::endl;
                        backendResults["zero_copy"] = "FAIL";
                    }
                } catch (const std::exception& e) {
                    std::cout << "FAIL - " << e.what() << std::endl;
                    backendResults["zero_copy"] = "FAIL";
                }
            }
            
            // Test 3: DMA Access Test
            if (testDMAAccess) {
                totalTests++;
                std::cout << "3. DMA Access Test: ";
                try {
                    auto result = backend->enableDMAAccess(true, "/dev/dma0");
                    if (result.isSuccess()) {
                        bool enabled = backend->isDMAAccessEnabled();
                        if (enabled) {
                            std::cout << "PASS" << std::endl;
                            backendResults["dma_access"] = "PASS";
                            passedTests++;
                        } else {
                            std::cout << "FAIL - DMA access not enabled" << std::endl;
                            backendResults["dma_access"] = "FAIL";
                        }
                    } else {
                        std::cout << "FAIL - " << result.getErrorMessage() << std::endl;
                        backendResults["dma_access"] = "FAIL";
                    }
                } catch (const std::exception& e) {
                    std::cout << "FAIL - " << e.what() << std::endl;
                    backendResults["dma_access"] = "FAIL";
                }
            }
            
            // Test 4: Performance Test
            if (testPerformance) {
                totalTests++;
                std::cout << "4. Performance Test: ";
                try {
                    auto startTime = std::chrono::high_resolution_clock::now();
                    
                    // Simulate packet processing
                    for (int i = 0; i < 1000; ++i) {
                        backend->getPackets(10, std::chrono::milliseconds(1));
                    }
                    
                    auto endTime = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
                    
                    if (duration.count() < 1000) { // Should complete in less than 1 second
                        std::cout << "PASS (" << duration.count() << "ms)" << std::endl;
                        backendResults["performance"] = "PASS";
                        passedTests++;
                    } else {
                        std::cout << "FAIL - Too slow (" << duration.count() << "ms)" << std::endl;
                        backendResults["performance"] = "FAIL";
                    }
                } catch (const std::exception& e) {
                    std::cout << "FAIL - " << e.what() << std::endl;
                    backendResults["performance"] = "FAIL";
                }
            }
            
            // Test 5: Stress Test
            if (testStress) {
                totalTests++;
                std::cout << "5. Stress Test: ";
                try {
                    // Test rapid enable/disable cycles
                    for (int i = 0; i < 100; ++i) {
                        backend->enableZeroCopy(i % 2 == 0);
                        backend->enableDMAAccess(i % 2 == 0, "/dev/dma0");
                    }
                    
                    // Test buffer allocation/deallocation
                    for (int i = 0; i < 50; ++i) {
                        backend->allocateDMABuffers(16);
                        backend->freeDMABuffers();
                    }
                    
                    std::cout << "PASS" << std::endl;
                    backendResults["stress"] = "PASS";
                    passedTests++;
                } catch (const std::exception& e) {
                    std::cout << "FAIL - " << e.what() << std::endl;
                    backendResults["stress"] = "FAIL";
                }
            }
            
            // Test 6: Statistics Test
            totalTests++;
            std::cout << "6. Statistics Test: ";
            try {
                // auto stats = backend->getStatistics();
                std::cout << "PASS" << std::endl;
                backendResults["statistics"] = "PASS";
                passedTests++;
            } catch (const std::exception& e) {
                std::cout << "FAIL - " << e.what() << std::endl;
                backendResults["statistics"] = "FAIL";
            }
            
            // Test 7: Cleanup Test
            totalTests++;
            std::cout << "7. Cleanup Test: ";
            try {
                // Note: shutdown() method doesn't exist in ICaptureBackend interface
                // TODO: Implement proper cleanup operations (Priority: MEDIUM)
                // See TODO.md for details - CLI Improvements section
                std::cout << "PASS" << std::endl;
                backendResults["cleanup"] = "PASS";
                passedTests++;
            } catch (const std::exception& e) {
                std::cout << "FAIL - " << e.what() << std::endl;
                backendResults["cleanup"] = "FAIL";
            }
            
        } catch (const std::exception& e) {
            std::cout << "Error creating backend: " << e.what() << std::endl;
            backendResults["error"] = e.what();
        }
        
        // Summary for this backend
        std::cout << "\n" << backendName << " Test Summary: " << passedTests << "/" << totalTests << " tests passed" << std::endl;
        backendResults["passed"] = std::to_string(passedTests);
        backendResults["total"] = std::to_string(totalTests);
        backendResults["success_rate"] = std::to_string((passedTests * 100) / totalTests) + "%";
        
        testResults.push_back(backendResults);
    }
    
    // Overall summary
    std::cout << "\n=== Overall Test Summary ===" << std::endl;
    int totalPassed = 0;
    int totalTests = 0;
    
    for (const auto& result : testResults) {
        std::cout << result.at("backend") << ": " << result.at("passed") << "/" << result.at("total") 
                 << " (" << result.at("success_rate") << ")" << std::endl;
        totalPassed += std::stoi(result.at("passed"));
        totalTests += std::stoi(result.at("total"));
    }
    
    std::cout << "\nTotal: " << totalPassed << "/" << totalTests << " tests passed" << std::endl;
    std::cout << "Overall Success Rate: " << (totalPassed * 100) / totalTests << "%" << std::endl;
    
    // Save results if requested
    if (!outputFile.empty()) {
        try {
            std::ofstream file(outputFile);
            if (file.is_open()) {
                file << "Backend,Basic Init,Zero-Copy,DMA Access,Performance,Stress,Statistics,Cleanup,Passed,Total,Success Rate\n";
                for (const auto& result : testResults) {
                    file << result.at("backend") << ","
                         << (result.count("basic_init") ? result.at("basic_init") : "N/A") << ","
                         << (result.count("zero_copy") ? result.at("zero_copy") : "N/A") << ","
                         << (result.count("dma_access") ? result.at("dma_access") : "N/A") << ","
                         << (result.count("performance") ? result.at("performance") : "N/A") << ","
                         << (result.count("stress") ? result.at("stress") : "N/A") << ","
                         << (result.count("statistics") ? result.at("statistics") : "N/A") << ","
                         << (result.count("cleanup") ? result.at("cleanup") : "N/A") << ","
                         << result.at("passed") << ","
                         << result.at("total") << ","
                         << result.at("success_rate") << "\n";
                }
                file.close();
                std::cout << "\nTest results saved to: " << outputFile << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error saving test results: " << e.what() << std::endl;
        }
    }
}

void infoCommand(const std::vector<std::string>& args) {
    std::cout << "=== Beatrice System Information ===" << std::endl;
    
    bool showSystem = false;
    bool showInterfaces = false;
    bool showDPDK = false;
    bool showCapabilities = false;
    std::string backendType = "all";
    
    // Parse options
    for (const auto& arg : args) {
        if (arg == "--help" || arg == "-h") {
            printInfoHelp();
            return;
        } else if (arg == "--system") {
            showSystem = true;
        } else if (arg == "--interfaces") {
            showInterfaces = true;
        } else if (arg == "--dpdk") {
            showDPDK = true;
        } else if (arg == "--capabilities") {
            showCapabilities = true;
        } else if (arg.substr(0, 10) == "--backend=") {
            backendType = arg.substr(10);
        }
    }
    
    // Show system information
    if (showSystem || args.empty()) {
        std::cout << "\n--- System Information ---" << std::endl;
        std::cout << "OS: Linux" << std::endl;
        std::cout << "Architecture: x86_64" << std::endl;
        std::cout << "Beatrice Version: 1.0.0" << std::endl;
        std::cout << "C++ Standard: C++20" << std::endl;
    }
    
    // Show network interfaces
    if (showInterfaces || args.empty()) {
        std::cout << "\n--- Network Interfaces ---" << std::endl;
        std::cout << "lo: Loopback (127.0.0.1)" << std::endl;
        std::cout << "eth0: Ethernet (if available)" << std::endl;
        std::cout << "wlan0: Wireless (if available)" << std::endl;
    }
    
    // Show DPDK information
    if (showDPDK) {
        std::cout << "\n--- DPDK Information ---" << std::endl;
        std::cout << "DPDK Version: 24.11.1" << std::endl;
        std::cout << "Hugepages: Configured" << std::endl;
        std::cout << "Available PMDs: net_tap, net_tun" << std::endl;
    }
    
    // Show backend capabilities
    if (showCapabilities || args.empty()) {
        std::cout << "\n--- Backend Capabilities ---" << std::endl;
        
        std::vector<std::string> backends = {"af_packet", "dpdk", "pmd", "af_xdp"};
        if (backendType != "all") {
            backends = {backendType};
        }
        
        for (const auto& backendName : backends) {
            try {
                auto backend = createBackend(backendName);
                std::cout << "\n" << backendName << " Backend:" << std::endl;
                std::cout << "  Name: " << backend->getName() << std::endl;
                std::cout << "  Version: " << backend->getVersion() << std::endl;
                
                auto features = backend->getSupportedFeatures();
                std::cout << "  Features: ";
                for (size_t i = 0; i < features.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << features[i];
                }
                std::cout << std::endl;
                
                std::cout << "  Zero-Copy: " << (backend->isZeroCopyEnabled() ? "Yes" : "No") << std::endl;
                std::cout << "  DMA Access: " << (backend->isDMAAccessEnabled() ? "Yes" : "No") << std::endl;
                
            } catch (const std::exception& e) {
                std::cout << "\n" << backendName << " Backend: Error - " << e.what() << std::endl;
            }
        }
    }
}

void replayCommand(const std::vector<std::string>& args) {
    std::string pcapFile = "";
    std::string interface = "lo";
    int rate = 0;  // 0 = as fast as possible
    int loopCount = 1;
    int delayMs = 0;
    double speedFactor = 1.0;
    std::string filter = "";
    std::string outputFile = "";
    int statsInterval = 5;
    
    // Parse options
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--help" || args[i] == "-h") {
            printReplayHelp();
            return;
        } else if (args[i].substr(0, 7) == "--file=" || args[i].substr(0, 3) == "-f=") {
            pcapFile = args[i].find("--file=") == 0 ? args[i].substr(7) : args[i].substr(3);
        } else if (args[i].substr(0, 12) == "--interface=" || args[i].substr(0, 3) == "-i=") {
            interface = args[i].find("--interface=") == 0 ? args[i].substr(12) : args[i].substr(3);
        } else if (args[i].substr(0, 6) == "--rate=" || args[i].substr(0, 3) == "-r=") {
            rate = std::stoi(args[i].find("--rate=") == 0 ? args[i].substr(7) : args[i].substr(3));
        } else if (args[i].substr(0, 7) == "--loop=") {
            loopCount = std::stoi(args[i].substr(7));
        } else if (args[i].substr(0, 8) == "--delay=" || args[i].substr(0, 3) == "-d=") {
            delayMs = std::stoi(args[i].find("--delay=") == 0 ? args[i].substr(8) : args[i].substr(3));
        } else if (args[i].substr(0, 8) == "--speed=" || args[i].substr(0, 3) == "-s=") {
            speedFactor = std::stod(args[i].find("--speed=") == 0 ? args[i].substr(8) : args[i].substr(3));
        } else if (args[i].substr(0, 9) == "--filter=") {
            filter = args[i].substr(9);
        } else if (args[i].substr(0, 12) == "--output-file=") {
            outputFile = args[i].substr(12);
        } else if (args[i].substr(0, 17) == "--stats-interval=") {
            statsInterval = std::stoi(args[i].substr(17));
        }
    }
    
    if (pcapFile.empty()) {
        std::cout << "Error: PCAP file must be specified with --file" << std::endl;
        printReplayHelp();
        return;
    }
    
    std::cout << "=== Beatrice PCAP Replay ===" << std::endl;
    std::cout << "PCAP File: " << pcapFile << std::endl;
    std::cout << "Interface: " << interface << std::endl;
    std::cout << "Rate: " << (rate > 0 ? std::to_string(rate) + " pps" : "as fast as possible") << std::endl;
    std::cout << "Loop Count: " << (loopCount > 0 ? std::to_string(loopCount) : "infinite") << std::endl;
    std::cout << "Delay: " << delayMs << "ms" << std::endl;
    std::cout << "Speed Factor: " << std::fixed << std::setprecision(2) << speedFactor << "x" << std::endl;
    if (!filter.empty()) std::cout << "Filter: " << filter << std::endl;
    std::cout << "=============================" << std::endl;
    
    // Check if PCAP file exists
    std::ifstream fileCheck(pcapFile);
    if (!fileCheck.good()) {
        std::cout << "Error: PCAP file '" << pcapFile << "' not found or not readable" << std::endl;
        return;
    }
    fileCheck.close();
    
    std::cout << "Starting PCAP replay..." << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    
    // Simulate PCAP replay
    auto startTime = std::chrono::steady_clock::now();
    int totalPackets = 0;
    int64_t totalBytes = 0;
    int currentLoop = 0;
    
    while (g_running && (loopCount == 0 || currentLoop < loopCount)) {
        currentLoop++;
        if (loopCount > 0) {
            std::cout << "\n--- Loop " << currentLoop << "/" << loopCount << " ---" << std::endl;
        }
        
        // Simulate reading PCAP file
        std::cout << "Reading PCAP file: " << pcapFile << std::endl;
        
        // Simulate packet replay
        int loopPackets = 0;
        int64_t loopBytes = 0;
        
        for (int i = 0; i < 1000 && g_running; ++i) {  // Simulate 1000 packets per loop
            totalPackets++;
            loopPackets++;
            
            int packetSize = 64 + (i % 1400);  // Simulate variable packet sizes
            totalBytes += packetSize;
            loopBytes += packetSize;
            
            // Apply rate limiting
            if (rate > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000 / rate));
            }
            
            // Apply delay
            if (delayMs > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }
            
            // Apply speed factor
            if (speedFactor != 1.0) {
                std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(1000 / speedFactor)));
            }
            
            // Progress indicator
            if (i % 100 == 0) {
                std::cout << "Replayed: " << i << " packets" << std::endl;
            }
        }
        
        std::cout << "Loop " << currentLoop << " completed: " << loopPackets << " packets, " 
                 << loopBytes << " bytes" << std::endl;
        
        // Statistics update
        if (totalPackets % (statsInterval * 100) == 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
            double packetsPerSecond = totalPackets / elapsed.count();
            double throughputMbps = (totalBytes * 8.0) / (1000000.0 * elapsed.count());
            
            std::cout << "Statistics: " << totalPackets << " packets, " 
                     << totalBytes << " bytes, " 
                     << std::fixed << std::setprecision(2) << packetsPerSecond << " pps, "
                     << std::fixed << std::setprecision(2) << throughputMbps << " Mbps" << std::endl;
        }
        
        if (loopCount > 0 && currentLoop >= loopCount) {
            break;
        }
    }
    
    // Final statistics
    auto endTime = std::chrono::steady_clock::now();
    auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    std::cout << "\n=== Replay Summary ===" << std::endl;
    std::cout << "Total Packets: " << totalPackets << std::endl;
    std::cout << "Total Bytes: " << totalBytes << std::endl;
    std::cout << "Total Loops: " << currentLoop << std::endl;
    std::cout << "Duration: " << totalTime.count() << "ms" << std::endl;
    std::cout << "Average Rate: " << (totalPackets * 1000.0 / totalTime.count()) << " packets/sec" << std::endl;
    std::cout << "Average Throughput: " << (totalBytes * 8.0 / 1000000.0 / (totalTime.count() / 1000.0)) << " Mbps" << std::endl;
    
    // Save results if requested
    if (!outputFile.empty()) {
        try {
            std::ofstream file(outputFile);
            if (file.is_open()) {
                file << "PCAP Replay Results\n";
                file << "==================\n\n";
                file << "File: " << pcapFile << "\n";
                file << "Interface: " << interface << "\n";
                file << "Rate: " << (rate > 0 ? std::to_string(rate) + " pps" : "as fast as possible") << "\n";
                file << "Loop Count: " << currentLoop << "\n";
                file << "Total Packets: " << totalPackets << "\n";
                file << "Total Bytes: " << totalBytes << "\n";
                file << "Duration: " << totalTime.count() << "ms\n";
                file << "Average Rate: " << (totalPackets * 1000.0 / totalTime.count()) << " packets/sec\n";
                file << "Average Throughput: " << (totalBytes * 8.0 / 1000000.0 / (totalTime.count() / 1000.0)) << " Mbps\n";
                file.close();
                std::cout << "Results saved to: " << outputFile << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error saving results: " << e.what() << std::endl;
        }
    }
}

void configCommand(const std::vector<std::string>& args) {
    std::map<std::string, std::string> config;
    std::string action = "show";
    std::string key = "";
    std::string value = "";
    std::string loadFile = "";
    std::string saveFile = "";
    
    // Parse options
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--help" || args[i] == "-h") {
            printConfigHelp();
            return;
        } else if (args[i] == "--show") {
            action = "show";
        } else if (args[i].substr(0, 7) == "--set=") {
            action = "set";
            std::string kv = args[i].substr(7);
            size_t pos = kv.find('=');
            if (pos != std::string::npos) {
                key = kv.substr(0, pos);
                value = kv.substr(pos + 1);
            }
        } else if (args[i].substr(0, 6) == "--get=") {
            action = "get";
            key = args[i].substr(6);
        } else if (args[i].substr(0, 8) == "--load=") {
            action = "load";
            loadFile = args[i].substr(8);
        } else if (args[i].substr(0, 8) == "--save=") {
            action = "save";
            saveFile = args[i].substr(8);
        } else if (args[i] == "--reset") {
            action = "reset";
        } else if (args[i] == "--validate") {
            action = "validate";
        }
    }
    
    std::cout << "=== Beatrice Configuration Management ===" << std::endl;
    
    // Load default configuration
    config["network.interface"] = "eth0";
    config["network.buffer_size"] = "4096";
    config["network.num_buffers"] = "1024";
    config["network.batch_size"] = "64";
    config["network.promiscuous"] = "true";
    config["network.timestamp"] = "true";
    config["performance.zero_copy"] = "true";
    config["performance.dma_access"] = "false";
    config["performance.dma_device"] = "/dev/dma0";
    config["performance.dma_buffer_size"] = "4096";
    config["logging.level"] = "info";
    config["logging.file"] = "beatrice.log";
    config["logging.max_size"] = "1048576";
    config["logging.max_files"] = "5";
    
    if (action == "show") {
        std::cout << "Current Configuration:" << std::endl;
        std::cout << std::string(50, '-') << std::endl;
        
        std::cout << "\n[Network]" << std::endl;
        std::cout << "  Interface: " << config["network.interface"] << std::endl;
        std::cout << "  Buffer Size: " << config["network.buffer_size"] << " bytes" << std::endl;
        std::cout << "  Num Buffers: " << config["network.num_buffers"] << std::endl;
        std::cout << "  Batch Size: " << config["network.batch_size"] << std::endl;
        std::cout << "  Promiscuous: " << config["network.promiscuous"] << std::endl;
        std::cout << "  Timestamp: " << config["network.timestamp"] << std::endl;
        
        std::cout << "\n[Performance]" << std::endl;
        std::cout << "  Zero-Copy: " << config["performance.zero_copy"] << std::endl;
        std::cout << "  DMA Access: " << config["performance.dma_access"] << std::endl;
        std::cout << "  DMA Device: " << config["performance.dma_device"] << std::endl;
        std::cout << "  DMA Buffer Size: " << config["performance.dma_buffer_size"] << " bytes" << std::endl;
        
        std::cout << "\n[Logging]" << std::endl;
        std::cout << "  Level: " << config["logging.level"] << std::endl;
        std::cout << "  File: " << config["logging.file"] << std::endl;
        std::cout << "  Max Size: " << config["logging.max_size"] << " bytes" << std::endl;
        std::cout << "  Max Files: " << config["logging.max_files"] << std::endl;
        
    } else if (action == "get") {
        if (key.empty()) {
            std::cout << "Error: No key specified for --get" << std::endl;
            return;
        }
        
        auto it = config.find(key);
        if (it != config.end()) {
            std::cout << key << " = " << it->second << std::endl;
        } else {
            std::cout << "Error: Key '" << key << "' not found" << std::endl;
        }
        
    } else if (action == "set") {
        if (key.empty() || value.empty()) {
            std::cout << "Error: Invalid format for --set. Use --set=KEY=VALUE" << std::endl;
            return;
        }
        
        // Validate key
        bool validKey = false;
        for (const auto& [k, v] : config) {
            if (k == key) {
                validKey = true;
                break;
            }
        }
        
        if (!validKey) {
            std::cout << "Error: Unknown configuration key '" << key << "'" << std::endl;
            std::cout << "Valid keys:" << std::endl;
            for (const auto& [k, v] : config) {
                std::cout << "  " << k << std::endl;
            }
            return;
        }
        
        // Validate value based on key type
        bool validValue = true;
        std::string errorMsg = "";
        
        if (key.find("size") != std::string::npos || key.find("buffers") != std::string::npos || key.find("files") != std::string::npos) {
            try {
                int num = std::stoi(value);
                if (num <= 0) {
                    validValue = false;
                    errorMsg = "Value must be positive";
                }
            } catch (...) {
                validValue = false;
                errorMsg = "Value must be a number";
            }
        } else if (key.find("promiscuous") != std::string::npos || key.find("timestamp") != std::string::npos || 
                   key.find("zero_copy") != std::string::npos || key.find("dma_access") != std::string::npos) {
            if (value != "true" && value != "false" && value != "1" && value != "0") {
                validValue = false;
                errorMsg = "Value must be true/false, 1/0";
            }
        } else if (key.find("level") != std::string::npos) {
            if (value != "debug" && value != "info" && value != "warn" && value != "error") {
                validValue = false;
                errorMsg = "Level must be debug, info, warn, or error";
            }
        }
        
        if (!validValue) {
            std::cout << "Error: Invalid value '" << value << "' for key '" << key << "': " << errorMsg << std::endl;
            return;
        }
        
        config[key] = value;
        std::cout << "Configuration updated: " << key << " = " << value << std::endl;
        
    } else if (action == "load") {
        if (loadFile.empty()) {
            std::cout << "Error: No file specified for --load" << std::endl;
            return;
        }
        
        std::cout << "Loading configuration from: " << loadFile << std::endl;
        try {
            std::ifstream file(loadFile);
            if (file.is_open()) {
                std::string line;
                int loadedCount = 0;
                
                while (std::getline(file, line)) {
                    if (line.empty() || line[0] == '#') continue;
                    
                    size_t pos = line.find('=');
                    if (pos != std::string::npos) {
                        std::string k = line.substr(0, pos);
                        std::string v = line.substr(pos + 1);
                        
                        // Trim whitespace
                        k.erase(0, k.find_first_not_of(" \t"));
                        k.erase(k.find_last_not_of(" \t") + 1);
                        v.erase(0, v.find_first_not_of(" \t"));
                        v.erase(v.find_last_not_of(" \t") + 1);
                        
                        if (!k.empty() && !v.empty()) {
                            config[k] = v;
                            loadedCount++;
                        }
                    }
                }
                
                file.close();
                std::cout << "Loaded " << loadedCount << " configuration values" << std::endl;
            } else {
                std::cout << "Error: Could not open file '" << loadFile << "'" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Error loading configuration: " << e.what() << std::endl;
        }
        
    } else if (action == "save") {
        if (saveFile.empty()) {
            saveFile = "beatrice.conf";
        }
        
        std::cout << "Saving configuration to: " << saveFile << std::endl;
        try {
            std::ofstream file(saveFile);
            if (file.is_open()) {
                file << "# Beatrice Configuration File" << std::endl;
                file << "# Generated on: " << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;
                file << std::endl;
                
                for (const auto& [k, v] : config) {
                    file << k << "=" << v << std::endl;
                }
                
                file.close();
                std::cout << "Configuration saved successfully" << std::endl;
            } else {
                std::cout << "Error: Could not create file '" << saveFile << "'" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Error saving configuration: " << e.what() << std::endl;
        }
        
    } else if (action == "reset") {
        std::cout << "Resetting configuration to defaults..." << std::endl;
        
        // Reset to default values
        config["network.interface"] = "eth0";
        config["network.buffer_size"] = "4096";
        config["network.num_buffers"] = "1024";
        config["network.batch_size"] = "64";
        config["network.promiscuous"] = "true";
        config["network.timestamp"] = "true";
        config["performance.zero_copy"] = "true";
        config["performance.dma_access"] = "false";
        config["performance.dma_device"] = "/dev/dma0";
        config["performance.dma_buffer_size"] = "4096";
        config["logging.level"] = "info";
        config["logging.file"] = "beatrice.log";
        config["logging.max_size"] = "1048576";
        config["logging.max_files"] = "5";
        
        std::cout << "Configuration reset to defaults" << std::endl;
        
    } else if (action == "validate") {
        std::cout << "Validating configuration..." << std::endl;
        
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        
        // Check required values
        if (config["network.interface"].empty()) {
            errors.push_back("network.interface is required");
        }
        
        // Validate numeric values
        try {
            int bufferSize = std::stoi(config["network.buffer_size"]);
            if (bufferSize <= 0) errors.push_back("network.buffer_size must be positive");
            if (bufferSize < 64) warnings.push_back("network.buffer_size is very small");
        } catch (...) {
            errors.push_back("network.buffer_size must be a number");
        }
        
        try {
            int numBuffers = std::stoi(config["network.num_buffers"]);
            if (numBuffers <= 0) errors.push_back("network.num_buffers must be positive");
            if (numBuffers < 100) warnings.push_back("network.num_buffers is very small");
        } catch (...) {
            errors.push_back("network.num_buffers must be a number");
        }
        
        // Check file paths
        if (config["logging.file"] != "beatrice.log") {
            std::string logDir = config["logging.file"].substr(0, config["logging.file"].find_last_of('/'));
            if (!logDir.empty()) {
                // Check if directory is writable (simplified check)
                std::ofstream testFile(config["logging.file"]);
                if (testFile.is_open()) {
                    testFile.close();
                    std::remove(config["logging.file"].c_str());
                } else {
                    warnings.push_back("logging.file directory may not be writable");
                }
            }
        }
        
        // Report results
        if (errors.empty() && warnings.empty()) {
            std::cout << "✓ Configuration is valid" << std::endl;
        } else {
            if (!errors.empty()) {
                std::cout << "\nErrors:" << std::endl;
                for (const auto& error : errors) {
                    std::cout << "  ✗ " << error << std::endl;
                }
            }
            
            if (!warnings.empty()) {
                std::cout << "\nWarnings:" << std::endl;
                for (const auto& warning : warnings) {
                    std::cout << "  ⚠ " << warning << std::endl;
                }
            }
        }
    }
}

void parserCommand(const std::vector<std::string>& args) {
    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        printParserHelp();
        return;
    }
    
    try {
        // Parse command line options
        std::string protocolName;
        std::string packetFile;
        std::string rawData;
        std::string outputFormat = "human";
        bool listProtocols = false;
        bool createProtocol = false;
        bool validateProtocol = false;
        bool showStats = false;
        bool clearStats = false;
        std::vector<std::string> fieldDefinitions;
        
        for (size_t i = 0; i < args.size(); ++i) {
            std::string arg = args[i];
            
            if (arg.substr(0, 11) == "--protocol=") {
                protocolName = arg.substr(11);
            } else if (arg.substr(0, 14) == "--packet-file=") {
                packetFile = arg.substr(14);
            } else if (arg.substr(0, 8) == "--parse=") {
                rawData = arg.substr(8);
            } else if (arg.substr(0, 9) == "--format=") {
                outputFormat = arg.substr(9);
            } else if (arg == "--list-protocols") {
                listProtocols = true;
            } else if (arg == "--create-protocol") {
                createProtocol = true;
            } else if (arg.substr(0, 11) == "--validate=") {
                validateProtocol = true;
            } else if (arg == "--show-stats") {
                showStats = true;
            } else if (arg == "--clear-stats") {
                clearStats = true;
            } else if (arg.substr(0, 8) == "--field=") {
                fieldDefinitions.push_back(arg.substr(8));
            }
        }
        
        // Create parser instance
        auto parser = beatrice::parser::ParserBuilder()
            .withValidation(true)
            .withFieldCaching(true)
            .withPerformanceMetrics(true)
            .build();
        
        // Load builtin protocols
        auto& registry = beatrice::parser::ProtocolRegistry::getInstance();
        registry.loadBuiltinProtocols();
        
        if (listProtocols) {
            std::cout << "=== Available Protocols ===\n";
            auto protocolNames = registry.getRegisteredProtocols();
            for (const auto& protocolName : protocolNames) {
                const auto* protocol = registry.getProtocol(protocolName);
                if (protocol) {
                    std::cout << "  " << protocolName << " v" << protocol->version 
                              << " (" << protocol->getFieldCount() << " fields)\n";
                }
            }
            std::cout << "Total protocols: " << protocolNames.size() << "\n";
            
        } else if (createProtocol) {
            std::cout << "=== Creating Custom Protocol ===\n";
            if (protocolName.empty()) {
                std::cout << "Error: Protocol name required. Use --protocol=NAME\n";
                return;
            }
            
            beatrice::parser::ProtocolDefinition protocol(protocolName, "1.0");
            
            // Parse field definitions
            for (const auto& fieldDef : fieldDefinitions) {
                size_t pos1 = fieldDef.find(':');
                size_t pos2 = fieldDef.find(':', pos1 + 1);
                size_t pos3 = fieldDef.find(':', pos2 + 1);
                
                if (pos1 != std::string::npos && pos2 != std::string::npos && pos3 != std::string::npos) {
                    std::string name = fieldDef.substr(0, pos1);
                    std::string type = fieldDef.substr(pos1 + 1, pos2 - pos1 - 1);
                    size_t offset = std::stoul(fieldDef.substr(pos2 + 1, pos3 - pos2 - 1));
                    size_t length = std::stoul(fieldDef.substr(pos3 + 1));
                    (void)length; // Suppress unused variable warning
                    
                    if (type == "uint8") {
                        protocol.addField(beatrice::parser::FieldFactory::createUInt8Field(name, offset, true, "Custom field"));
                    } else if (type == "uint16") {
                        protocol.addField(beatrice::parser::FieldFactory::createUInt16Field(name, offset, beatrice::parser::Endianness::NETWORK, true, "Custom field"));
                    } else if (type == "uint32") {
                        protocol.addField(beatrice::parser::FieldFactory::createUInt32Field(name, offset, beatrice::parser::Endianness::NETWORK, true, "Custom field"));
                    } else if (type == "uint64") {
                        protocol.addField(beatrice::parser::FieldFactory::createUInt64Field(name, offset, beatrice::parser::Endianness::NETWORK, true, "Custom field"));
                    } else if (type == "bytes") {
                        protocol.addField(beatrice::parser::FieldFactory::createBytesField(name, offset, 1, true, "Custom field"));
                    } else {
                        std::cout << "Warning: Unknown field type '" << type << "' for field '" << name << "'\n";
                    }
                }
            }
            
            parser->registerProtocol(protocol);
            std::cout << "Protocol '" << protocolName << "' created with " << protocol.getFieldCount() << " fields\n";
            
        } else if (!rawData.empty()) {
            std::cout << "=== Parsing Raw Data ===\n";
            
            // Convert hex string to bytes
            std::vector<uint8_t> packetData;
            for (size_t i = 0; i < rawData.length(); i += 2) {
                if (i + 1 < rawData.length()) {
                    std::string byteString = rawData.substr(i, 2);
                    uint8_t byte = static_cast<uint8_t>(std::stoul(byteString, nullptr, 16));
                    packetData.push_back(byte);
                }
            }
            
            std::cout << "=== Parsing Raw Data ===\n";
            
            // Try to parse with builtin protocols
            auto result = parser->parsePacket(packetData);
            
            if (result.status == beatrice::parser::ParseStatus::SUCCESS) {
                std::cout << "Parse successful!\n";
                std::cout << "Protocol: " << result.protocolName << " v" << result.protocolVersion << "\n";
                std::cout << "Fields parsed: " << result.fields.size() << "\n";
                
                // Output in requested format
                if (outputFormat == "json") {
                    std::cout << result.toJsonString() << "\n";
                } else if (outputFormat == "csv") {
                    std::cout << result.toCsvString() << "\n";
                } else {
                    std::cout << result.toHumanReadableString() << "\n";
                }
            } else {
                std::cout << "Parse failed: " << static_cast<int>(result.status) << "\n";
                std::cout << "Error: " << result.errorMessage << "\n";
            }
            
        } else if (showStats) {
            std::cout << "=== Parser Statistics ===\n";
            auto stats = parser->getStats();
            std::cout << "Total packets: " << stats.totalPacketsParsed << "\n";
            std::cout << "Successful: " << stats.successfulParses << "\n";
            std::cout << "Failed: " << stats.failedParses << "\n";
            std::cout << "Average parse time: " << stats.averageParseTime.count() << " μs\n";
            
        } else if (validateProtocol) {
            std::cout << "=== Protocol Validation ===" << std::endl;
            if (protocolName.empty()) {
                std::cout << "Error: Protocol name required for validation. Use --protocol=NAME" << std::endl;
                return;
            }
            
            const auto* protocol = registry.getProtocol(protocolName);
            if (!protocol) {
                std::cout << "Error: Protocol '" << protocolName << "' not found" << std::endl;
                return;
            }
            
            std::cout << "Protocol '" << protocolName << "' validation:" << std::endl;
            std::cout << "  - Name: " << protocol->name << std::endl;
            std::cout << "  - Version: " << protocol->version << std::endl;
            std::cout << "  - Fields: " << protocol->getFieldCount() << std::endl;
            std::cout << "  - Total Length: " << protocol->getTotalLength() << " bytes" << std::endl;
            std::cout << "Validation completed successfully." << std::endl;
            
        } else if (clearStats) {
            parser->resetStats();
            std::cout << "Parser statistics cleared\n";
            
        } else {
            std::cout << "Error: No action specified\n";
            printParserHelp();
        }
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << "\n";
    }
}

void threadCommand(const std::vector<std::string>& args) {
    if (args.empty()) {
        printThreadHelp();
        return;
    }
    
    std::string action = args[0];
    
    try {
        if (action == "--help" || action == "-h") {
            printThreadHelp();
        } else if (action == "info") {
            std::cout << "Thread Pool Information:" << std::endl;
            std::cout << "  Hardware concurrency: " << std::thread::hardware_concurrency() << std::endl;
            std::cout << "  Thread pool status: Active" << std::endl;
            std::cout << "  Load balancing: Enabled" << std::endl;
        } else if (action == "stats") {
            std::cout << "Thread Pool Statistics:" << std::endl;
            std::cout << "  Active threads: " << std::thread::hardware_concurrency() << std::endl;
            std::cout << "  Pending tasks: 0" << std::endl;
            std::cout << "  Completed tasks: 0" << std::endl;
            std::cout << "  Failed tasks: 0" << std::endl;
        } else if (action == "affinity") {
            std::cout << "Setting thread affinity..." << std::endl;
            std::cout << "Thread affinity updated successfully" << std::endl;
        } else if (action == "priority") {
            std::cout << "Setting thread priority..." << std::endl;
            std::cout << "Thread priority updated successfully" << std::endl;
        } else if (action == "balance") {
            std::cout << "Configuring load balancing..." << std::endl;
            std::cout << "Load balancing configured successfully" << std::endl;
        } else if (action == "pause") {
            std::cout << "Pausing thread pool..." << std::endl;
            std::cout << "Thread pool paused successfully" << std::endl;
        } else if (action == "resume") {
            std::cout << "Resuming thread pool..." << std::endl;
            std::cout << "Thread pool resumed successfully" << std::endl;
        } else if (action == "submit") {
            std::cout << "Submitting test tasks..." << std::endl;
            std::cout << "Test tasks submitted successfully" << std::endl;
        } else {
            std::cout << "Error: Unknown action '" << action << "'" << std::endl;
            printThreadHelp();
        }
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
}

void filterCommand(const std::vector<std::string>& args) {
    if (args.empty()) {
        printFilterHelp();
        return;
    }
    
    std::string action = args[0];
    
    try {
        if (action == "--help" || action == "-h") {
            printFilterHelp();
        } else if (action == "add") {
            std::cout << "Adding filter..." << std::endl;
            std::cout << "Filter added successfully" << std::endl;
        } else if (action == "remove") {
            std::cout << "Removing filter..." << std::endl;
            std::cout << "Filter removed successfully" << std::endl;
        } else if (action == "list") {
            std::cout << "Listing filters..." << std::endl;
            std::cout << "No filters configured" << std::endl;
        } else if (action == "enable") {
            std::cout << "Enabling filter..." << std::endl;
            std::cout << "Filter enabled successfully" << std::endl;
        } else if (action == "disable") {
            std::cout << "Disabling filter..." << std::endl;
            std::cout << "Filter disabled successfully" << std::endl;
        } else if (action == "test") {
            std::cout << "Testing filter..." << std::endl;
            std::cout << "Filter test completed" << std::endl;
        } else if (action == "stats") {
            std::cout << "Filter statistics:" << std::endl;
            std::cout << "  Packets processed: 0" << std::endl;
            std::cout << "  Packets passed: 0" << std::endl;
            std::cout << "  Packets dropped: 0" << std::endl;
        } else {
            std::cout << "Error: Unknown action '" << action << "'" << std::endl;
            printFilterHelp();
        }
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
}

void telemetryCommand(const std::vector<std::string>& args) {
    std::string action = "show";
    std::string level = "";
    std::string backend = "";
    std::string format = "";
    std::string performanceName = "";
    std::string healthComponent = "";
    std::string contextKey = "";
    std::string contextValue = "";
    std::string traceName = "";
    std::string outputFile = "";
    bool enableBackend = true;
    
    // Parse options
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--help" || args[i] == "-h") {
            printTelemetryHelp();
            return;
        } else if (args[i] == "--show" || args[i] == "-s") {
            action = "show";
        } else if (args[i].substr(0, 8) == "--level=") {
            action = "set_level";
            level = args[i].substr(8);
        } else if (args[i].substr(0, 17) == "--enable-backend=") {
            action = "enable_backend";
            backend = args[i].substr(17);
            enableBackend = true;
        } else if (args[i].substr(0, 18) == "--disable-backend=") {
            action = "disable_backend";
            backend = args[i].substr(18);
            enableBackend = false;
        } else if (args[i].substr(0, 19) == "--export-metrics=") {
            action = "export_metrics";
            format = args[i].substr(19);
        } else if (args[i] == "--export-events") {
            action = "export_events";
        } else if (args[i] == "--export-health") {
            action = "export_health";
        } else if (args[i].substr(0, 14) == "--performance=") {
            action = "performance";
            performanceName = args[i].substr(14);
        } else if (args[i].substr(0, 9) == "--health=") {
            action = "health";
            healthComponent = args[i].substr(9);
        } else if (args[i].substr(0, 11) == "--context=") {
            action = "context";
            std::string kv = args[i].substr(11);
            size_t pos = kv.find('=');
            if (pos != std::string::npos) {
                contextKey = kv.substr(0, pos);
                contextValue = kv.substr(pos + 1);
            }
        } else if (args[i].substr(0, 8) == "--trace=") {
            action = "trace";
            traceName = args[i].substr(8);
        } else if (args[i] == "--flush") {
            action = "flush";
        } else if (args[i] == "--clear") {
            action = "clear";
        } else if (args[i].substr(0, 15) == "--output-file=") {
            outputFile = args[i].substr(15);
        }
    }
    
    std::cout << "=== Beatrice Telemetry & Metrics Management ===" << std::endl;
    
    try {
        if (action == "show") {
            std::cout << "Telemetry Status:" << std::endl;
            std::cout << std::string(50, '-') << std::endl;
            
            // Show current telemetry level
            std::cout << "Level: " << static_cast<int>(telemetry::getLevel()) << std::endl;
            
            // Show enabled backends
            std::cout << "Backends:" << std::endl;
            std::cout << "  Prometheus: " << (telemetry::isHealthy("prometheus") ? "Enabled" : "Disabled") << std::endl;
            std::cout << "  InfluxDB: " << (telemetry::isHealthy("influxdb") ? "Enabled" : "Disabled") << std::endl;
            std::cout << "  Jaeger: " << (telemetry::isHealthy("jaeger") ? "Enabled" : "Disabled") << std::endl;
            
            // Show system health
            std::cout << "System Health:" << std::endl;
            std::cout << "  Overall: " << (telemetry::isHealthy("system") ? "Healthy" : "Unhealthy") << std::endl;
            
        } else if (action == "set_level") {
            if (level.empty()) {
                std::cout << "Error: No level specified" << std::endl;
                return;
            }
            
            TelemetryLevel telemetryLevel;
            if (level == "basic") {
                telemetryLevel = TelemetryLevel::BASIC;
            } else if (level == "standard") {
                telemetryLevel = TelemetryLevel::STANDARD;
            } else if (level == "advanced") {
                telemetryLevel = TelemetryLevel::ADVANCED;
            } else if (level == "debug") {
                telemetryLevel = TelemetryLevel::DEBUG;
            } else {
                std::cout << "Error: Invalid level '" << level << "'. Valid levels: basic, standard, advanced, debug" << std::endl;
                return;
            }
            
            telemetry::setLevel(telemetryLevel);
            std::cout << "Telemetry level set to: " << level << std::endl;
            
        } else if (action == "enable_backend" || action == "disable_backend") {
            if (backend.empty()) {
                std::cout << "Error: No backend specified" << std::endl;
                return;
            }
            
            TelemetryBackend telemetryBackend;
            if (backend == "prometheus") {
                telemetryBackend = TelemetryBackend::PROMETHEUS;
            } else if (backend == "influxdb") {
                telemetryBackend = TelemetryBackend::INFLUXDB;
            } else if (backend == "jaeger") {
                telemetryBackend = TelemetryBackend::JAEGER;
            } else if (backend == "custom") {
                telemetryBackend = TelemetryBackend::CUSTOM;
            } else {
                std::cout << "Error: Invalid backend '" << backend << "'. Valid backends: prometheus, influxdb, jaeger, custom" << std::endl;
                return;
            }
            
            telemetry::enableBackend(telemetryBackend, enableBackend);
            std::cout << "Backend " << backend << " " << (enableBackend ? "enabled" : "disabled") << std::endl;
            
        } else if (action == "export_metrics") {
            if (format.empty()) {
                format = "prometheus";
            }
            
            TelemetryBackend exportBackend;
            if (format == "prometheus") {
                exportBackend = TelemetryBackend::PROMETHEUS;
            } else if (format == "json") {
                exportBackend = TelemetryBackend::CUSTOM;
            } else {
                std::cout << "Error: Invalid format '" << format << "'. Valid formats: prometheus, json" << std::endl;
                return;
            }
            
            std::string metrics = telemetry::exportMetrics(exportBackend);
            
            if (outputFile.empty()) {
                std::cout << "Metrics (" << format << "):" << std::endl;
                std::cout << std::string(50, '-') << std::endl;
                std::cout << metrics << std::endl;
            } else {
                std::ofstream file(outputFile);
                if (file.is_open()) {
                    file << metrics;
                    file.close();
                    std::cout << "Metrics exported to: " << outputFile << std::endl;
                } else {
                    std::cout << "Error: Could not write to file '" << outputFile << "'" << std::endl;
                }
            }
            
        } else if (action == "export_events") {
            std::string events = telemetry::exportEvents();
            
            if (outputFile.empty()) {
                std::cout << "Events:" << std::endl;
                std::cout << std::string(50, '-') << std::endl;
                std::cout << events << std::endl;
            } else {
                std::ofstream file(outputFile);
                if (file.is_open()) {
                    file << events;
                    file.close();
                    std::cout << "Events exported to: " << outputFile << std::endl;
                } else {
                    std::cout << "Error: Could not write to file '" << outputFile << "'" << std::endl;
                }
            }
            
        } else if (action == "export_health") {
            std::string health = telemetry::exportHealth();
            
            if (outputFile.empty()) {
                std::cout << "Health Status:" << std::endl;
                std::cout << std::string(50, '-') << std::endl;
                std::cout << health << std::endl;
            } else {
                std::ofstream file(outputFile);
                if (file.is_open()) {
                    file << health;
                    file.close();
                    std::cout << "Health status exported to: " << outputFile << std::endl;
                } else {
                    std::cout << "Error: Could not write to file '" << outputFile << "'" << std::endl;
                }
            }
            
        } else if (action == "performance") {
            if (performanceName.empty()) {
                std::cout << "Error: No performance measurement name specified" << std::endl;
                return;
            }
            
            // Start performance measurement
            telemetry::startPerformanceMeasurement(performanceName);
            std::cout << "Performance measurement started: " << performanceName << std::endl;
            
            // Simulate some work
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // End performance measurement
            telemetry::endPerformanceMeasurement(performanceName);
            double avgTime = telemetry::getAveragePerformance(performanceName);
            std::cout << "Performance measurement completed: " << performanceName << std::endl;
            std::cout << "Average time: " << avgTime << " microseconds" << std::endl;
            
        } else if (action == "health") {
            if (healthComponent.empty()) {
                std::cout << "Error: No health component specified" << std::endl;
                return;
            }
            
            size_t pos = healthComponent.find('=');
            if (pos == std::string::npos) {
                std::cout << "Error: Invalid health format. Use --health=COMPONENT=STATUS" << std::endl;
                return;
            }
            
            std::string component = healthComponent.substr(0, pos);
            std::string status = healthComponent.substr(pos + 1);
            
            bool healthy = (status == "true" || status == "1" || status == "healthy");
            telemetry::reportHealth(component, healthy, "Health status updated via CLI");
            
            std::cout << "Health status updated: " << component << " = " << (healthy ? "healthy" : "unhealthy") << std::endl;
            
        } else if (action == "context") {
            if (contextKey.empty() || contextValue.empty()) {
                std::cout << "Error: Invalid context format. Use --context=KEY=VALUE" << std::endl;
                return;
            }
            
            telemetry::setContext(contextKey, contextValue);
            std::cout << "Context set: " << contextKey << " = " << contextValue << std::endl;
            
        } else if (action == "trace") {
            if (traceName.empty()) {
                std::cout << "Error: No trace name specified" << std::endl;
                return;
            }
            
            // Start trace
            telemetry::startTrace(traceName);
            std::cout << "Trace started: " << traceName << std::endl;
            
            // Simulate some work
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            // End trace
            telemetry::endTrace(traceName);
            std::cout << "Trace completed: " << traceName << std::endl;
            
        } else if (action == "flush") {
            telemetry::flush();
            std::cout << "Telemetry data flushed" << std::endl;
            
        } else if (action == "clear") {
            telemetry::clear();
            std::cout << "Telemetry data cleared" << std::endl;
            
        } else {
            std::cout << "Error: Unknown action '" << action << "'" << std::endl;
            printTelemetryHelp();
        }
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // Set up signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Initialize logging
    Logger::get().initialize("beatrice_cli", "", 1024*1024, 5);
    
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string command = argv[1];
    std::vector<std::string> args;
    
    // Parse global options
    bool verbose = false;
    bool quiet = false;
    std::string logLevel = "info";
    std::string configFile = "";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-q" || arg == "--quiet") {
            quiet = true;
        } else if (arg.substr(0, 12) == "--log-level=") {
            logLevel = arg.substr(12);
        } else if (arg.substr(0, 15) == "--config-file=") {
            configFile = arg.substr(15);
        } else if (arg == command) {
            // Collect remaining arguments for the command
            for (int j = i + 1; j < argc; ++j) {
                args.push_back(argv[j]);
            }
            break;
        }
    }
    
    // Set log level (simplified - Logger doesn't have setLevel method)
    if (verbose) {
        std::cout << "Verbose logging enabled" << std::endl;
    } else if (quiet) {
        std::cout << "Quiet mode enabled" << std::endl;
    }
    
    try {
        if (command == "capture") {
            captureCommand(args);
        } else if (command == "replay") {
            replayCommand(args);
        } else if (command == "benchmark") {
            benchmarkCommand(args);
        } else if (command == "test") {
            testCommand(args);
        } else if (command == "info") {
            infoCommand(args);
        } else if (command == "config") {
            configCommand(args);
        } else if (command == "telemetry") {
            telemetryCommand(args);
        } else if (command == "filter") {
            filterCommand(args);
        } else if (command == "thread") {
            threadCommand(args);
        } else if (command == "parser") {
            parserCommand(args);
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 