#ifndef BEATRICE_BEATRICE_CONTEXT_HPP
#define BEATRICE_BEATRICE_CONTEXT_HPP

#include "beatrice/ICaptureBackend.hpp"
#include "beatrice/PluginManager.hpp"
#include "beatrice/Logger.hpp"
#include "beatrice/Config.hpp"
#include "beatrice/Metrics.hpp"
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <csignal>
#include <nlohmann/json.hpp>

namespace beatrice {

class BeatriceContext {
public:
    BeatriceContext(std::unique_ptr<ICaptureBackend> backend, std::unique_ptr<PluginManager> pluginMgr);
    ~BeatriceContext();
    
    // Lifecycle management
    bool initialize();
    void run();
    void shutdown();
    
    // Configuration
    void setConfigFile(const std::string& configFile);
    void setLogLevel(const std::string& level);
    
    // Plugin management
    bool loadPlugin(const std::string& path);
    bool unloadPlugin(const std::string& name);
    std::vector<std::string> getLoadedPlugins() const;
    
    // Statistics and monitoring
    uint64_t getProcessedPacketCount() const;
    uint64_t getDroppedPacketCount() const;
    double getAverageProcessingLatency() const;
    std::string getMetricsJson() const;
    
    // Control
    void pause();
    void resume();
    bool isRunning() const;
    bool isPaused() const;

private:
    std::unique_ptr<ICaptureBackend> backend_;
    std::unique_ptr<PluginManager> pluginMgr_;
    
    // Metrics
    std::shared_ptr<Counter> packetsProcessed_;
    std::shared_ptr<Counter> packetsDropped_;
    std::shared_ptr<Histogram> processingLatency_;
    
    // State
    static volatile sig_atomic_t running_;
    bool paused_{false};
    std::string configFile_;
    
    // Signal handling
    static void signalHandler(int signal);
    void setupSignalHandlers();
    
    // Execution modes
    void runSingleThreaded(size_t batchSize);
    void runMultiThreaded(size_t numThreads, size_t batchSize, bool pinThreads, const nlohmann::json& cpuAffinity);
    
    // Packet processing
    void processPacket(Packet& packet);
    void loadPluginsFromDirectory(const std::string& directory);
    
    // Disable copying
    BeatriceContext(const BeatriceContext&) = delete;
    BeatriceContext& operator=(const BeatriceContext&) = delete;
};

} // namespace beatrice

#endif // BEATRICE_BEATRICE_CONTEXT_HPP
