#include "beatrice/Config.hpp"
#include "beatrice/Logger.hpp"
#include <iostream>
#include <fstream>

int main() {
    try {
        // Initialize logger
        beatrice::Logger::get().initialize("config_example", "", 1024*1024, 5);
        
        std::cout << "Starting configuration example" << std::endl;
        
        // Create a sample configuration file
        std::string configFile = "./example_config.json";
        std::ofstream configFileStream(configFile);
        if (configFileStream.is_open()) {
            configFileStream << "{\n"
                   << "  \"logging\": {\n"
                   << "    \"level\": \"debug\",\n"
                   << "    \"file\": \"./example.log\"\n"
                   << "  },\n"
                   << "  \"network\": {\n"
                   << "    \"interface\": \"eth0\",\n"
                   << "    \"backend\": \"af_xdp\"\n"
                   << "  }\n"
                   << "}\n";
            configFileStream.close();
            
            std::cout << "Created sample configuration file: " << configFile << std::endl;
        }
        
        // Initialize configuration from file
        auto& config = beatrice::Config::get();
        auto result = config.initialize(configFile);
        if (!result.isSuccess()) {
            std::cout << "Failed to initialize configuration from file: " << result.getErrorMessage() << std::endl;
            return 1;
        }
        
        std::cout << "Configuration loaded successfully" << std::endl;
        
        // Demonstrate configuration access
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "CONFIGURATION ACCESS EXAMPLES" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        
        // String values
        std::cout << "Log level: " << config.getString("logging.level", "info") << std::endl;
        std::cout << "Interface: " << config.getString("network.interface", "lo") << std::endl;
        
        // Check if keys exist
        std::cout << "logging.level exists: " << (config.has("logging.level") ? "yes" : "no") << std::endl;
        std::cout << "nonexistent.key exists: " << (config.has("nonexistent.key") ? "yes" : "no") << std::endl;
        
        // Set new values
        config.set("network.bufferSize", 4096);
        config.set("performance.numThreads", 4);
        config.set("custom.new_key", "new_value");
        
        // Get updated values
        std::cout << "Updated buffer size: " << config.getInt("network.bufferSize", 1024) << std::endl;
        std::cout << "Updated thread count: " << config.getInt("performance.numThreads", 1) << std::endl;
        std::cout << "New custom key: " << config.getString("custom.new_key", "default") << std::endl;
        
        // Save modified configuration
        std::string modifiedConfigFile = "./modified_config.json";
        if (config.saveToFile(modifiedConfigFile)) {
            std::cout << "Modified configuration saved to: " << modifiedConfigFile << std::endl;
        } else {
            std::cout << "Failed to save modified configuration" << std::endl;
        }
        
        // Load from environment
        if (config.loadFromEnvironment()) {
            std::cout << "Configuration loaded from environment variables" << std::endl;
            std::cout << "Interface from env: " << config.getString("network.interface", "lo") << std::endl;
            std::cout << "Log level from env: " << config.getString("logging.level", "info") << std::endl;
        }
        
        // Validate configuration
        if (config.validate()) {
            std::cout << "Configuration is valid" << std::endl;
        } else {
            std::cout << "Configuration validation failed" << std::endl;
        }
        
        // Export to JSON
        std::cout << "\nConfiguration as JSON:" << std::endl;
        std::cout << config.toJson() << std::endl;
        
        // Get all settings
        auto& settings = config.getSettings();
        std::cout << "\nAll settings:" << std::endl;
        for (const auto& [key, value] : settings) {
            std::cout << "  " << key << ": " << value << std::endl;
        }
        
        std::cout << "\nConfiguration example completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 