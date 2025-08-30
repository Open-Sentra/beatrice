#ifndef BEATRICE_CONFIG_HPP
#define BEATRICE_CONFIG_HPP

#include "Error.hpp"
#include <string>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>

namespace beatrice {

class Config {
public:
    struct Settings {
        struct Logging {
            std::string level = "info";
            std::string file = "";
            size_t maxFileSize = 10;
            size_t maxFiles = 5;
            bool console = true;
        } logging;
        
        struct Network {
            std::string interface = "";
            std::string backend = "af_xdp";
            size_t bufferSize = 4096;
            size_t numBuffers = 1024;
            bool promiscuous = true;
            int timeout = 1000;
        } network;
        
        struct Plugins {
            std::string directory = "./plugins";
            std::vector<std::string> enabled;
            bool autoLoad = false;
            size_t maxPlugins = 10;
        } plugins;
        
        struct Performance {
            size_t numThreads = 1;
            bool pinThreads = false;
            std::vector<int> cpuAffinity;
            size_t batchSize = 64;
            bool enableMetrics = true;
        } performance;
    };

    Config() = default;
    ~Config() = default;

    Result<void> initialize(const std::string& configFile = "", const std::string& defaultConfig = "");
    static Config& get();
    
    bool loadFromFile(const std::string& filename);
    bool loadFromEnvironment();
    
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    int getInt(const std::string& key, int defaultValue = 0) const;
    bool getBool(const std::string& key, bool defaultValue = false) const;
    double getDouble(const std::string& key, double defaultValue = 0.0) const;
    nlohmann::json getArray(const std::string& key) const;
    nlohmann::json getObject(const std::string& key) const;
    
    void set(const std::string& key, const nlohmann::json& value);
    bool has(const std::string& key) const;
    
    bool saveToFile(const std::string& filename) const;
    bool validate() const;
    std::string toJson() const;
    
    const Settings& getSettings() const { return settings_; }

private:
    static std::unique_ptr<Config> instance_;
    static std::mutex mutex_;
    
    Settings settings_;
    nlohmann::json config_;
    bool initialized_;
    
    std::vector<std::string> splitKey(const std::string& key) const;
    nlohmann::json getNestedValue(const std::vector<std::string>& keys) const;
    void setNestedValue(const std::vector<std::string>& keys, const nlohmann::json& value);
};

} // namespace beatrice

#endif // BEATRICE_CONFIG_HPP 