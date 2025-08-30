#include "beatrice/BeatriceContext.hpp"
#include "beatrice/AF_XDPBackend.hpp"
#include "beatrice/PluginManager.hpp"
#include "beatrice/Logger.hpp"
#include "beatrice/Config.hpp"
#include "beatrice/Error.hpp"
#include <memory>
#include <csignal>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <net/if.h>

namespace {
    volatile sig_atomic_t shutdownRequested = 0;
    
    void signalHandler(int signal) {
        shutdownRequested = 1;
    }
    
    void setupSignalHandlers() {
        struct sigaction sa;
        sa.sa_handler = signalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        
        if (sigaction(SIGINT, &sa, nullptr) == -1) {
            std::cerr << "Failed to set SIGINT handler" << std::endl;
        }
        
        if (sigaction(SIGTERM, &sa, nullptr) == -1) {
            std::cerr << "Failed to set SIGTERM handler" << std::endl;
        }
        
        if (sigaction(SIGHUP, &sa, nullptr) == -1) {
            std::cerr << "Failed to set SIGHUP handler" << std::endl;
        }
    }
    
    void printUsage(const char* programName) {
        std::cout << "Usage: " << programName << " [OPTIONS]\n"
                  << "Options:\n"
                  << "  -c, --config FILE     Configuration file path\n"
                  << "  -i, --interface IFACE Network interface name\n"
                  << "  -l, --log-level LEVEL Log level (trace, debug, info, warn, error, critical)\n"
                  << "  -p, --plugin-dir DIR  Plugin directory path\n"
                  << "  -t, --threads NUM     Number of processing threads\n"
                  << "  -b, --batch-size NUM  Packet batch size\n"
                  << "  -x, --xdp-program PATH XDP program path (BPF object file)\n"
                  << "  -m, --xdp-mode MODE   XDP mode: driver, skb, generic (default: driver)\n"
                  << "  -v, --version         Show version information\n"
                  << "  -h, --help            Show this help message\n"
                  << "\n"
                  << "Examples:\n"
                  << "  " << programName << " -i eth0 -l debug\n"
                  << "  " << programName << " -c /etc/beatrice/config.json\n"
                  << "  " << programName << " -i eth0 -t 4 -b 128\n"
                  << "  " << programName << " -i eth0 -x ./xdp_program.o\n";
    }
    
    void printVersion() {
        std::cout << "Beatrice Network Packet Capture SDK v1.0.0\n"
                  << "Copyright (c) 2024 Open Sentra\n"
                  << "Licensed under MIT License\n";
    }
    
    struct CommandLineArgs {
        std::string configFile;
        std::string interface;
        std::string logLevel = "info";
        std::string pluginDir;
        std::string xdpProgramPath;  // New: XDP program path
        std::string xdpMode = "driver"; // New: XDP mode (driver/skb/generic)
        size_t numThreads = 1;
        size_t batchSize = 64;
        bool showHelp = false;
        bool showVersion = false;
    };
    
    CommandLineArgs parseCommandLine(int argc, char* argv[]) {
        CommandLineArgs args;
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            if (arg == "-h" || arg == "--help") {
                args.showHelp = true;
            } else if (arg == "-v" || arg == "--version") {
                args.showVersion = true;
            } else if (arg == "-c" || arg == "--config") {
                if (++i < argc) args.configFile = argv[i];
            } else if (arg == "-i" || arg == "--interface") {
                if (++i < argc) args.interface = argv[i];
            } else if (arg == "-l" || arg == "--log-level") {
                if (++i < argc) args.logLevel = argv[i];
            } else if (arg == "-p" || arg == "--plugin-dir") {
                if (++i < argc) args.pluginDir = argv[i];
            } else if (arg == "-t" || arg == "--threads") {
                if (++i < argc) args.numThreads = std::stoul(argv[i]);
            } else if (arg == "-b" || arg == "--batch-size") {
                if (++i < argc) args.batchSize = std::stoul(argv[i]);
            } else if (arg == "-x" || arg == "--xdp-program") {
                if (++i < argc) args.xdpProgramPath = argv[i];
            } else if (arg == "-m" || arg == "--xdp-mode") {
                if (++i < argc) args.xdpMode = argv[i];
            } else {
                std::cerr << "Unknown option: " << arg << std::endl;
                args.showHelp = true;
            }
        }
        
        return args;
    }
    
    bool validateArgs(const CommandLineArgs& args) {
        if (args.numThreads == 0) {
            std::cerr << "Error: Number of threads must be greater than 0" << std::endl;
            return false;
        }
        
        if (args.batchSize == 0) {
            std::cerr << "Error: Batch size must be greater than 0" << std::endl;
            return false;
        }
        
        if (!args.interface.empty() && args.interface.length() > IFNAMSIZ) {
            std::cerr << "Error: Interface name too long: " << args.interface << std::endl;
            return false;
        }
        
        if (args.xdpMode != "driver" && args.xdpMode != "skb" && args.xdpMode != "generic") {
            std::cerr << "Error: Invalid XDP mode: " << args.xdpMode << " (use: driver, skb, generic)" << std::endl;
            return false;
        }
        
        return true;
    }
    
    void createDefaultConfig(const std::string& configFile) {
        try {
            std::filesystem::path configPath(configFile);
            std::filesystem::create_directories(configPath.parent_path());
            
            std::ofstream config(configFile);
            if (config.is_open()) {
                config << "{\n"
                       << "  \"logging\": {\n"
                       << "    \"level\": \"info\",\n"
                       << "    \"file\": \"/var/log/beatrice.log\",\n"
                       << "    \"maxFileSize\": 10,\n"
                       << "    \"maxFiles\": 5,\n"
                       << "    \"console\": true\n"
                       << "  },\n"
                       << "  \"network\": {\n"
                       << "    \"interface\": \"eth0\",\n"
                       << "    \"backend\": \"af_xdp\",\n"
                       << "    \"bufferSize\": 4096,\n"
                       << "    \"numBuffers\": 1024,\n"
                       << "    \"promiscuous\": true,\n"
                       << "    \"timeout\": 1000,\n"
                       << "    \"batchSize\": 64\n"
                       << "  },\n"
                       << "  \"plugins\": {\n"
                       << "    \"directory\": \"./plugins\",\n"
                       << "    \"enabled\": [],\n"
                       << "    \"autoLoad\": false,\n"
                       << "    \"maxPlugins\": 10\n"
                       << "  },\n"
                       << "  \"performance\": {\n"
                       << "    \"numThreads\": 1,\n"
                       << "    \"pinThreads\": false,\n"
                       << "    \"cpuAffinity\": [],\n"
                       << "    \"batchSize\": 64,\n"
                       << "    \"enableMetrics\": true\n"
                       << "  }\n"
                       << "}\n";
                config.close();
                
                std::cout << "Created default configuration file: " << configFile << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to create default config: " << e.what() << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        // Parse command line arguments
        auto args = parseCommandLine(argc, argv);
        
        if (args.showHelp) {
            printUsage(argv[0]);
            return 0;
        }
        
        if (args.showVersion) {
            printVersion();
            return 0;
        }
        
        if (!validateArgs(args)) {
            return 1;
        }
        
        // Setup signal handlers
        setupSignalHandlers();
        
        // Initialize logging
        if (!beatrice::Logger::get().initialize("", args.logLevel).isSuccess()) {
            std::cerr << "Failed to initialize logger" << std::endl;
            return 1;
        }
        
        // Initialize configuration
        if (!beatrice::Config::get().initialize(args.configFile).isSuccess()) {
            BEATRICE_ERROR("Failed to initialize configuration from file: {}", args.configFile);
            return 1;
        }
        
        // Initialize configuration with default config
        std::string defaultConfig = "/etc/beatrice/config.json";
        if (!beatrice::Config::get().initialize(defaultConfig).isSuccess()) {
            BEATRICE_ERROR("Failed to initialize configuration with defaults");
            return 1;
        }
        
        auto& config = beatrice::Config::get();
        
        // Override configuration with command line arguments
        if (!args.interface.empty()) {
            config.set("network.interface", args.interface);
        }
        
        if (!args.pluginDir.empty()) {
            config.set("plugins.directory", args.pluginDir);
        }
        
        config.set("performance.numThreads", static_cast<int>(args.numThreads));
        config.set("network.batchSize", static_cast<int>(args.batchSize));
        config.set("logging.level", args.logLevel);
        
        BEATRICE_INFO("Configuration loaded successfully");
        BEATRICE_DEBUG("Interface: {}", config.getString("network.interface"));
        BEATRICE_DEBUG("Backend: {}", config.getString("network.backend"));
        BEATRICE_DEBUG("Threads: {}", config.getInt("performance.numThreads"));
        BEATRICE_DEBUG("Batch size: {}", config.getInt("network.batchSize"));
        
        // Create backend
        BEATRICE_INFO("Creating AF_XDP backend");
        auto backend = std::make_unique<beatrice::AF_XDPBackend>();
        auto* backendPtr = backend.get();  // Keep a pointer before moving
        
        // Create plugin manager
        BEATRICE_INFO("Creating plugin manager");
        auto pluginMgr = std::make_unique<beatrice::PluginManager>();
        
        // Set plugin manager configuration
        pluginMgr->setMaxPlugins(config.getInt("plugins.maxPlugins", 10));
        
        // Create context
        BEATRICE_INFO("Creating Beatrice context");
        beatrice::BeatriceContext context(std::move(backend), std::move(pluginMgr));
        
        // Initialize context
        BEATRICE_INFO("Initializing Beatrice context");
        if (!context.initialize()) {
            BEATRICE_ERROR("Failed to initialize Beatrice context");
            return 1;
        }
        
        BEATRICE_INFO("Beatrice context initialized successfully");
        
        // Load XDP program if specified (AFTER context initialization)
        if (!args.xdpProgramPath.empty()) {
            BEATRICE_INFO("Loading XDP program: {} in {} mode", args.xdpProgramPath, args.xdpMode);
            if (!backendPtr->loadXdpProgram(args.xdpProgramPath, "beatrice_xdp", args.xdpMode).isSuccess()) {
                BEATRICE_ERROR("Failed to load XDP program: {}", args.xdpProgramPath);
                return 1;
            }
            BEATRICE_INFO("XDP program loaded successfully");
        }
        
        // Run the context
        BEATRICE_INFO("Starting packet capture");
        context.run();
        
        BEATRICE_INFO("Beatrice shutdown complete");
        return 0;
        
    } catch (const beatrice::BeatriceException& e) {
        std::cerr << "Beatrice error: " << e.what() << " (code: " << static_cast<int>(e.getCode()) << ")" << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
}
