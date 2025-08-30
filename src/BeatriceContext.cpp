#include "beatrice/BeatriceContext.hpp"
#include "beatrice/Logger.hpp"
#include "beatrice/Error.hpp"
#include "beatrice/Config.hpp"
#include "beatrice/Metrics.hpp"
#include <csignal>
#include <thread>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <cerrno>
#include <pthread.h>
#include <sched.h>

namespace beatrice {

volatile sig_atomic_t BeatriceContext::running_ = 1;

BeatriceContext::BeatriceContext(std::unique_ptr<ICaptureBackend> backend, 
                                 std::unique_ptr<PluginManager> pluginMgr)
    : backend_(std::move(backend)), pluginMgr_(std::move(pluginMgr)) {
    BEATRICE_DEBUG("BeatriceContext created");
}

BeatriceContext::~BeatriceContext() {
    BEATRICE_DEBUG("BeatriceContext destroying");
    shutdown();
}

bool BeatriceContext::initialize() {
    try {
        BEATRICE_INFO("Initializing Beatrice context");
        
        if (!backend_) {
            BEATRICE_ERROR("Backend is null");
            return false;
        }
        
        if (!pluginMgr_) {
            BEATRICE_ERROR("Plugin manager is null");
            return false;
        }
        
        // Initialize configuration
        if (!Config::get().initialize().isSuccess()) {
            BEATRICE_ERROR("Failed to initialize configuration");
            return false;
        }
        
        // Initialize metrics
        auto& metrics = MetricsRegistry::get();
        packetsProcessed_ = metrics.createCounter("packets_processed", "Total packets processed");
        packetsDropped_ = metrics.createCounter("packets_dropped", "Total packets dropped");
        processingLatency_ = metrics.createHistogram("processing_latency", "Packet processing latency");
        
        // Initialize backend
        ICaptureBackend::Config backendConfig;
        auto& config = Config::get();
        
        backendConfig.interface = config.getString("network.interface", "eth0");
        backendConfig.bufferSize = config.getInt("network.bufferSize", 4096);
        backendConfig.numBuffers = config.getInt("network.numBuffers", 1024);
        backendConfig.promiscuous = config.getBool("network.promiscuous", true);
        backendConfig.timeout = config.getInt("network.timeout", 1000);
        backendConfig.batchSize = config.getInt("network.batchSize", 64);
        backendConfig.enableTimestamping = config.getBool("network.enableTimestamping", true);
        backendConfig.enableZeroCopy = config.getBool("network.enableZeroCopy", true);
        
        auto result = backend_->initialize(backendConfig);
        if (result.isError()) {
            BEATRICE_ERROR("Failed to initialize backend: {}", result.getErrorMessage());
            return false;
        }
        
        // Load plugins if auto-load is enabled
        if (config.getBool("plugins.autoLoad", false)) {
            std::string pluginDir = config.getString("plugins.directory", "./plugins");
            loadPluginsFromDirectory(pluginDir);
        }
        
        // Load specific enabled plugins
        auto enabledPlugins = config.getArray("plugins.enabled");
        for (const auto& pluginName : enabledPlugins) {
            if (pluginName.is_string()) {
                std::string pluginPath = config.getString("plugins.directory", "./plugins") + "/" + 
                                       pluginName.get<std::string>() + ".so";
                if (!pluginMgr_->loadPlugin(pluginPath)) {
                    BEATRICE_WARN("Failed to load enabled plugin: {}", pluginName.get<std::string>());
                }
            }
        }
        
        // Set up signal handlers
        setupSignalHandlers();
        
        BEATRICE_INFO("Beatrice context initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        BEATRICE_ERROR("Exception during initialization: {}", e.what());
        return false;
    }
}

void BeatriceContext::run() {
    try {
        BEATRICE_INFO("Starting Beatrice context");
        
        if (!backend_->isRunning()) {
            auto result = backend_->start();
            if (result.isError()) {
                BEATRICE_ERROR("Failed to start backend: {}", result.getErrorMessage());
                return;
            }
        }
        
        // Get performance configuration
        auto& config = Config::get();
        size_t numThreads = config.getInt("performance.numThreads", 1);
        size_t batchSize = config.getInt("performance.batchSize", 64);
        bool pinThreads = config.getBool("performance.pinThreads", false);
        auto cpuAffinity = config.getArray("performance.cpuAffinity");
        
        if (numThreads > 1) {
            runMultiThreaded(numThreads, batchSize, pinThreads, cpuAffinity);
        } else {
            runSingleThreaded(batchSize);
        }
        
    } catch (const std::exception& e) {
        BEATRICE_ERROR("Exception during execution: {}", e.what());
    }
}

void BeatriceContext::shutdown() {
    BEATRICE_INFO("Shutting down Beatrice context");
    
    running_ = 0;
    
    try {
        if (backend_ && backend_->isRunning()) {
            auto result = backend_->stop();
            if (result.isError()) {
                BEATRICE_ERROR("Error stopping backend: {}", result.getErrorMessage());
            }
        }
        
        // Plugin manager will clean up plugins in destructor
        pluginMgr_.reset();
        backend_.reset();
        
        BEATRICE_INFO("Beatrice context shutdown complete");
        
    } catch (const std::exception& e) {
        BEATRICE_ERROR("Exception during shutdown: {}", e.what());
    }
}

void BeatriceContext::setupSignalHandlers() {
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  // Restart interrupted system calls
    
    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        BEATRICE_ERROR("Failed to set SIGINT handler: {}", strerror(errno));
    }
    
    if (sigaction(SIGTERM, &sa, nullptr) == -1) {
        BEATRICE_ERROR("Failed to set SIGTERM handler: {}", strerror(errno));
    }
    
    if (sigaction(SIGHUP, &sa, nullptr) == -1) {
        BEATRICE_ERROR("Failed to set SIGHUP handler: {}", strerror(errno));
    }
    
    BEATRICE_DEBUG("Signal handlers configured");
}

void BeatriceContext::runSingleThreaded(size_t batchSize) {
    BEATRICE_INFO("Running in single-threaded mode with batch size {}", batchSize);
    
    while (running_) {
        try {
            auto startTime = std::chrono::steady_clock::now();
            
            // Process packets in batches
            auto packets = backend_->getPackets(batchSize, std::chrono::milliseconds(100));
            
            if (!packets.empty()) {
                for (auto& packet : packets) {
                    if (running_) {
                        processPacket(packet);
                    }
                }
                
                // Update metrics
                packetsProcessed_->increment(packets.size());
                
                auto endTime = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
                processingLatency_->observe(duration.count());
            }
            
            // Small sleep to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            
        } catch (const std::exception& e) {
            BEATRICE_ERROR("Exception in packet processing loop: {}", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void BeatriceContext::runMultiThreaded(size_t numThreads, size_t batchSize, 
                                      bool pinThreads, const nlohmann::json& cpuAffinity) {
    BEATRICE_INFO("Running in multi-threaded mode with {} threads, batch size {}", numThreads, batchSize);
    
    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    
    for (size_t i = 0; i < numThreads && running_; ++i) {
        threads.emplace_back([this, i, batchSize, pinThreads, &cpuAffinity]() {
            // Set thread name for debugging
            std::string threadName = "beatrice-worker-" + std::to_string(i);
            pthread_setname_np(pthread_self(), threadName.c_str());
            
            // Set CPU affinity if requested
            if (pinThreads && !cpuAffinity.empty() && i < cpuAffinity.size()) {
                if (cpuAffinity[i].is_number()) {
                    int cpu = cpuAffinity[i].get<int>();
                    cpu_set_t cpuset;
                    CPU_ZERO(&cpuset);
                    CPU_SET(cpu, &cpuset);
                    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
                    BEATRICE_DEBUG("Thread {} pinned to CPU {}", i, cpu);
                }
            }
            
            // Worker thread loop
            while (running_) {
                try {
                    auto startTime = std::chrono::steady_clock::now();
                    
                    auto packets = backend_->getPackets(batchSize, std::chrono::milliseconds(100));
                    
                    if (!packets.empty()) {
                        for (auto& packet : packets) {
                            if (running_) {
                                processPacket(packet);
                            }
                        }
                        
                        // Update metrics (thread-safe)
                        packetsProcessed_->increment(packets.size());
                        
                        auto endTime = std::chrono::steady_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
                        processingLatency_->observe(duration.count());
                    }
                    
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    
                } catch (const std::exception& e) {
                    BEATRICE_ERROR("Exception in worker thread {}: {}", i, e.what());
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void BeatriceContext::processPacket(Packet& packet) {
    try {
        if (packet.empty()) {
            packetsDropped_->increment();
            return;
        }
        
        // Process packet through plugins
        pluginMgr_->processPacket(packet);
        
    } catch (const std::exception& e) {
        BEATRICE_ERROR("Exception processing packet: {}", e.what());
        packetsDropped_->increment();
    }
}

void BeatriceContext::loadPluginsFromDirectory(const std::string& directory) {
    try {
        BEATRICE_INFO("Loading plugins from directory: {}", directory);
        
        if (!std::filesystem::exists(directory)) {
            BEATRICE_WARN("Plugin directory does not exist: {}", directory);
            return;
        }
        
        size_t loadedCount = 0;
        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".so") {
                std::string pluginPath = entry.path().string();
                if (pluginMgr_->loadPlugin(pluginPath)) {
                    loadedCount++;
                }
            }
        }
        
        BEATRICE_INFO("Loaded {} plugins from directory {}", loadedCount, directory);
        
    } catch (const std::exception& e) {
        BEATRICE_ERROR("Exception loading plugins from directory {}: {}", directory, e.what());
    }
}

void BeatriceContext::signalHandler(int signal) {
    BEATRICE_INFO("Received signal {}, shutting down", signal);
    running_ = 0;
}

} // namespace beatrice
