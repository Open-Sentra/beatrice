#include "beatrice/Config.hpp"
#include "beatrice/Logger.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>

int main() {
    try {
        // Initialize logging
        beatrice::Logger::initialize("config_example", "", beatrice::LogLevel::INFO);
        
        BEATRICE_INFO("Starting configuration example");
        
        // Create a sample configuration file
        std::string configFile = "./example_config.json";
        std::ofstream config(configFile);
        if (config.is_open()) {
            config << "{\n"
                   << "  \"logging\": {\n"
                   << "    \"level\": \"debug\",\n"
                   << "    \"file\": \"./example.log\",\n"
                   << "    \"maxFileSize\": 5,\n"
                   << "    \"maxFiles\": 3,\n"
                   << "    \"console\": true\n"
                   << "  },\n"
                   << "  \"network\": {\n"
                   << "    \"interface\": \"eth0\",\n"
                   << "    \"backend\": \"af_xdp\",\n"
                   << "    \"bufferSize\": 2048,\n"
                   << "    \"numBuffers\": 512,\n"
                   << "    \"promiscuous\": true,\n"
                   << "    \"timeout\": 500,\n"
                   << "    \"batchSize\": 32\n"
                   << "  },\n"
                   << "  \"plugins\": {\n"
                   << "    \"directory\": \"./plugins\",\n"
                   << "    \"enabled\": [\"tcp_analyzer\", \"http_parser\"],\n"
                   << "    \"autoLoad\": true,\n"
                   << "    \"maxPlugins\": 5\n"
                   << "  },\n"
                   << "  \"performance\": {\n"
                   << "    \"numThreads\": 2,\n"
                   << "    \"pinThreads\": false,\n"
                   << "    \"cpuAffinity\": [0, 1],\n"
                   << "    \"batchSize\": 32,\n"
                   << "    \"enableMetrics\": true\n"
                   << "  },\n"
                   << "  \"custom\": {\n"
                   << "    \"string_value\": \"hello world\",\n"
                   << "    \"int_value\": 42,\n"
                   << "    \"bool_value\": true,\n"
                   << "    \"double_value\": 3.14159,\n"
                   << "    \"array_value\": [1, 2, 3, 4, 5],\n"
                   << "    \"nested\": {\n"
                   << "      \"key1\": \"value1\",\n"
                   << "      \"key2\": \"value2\"\n"
                   << "    }\n"
                   << "  }\n"
                   << "}\n";
            config.close();
            
            BEATRICE_INFO("Created sample configuration file: {}", configFile);
        }
        
        // Initialize configuration from file
        if (!beatrice::Config::initialize(configFile)) {
            BEATRICE_ERROR("Failed to initialize configuration from file");
            return 1;
        }
        
        auto& config = beatrice::Config::get();
        BEATRICE_INFO("Configuration loaded successfully");
        
        // Demonstrate configuration access
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "CONFIGURATION ACCESS EXAMPLES\n";
        std::cout << std::string(60, '=') << "\n";
        
        // String values
        std::cout << "Log level: " << config.getString("logging.level") << "\n";
        std::cout << "Interface: " << config.getString("network.interface") << "\n";
        std::cout << "Custom string: " << config.getString("custom.string_value") << "\n";
        
        // Integer values
        std::cout << "Buffer size: " << config.getInt("network.bufferSize") << "\n";
        std::cout << "Number of threads: " << config.getInt("performance.numThreads") << "\n";
        std::cout << "Custom int: " << config.getInt("custom.int_value") << "\n";
        
        // Boolean values
        std::cout << "Promiscuous mode: " << (config.getBool("network.promiscuous") ? "enabled" : "disabled") << "\n";
        std::cout << "Auto load plugins: " << (config.getBool("plugins.autoLoad") ? "enabled" : "disabled") << "\n";
        std::cout << "Custom bool: " << (config.getBool("custom.bool_value") ? "true" : "false") << "\n";
        
        // Double values
        std::cout << "Custom double: " << config.getDouble("custom.double_value") << "\n";
        
        // Array values
        auto cpuAffinity = config.getArray("performance.cpuAffinity");
        std::cout << "CPU affinity: [";
        for (size_t i = 0; i < cpuAffinity.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << cpuAffinity[i].get<int>();
        }
        std::cout << "]\n";
        
        auto enabledPlugins = config.getArray("plugins.enabled");
        std::cout << "Enabled plugins: [";
        for (size_t i = 0; i < enabledPlugins.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << enabledPlugins[i].get<std::string>();
        }
        std::cout << "]\n";
        
        // Object values
        auto nested = config.getObject("custom.nested");
        std::cout << "Nested object:\n";
        for (auto it = nested.begin(); it != nested.end(); ++it) {
            std::cout << "  " << it.key() << ": " << it.value().get<std::string>() << "\n";
        }
        
        // Default values
        std::cout << "Non-existent key (with default): " << config.getString("nonexistent.key", "default_value") << "\n";
        std::cout << "Non-existent int (with default): " << config.getInt("nonexistent.number", 999) << "\n";
        
        // Check if keys exist
        std::cout << "\nKey existence checks:\n";
        std::cout << "logging.level exists: " << (config.has("logging.level") ? "yes" : "no") << "\n";
        std::cout << "nonexistent.key exists: " << (config.has("nonexistent.key") ? "yes" : "no") << "\n";
        
        // Modify configuration
        std::cout << "\nModifying configuration...\n";
        config.set("network.bufferSize", 4096);
        config.set("performance.numThreads", 4);
        config.set("custom.new_key", "new_value");
        
        std::cout << "Updated buffer size: " << config.getInt("network.bufferSize") << "\n";
        std::cout << "Updated thread count: " << config.getInt("performance.numThreads") << "\n";
        std::cout << "New custom key: " << config.getString("custom.new_key") << "\n";
        
        // Save modified configuration
        std::string modifiedConfigFile = "./modified_config.json";
        if (config.saveToFile(modifiedConfigFile)) {
            BEATRICE_INFO("Modified configuration saved to: {}", modifiedConfigFile);
        } else {
            BEATRICE_ERROR("Failed to save modified configuration");
        }
        
        // Environment variable override
        std::cout << "\nEnvironment variable override:\n";
        setenv("BEATRICE_NETWORK_INTERFACE", "eth1", 1);
        setenv("BEATRICE_LOGGING_LEVEL", "trace", 1);
        
        if (config.loadFromEnvironment()) {
            std::cout << "Interface from env: " << config.getString("network.interface") << "\n";
            std::cout << "Log level from env: " << config.getString("logging.level") << "\n";
        }
        
        // Configuration validation
        std::cout << "\nConfiguration validation:\n";
        if (config.validate()) {
            std::cout << "Configuration is valid\n";
        } else {
            std::cout << "Configuration validation failed\n";
        }
        
        // Export configuration as JSON
        std::cout << "\nCurrent configuration (JSON):\n";
        std::cout << config.toJson() << "\n";
        
        // Get all settings
        auto& settings = config.getSettings();
        std::cout << "\nSettings structure access:\n";
        std::cout << "Log level: " << settings.logging.level << "\n";
        std::cout << "Interface: " << settings.network.interface << "\n";
        std::cout << "Threads: " << settings.performance.numThreads << "\n";
        
        std::cout << std::string(60, '=') << "\n";
        BEATRICE_INFO("Configuration example completed successfully");
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Configuration example failed: " << e.what() << std::endl;
        return 1;
    }
} 