#include "beatrice/Config.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>

namespace beatrice {

// Static member initialization
std::unique_ptr<Config> Config::instance_ = nullptr;
std::mutex Config::mutex_;

Result<void> Config::initialize(const std::string& configFile, const std::string& defaultConfig) {
    try {
        if (initialized_) {
            return Result<void>::success();
        }

        // Load default configuration
        if (!defaultConfig.empty()) {
            try {
                config_ = nlohmann::json::parse(defaultConfig);
            } catch (const nlohmann::json::parse_error& e) {
                return Result<void>::error(ErrorCode::INITIALIZATION_FAILED, 
                                         "Failed to parse default config: " + std::string(e.what()));
            }
        }

        // Load configuration from file if specified
        if (!configFile.empty()) {
            if (!loadFromFile(configFile)) {
                return Result<void>::error(ErrorCode::RESOURCE_UNAVAILABLE, 
                                         "Cannot open config file: " + configFile);
            }
        }

        // Load environment variables
        loadFromEnvironment();

        initialized_ = true;
        return Result<void>::success();

    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::INITIALIZATION_FAILED, e.what());
    }
}

Config& Config::get() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!instance_) {
        instance_ = std::unique_ptr<Config>(new Config());
    }
    return *instance_;
}

bool Config::loadFromFile(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json fileConfig;
        file >> fileConfig;

        // Merge with existing config
        config_.merge_patch(fileConfig);

        return true;

    } catch (const std::exception& e) {
        return false;
    }
}

bool Config::loadFromEnvironment() {
    bool loaded = false;
    
    // Common environment variables
    const char* envVars[] = {
        "BEATRICE_LOG_LEVEL", "BEATRICE_LOG_FILE", "BEATRICE_INTERFACE",
        "BEATRICE_BUFFER_SIZE", "BEATRICE_NUM_BUFFERS", "BEATRICE_PROMISCUOUS",
        "BEATRICE_TIMEOUT", "BEATRICE_BATCH_SIZE", "BEATRICE_NUM_THREADS"
    };

    for (const auto& envVar : envVars) {
        const char* value = std::getenv(envVar);
        if (value) {
            std::string key = std::string(envVar).substr(9); // Remove "BEATRICE_" prefix
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            
            // Convert key to dot notation
            std::replace(key.begin(), key.end(), '_', '.');
            
            // Try to parse as different types
            try {
                int intVal = std::stoi(value);
                config_[key] = intVal;
                loaded = true;
            } catch (...) {
                try {
                    double doubleVal = std::stod(value);
                    config_[key] = doubleVal;
                    loaded = true;
                } catch (...) {
                    // Treat as string
                    config_[key] = value;
                    loaded = true;
                }
            }
        }
    }

    return loaded;
}

std::string Config::getString(const std::string& key, const std::string& defaultValue) const {
    try {
        auto keys = splitKey(key);
        auto value = getNestedValue(keys);
        if (value.is_string()) {
            return value.get<std::string>();
        }
    } catch (...) {
        // Fall through to default
    }
    return defaultValue;
}

int Config::getInt(const std::string& key, int defaultValue) const {
    try {
        auto keys = splitKey(key);
        auto value = getNestedValue(keys);
        if (value.is_number()) {
            return value.get<int>();
        }
    } catch (...) {
        // Fall through to default
    }
    return defaultValue;
}

bool Config::getBool(const std::string& key, bool defaultValue) const {
    try {
        auto keys = splitKey(key);
        auto value = getNestedValue(keys);
        if (value.is_boolean()) {
            return value.get<bool>();
        }
    } catch (...) {
        // Fall through to default
    }
    return defaultValue;
}

double Config::getDouble(const std::string& key, double defaultValue) const {
    try {
        auto keys = splitKey(key);
        auto value = getNestedValue(keys);
        if (value.is_number()) {
            return value.get<double>();
        }
    } catch (...) {
        // Fall through to default
    }
    return defaultValue;
}

nlohmann::json Config::getArray(const std::string& key) const {
    try {
        auto keys = splitKey(key);
        auto value = getNestedValue(keys);
        if (value.is_array()) {
            return value;
        }
    } catch (...) {
        // Fall through to empty array
    }
    return nlohmann::json::array();
}

nlohmann::json Config::getObject(const std::string& key) const {
    try {
        auto keys = splitKey(key);
        auto value = getNestedValue(keys);
        if (value.is_object()) {
            return value;
        }
    } catch (...) {
        // Fall through to empty object
    }
    return nlohmann::json::object();
}

bool Config::has(const std::string& key) const {
    try {
        auto keys = splitKey(key);
        auto value = getNestedValue(keys);
        return !value.is_null();
    } catch (...) {
        return false;
    }
}

void Config::set(const std::string& key, const nlohmann::json& value) {
    auto keys = splitKey(key);
    setNestedValue(keys, value);
}

bool Config::saveToFile(const std::string& filename) const {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        file << config_.dump(2);
        return true;
        
    } catch (...) {
        return false;
    }
}

bool Config::validate() const {
    // Basic validation - check if required fields exist
    std::vector<std::string> requiredFields = {
        "logging.level", "network.interface", "network.bufferSize"
    };
    
    for (const auto& field : requiredFields) {
        if (!has(field)) {
            return false;
        }
    }
    
    return true;
}

std::string Config::toJson() const {
    return config_.dump(2);
}

// Private helper methods

std::vector<std::string> Config::splitKey(const std::string& key) const {
    std::vector<std::string> keys;
    std::string current;
    
    for (char c : key) {
        if (c == '.') {
            if (!current.empty()) {
                keys.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    
    if (!current.empty()) {
        keys.push_back(current);
    }
    
    return keys;
}

nlohmann::json Config::getNestedValue(const std::vector<std::string>& keys) const {
    nlohmann::json current = config_;
    
    for (const auto& key : keys) {
        if (current.is_object() && current.contains(key)) {
            current = current[key];
        } else {
            throw std::runtime_error("Key not found: " + key);
        }
    }
    
    return current;
}

void Config::setNestedValue(const std::vector<std::string>& keys, const nlohmann::json& value) {
    if (keys.empty()) {
        config_ = value;
        return;
    }
    
    nlohmann::json* current = &config_;
    
    for (size_t i = 0; i < keys.size() - 1; ++i) {
        if (!current->contains(keys[i]) || !(*current)[keys[i]].is_object()) {
            (*current)[keys[i]] = nlohmann::json::object();
        }
        current = &(*current)[keys[i]];
    }
    
    (*current)[keys.back()] = value;
}

} // namespace beatrice 