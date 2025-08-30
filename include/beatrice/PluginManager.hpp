#ifndef BEATRICE_PLUGINMANAGER_HPP
#define BEATRICE_PLUGINMANAGER_HPP

#include "IPacketPlugin.hpp"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <dlfcn.h>

namespace beatrice {

class PluginManager {
public:
    PluginManager();
    ~PluginManager();
    
    // Plugin lifecycle management
    bool loadPlugin(const std::string& path);
    void unloadPlugin(const std::string& name);
    void reloadPlugin(const std::string& name);
    
    // Packet processing
    void processPacket(Packet& packet);
    void processPackets(const std::vector<Packet>& packets);
    
    // Plugin information
    bool hasPlugin(const std::string& name) const;
    std::vector<std::string> getLoadedPluginNames() const;
    size_t getPluginCount() const;
    
    // Configuration
    void setMaxPlugins(size_t max);
    size_t getMaxPlugins() const;

private:
    std::vector<std::unique_ptr<IPacketPlugin>> plugins_;
    std::unordered_map<std::string, void*> handles_;
    size_t maxPlugins_{10};
    
    // Disable copying
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;
};

} // namespace beatrice

#endif // BEATRICE_PLUGINMANAGER_HPP
