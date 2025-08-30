#include "beatrice/PluginManager.hpp"
#include "beatrice/Logger.hpp"
#include "beatrice/Error.hpp"
#include <filesystem>
#include <algorithm>
#include <dlfcn.h>

namespace beatrice {

PluginManager::PluginManager() {
    BEATRICE_DEBUG("PluginManager initialized");
}

PluginManager::~PluginManager() {
    BEATRICE_DEBUG("PluginManager shutting down, unloading {} plugins", plugins_.size());
    
    // Unload all plugins in reverse order
    for (auto it = plugins_.rbegin(); it != plugins_.rend(); ++it) {
        try {
            (*it)->onStop();
        } catch (const std::exception& e) {
            BEATRICE_ERROR("Exception during plugin shutdown: {}", e.what());
        }
    }
    
    // Close all handles
    for (auto& [name, handle] : handles_) {
        if (handle) {
            dlclose(handle);
            BEATRICE_DEBUG("Closed plugin handle: {}", name);
        }
    }
    
    plugins_.clear();
    handles_.clear();
}

bool PluginManager::loadPlugin(const std::string& path) {
    if (path.empty()) {
        BEATRICE_ERROR("Plugin path is empty");
        return false;
    }
    
    if (!std::filesystem::exists(path)) {
        BEATRICE_ERROR("Plugin file does not exist: {}", path);
        return false;
    }
    
    if (plugins_.size() >= maxPlugins_) {
        BEATRICE_ERROR("Maximum number of plugins ({}) reached", maxPlugins_);
        return false;
    }
    
    BEATRICE_INFO("Loading plugin: {}", path);
    
    // Open the shared library
    void* handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!handle) {
        BEATRICE_ERROR("Failed to load plugin {}: {}", path, dlerror());
        return false;
    }
    
    // Clear any previous error
    dlerror();
    
    // Get the plugin creation function
    using CreateFunc = IPacketPlugin* (*)();
    CreateFunc create = reinterpret_cast<CreateFunc>(dlsym(handle, "createPlugin"));
    
    if (!create) {
        BEATRICE_ERROR("Failed to find createPlugin symbol in {}: {}", path, dlerror());
        dlclose(handle);
        return false;
    }
    
    // Create plugin instance
    IPacketPlugin* rawPlugin = nullptr;
    try {
        rawPlugin = create();
        if (!rawPlugin) {
            BEATRICE_ERROR("Plugin creation function returned null for {}", path);
            dlclose(handle);
            return false;
        }
    } catch (const std::exception& e) {
        BEATRICE_ERROR("Exception during plugin creation for {}: {}", path, e.what());
        dlclose(handle);
        return false;
    }
    
    auto plugin = std::unique_ptr<IPacketPlugin>(rawPlugin);
    
    // Initialize plugin
    try {
        plugin->onStart();
        BEATRICE_INFO("Plugin {} started successfully", plugin->getName());
    } catch (const std::exception& e) {
        BEATRICE_ERROR("Failed to start plugin {}: {}", path, e.what());
        dlclose(handle);
        return false;
    }
    
    // Store plugin and handle
    std::string pluginName = plugin->getName();
    if (pluginName.empty()) {
        pluginName = std::filesystem::path(path).stem().string();
    }
    
    // Check for duplicate names
    if (std::any_of(plugins_.begin(), plugins_.end(), 
                    [&pluginName](const auto& p) { return p->getName() == pluginName; })) {
        BEATRICE_ERROR("Plugin with name '{}' already loaded", pluginName);
        dlclose(handle);
        return false;
    }
    
    plugins_.push_back(std::move(plugin));
    handles_[pluginName] = handle;
    
    BEATRICE_INFO("Plugin {} loaded successfully ({} total)", pluginName, plugins_.size());
    return true;
}

void PluginManager::unloadPlugin(const std::string& name) {
    BEATRICE_INFO("Unloading plugin: {}", name);
    
    auto handleIt = handles_.find(name);
    if (handleIt == handles_.end()) {
        BEATRICE_WARN("Plugin '{}' not found for unloading", name);
        return;
    }
    
    // Find and remove plugin from plugins_ vector
    auto pluginIt = std::find_if(plugins_.begin(), plugins_.end(),
                                [&name](const auto& plugin) { return plugin->getName() == name; });
    
    if (pluginIt != plugins_.end()) {
        try {
            (*pluginIt)->onStop();
            BEATRICE_DEBUG("Plugin {} stopped successfully", name);
        } catch (const std::exception& e) {
            BEATRICE_ERROR("Exception during plugin shutdown for {}: {}", name, e.what());
        }
        
        plugins_.erase(pluginIt);
    }
    
    // Close handle
    if (handleIt->second) {
        dlclose(handleIt->second);
        BEATRICE_DEBUG("Plugin handle closed for {}", name);
    }
    
    handles_.erase(handleIt);
    BEATRICE_INFO("Plugin {} unloaded successfully ({} remaining)", name, plugins_.size());
}

void PluginManager::processPacket(Packet& packet) {
    if (plugins_.empty()) {
        return;
    }
    
    for (auto& plugin : plugins_) {
        try {
            plugin->onPacket(packet);
        } catch (const std::exception& e) {
            BEATRICE_ERROR("Exception in plugin {} while processing packet: {}", 
                          plugin->getName(), e.what());
        }
    }
}

void PluginManager::processPackets(const std::vector<Packet>& packets) {
    if (plugins_.empty() || packets.empty()) {
        return;
    }
    
    for (auto& plugin : plugins_) {
        try {
            for (const auto& packet : packets) {
                plugin->onPacket(const_cast<Packet&>(packet));
            }
        } catch (const std::exception& e) {
            BEATRICE_ERROR("Exception in plugin {} while processing {} packets: {}", 
                          plugin->getName(), packets.size(), e.what());
        }
    }
}

bool PluginManager::hasPlugin(const std::string& name) const {
    return std::any_of(plugins_.begin(), plugins_.end(),
                      [&name](const auto& plugin) { return plugin->getName() == name; });
}

std::vector<std::string> PluginManager::getLoadedPluginNames() const {
    std::vector<std::string> names;
    names.reserve(plugins_.size());
    
    for (const auto& plugin : plugins_) {
        names.push_back(plugin->getName());
    }
    
    return names;
}

size_t PluginManager::getPluginCount() const {
    return plugins_.size();
}

void PluginManager::setMaxPlugins(size_t max) {
    maxPlugins_ = max;
    BEATRICE_DEBUG("Maximum plugins set to {}", maxPlugins_);
}

size_t PluginManager::getMaxPlugins() const {
    return maxPlugins_;
}

void PluginManager::reloadPlugin(const std::string& name) {
    BEATRICE_INFO("Reloading plugin: {}", name);
    
    if (!hasPlugin(name)) {
        BEATRICE_ERROR("Cannot reload plugin '{}': not loaded", name);
        return;
    }
    
    // Get the original path from handle
    auto handleIt = handles_.find(name);
    if (handleIt == handles_.end()) {
        BEATRICE_ERROR("Cannot find handle for plugin '{}'", name);
        return;
    }
    
    // Unload and reload
    unloadPlugin(name);
    
    // Note: We would need to store the original path to reload
    // For now, this is a limitation - plugins need to be reloaded manually
    BEATRICE_WARN("Plugin '{}' unloaded. Please reload manually with loadPlugin()", name);
}

} // namespace beatrice
