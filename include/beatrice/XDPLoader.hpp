#ifndef BEATRICE_XDPLOADER_HPP
#define BEATRICE_XDPLOADER_HPP

#include "beatrice/Error.hpp"
#include <optional>
#include <string>
#include <memory>
#include <vector>

namespace beatrice {

/**
 * @brief XDP Program Loader for Beatrice SDK
 * 
 * Handles loading and managing XDP programs in the kernel.
 * Supports both native and JIT compilation modes.
 */
class XDPLoader {
public:
    /**
     * @brief XDP program configuration
     */
    struct Config {
        std::string interface;           ///< Network interface name
        std::string programPath;         ///< Path to BPF program file
        std::string programName;         ///< Program name in kernel
        bool jitCompile = true;          ///< Enable JIT compilation
        bool forceReload = false;        ///< Force program reload
        std::string pinPath = "/sys/fs/bpf"; ///< BPF filesystem path
        int priority = 0;                ///< XDP program priority
    };

    /**
     * @brief XDP program information
     */
    struct ProgramInfo {
        int programFd;                   ///< Program file descriptor
        int mapFd;                       ///< Map file descriptor
        std::string programName;         ///< Program name
        std::string interface;           ///< Attached interface
        bool isAttached;                 ///< Whether program is attached
        std::string pinPath;             ///< Pinned program path
    };

    XDPLoader();
    ~XDPLoader();

    /**
     * @brief Load XDP program into kernel
     * @param config Configuration for program loading
     * @return Result indicating success or failure
     */
    Result<void> loadProgram(const Config& config);

    /**
     * @brief Attach XDP program to network interface
     * @param interface Network interface name
     * @param programFd Program file descriptor
     * @param xdpMode XDP mode (driver/skb/generic)
     * @return Result indicating success or failure
     */
    Result<void> attachProgram(const std::string& interface, int programFd, const std::string& xdpMode = "driver");

    /**
     * @brief Detach XDP program from network interface
     * @param interface Network interface name
     * @return Result indicating success or failure
     */
    Result<void> detachProgram(const std::string& interface);

    /**
     * @brief Unload XDP program from kernel
     * @param programName Program name to unload
     * @return Result indicating success or failure
     */
    Result<void> unloadProgram(const std::string& programName);

    /**
     * @brief Get loaded program information
     * @param programName Program name
     * @return Program information or empty optional if not found
     */
    std::optional<ProgramInfo> getProgramInfo(const std::string& programName) const;
    
    /**
     * @brief Check if program is attached to interface
     * @param interface Network interface name
     * @return True if program is attached
     */
    bool isProgramAttached(const std::string& interface) const;

    /**
     * @brief List all loaded XDP programs
     * @return Vector of program information
     */
    std::vector<ProgramInfo> listPrograms();

    /**
     * @brief Get XDP program statistics
     * @param interface Network interface name
     * @return Statistics string or empty string on error
     */
    std::string getProgramStats(const std::string& interface);

    /**
     * @brief Clean up all loaded programs
     */
    void cleanup();

private:
    // BPF program management
    Result<int> loadBpfProgram(const std::string& programPath, const std::string& programName);
    Result<int> createBpfMap();
    Result<void> pinProgram(const std::string& programName, int programFd);
    Result<void> pinMap(const std::string& mapName, int mapFd);
    
    // Interface management
    Result<void> setInterfaceXdpMode(const std::string& interface, bool enable);
    Result<int> getInterfaceIndex(const std::string& interface);
    
    // Filesystem operations
    Result<void> createBpfDirectory(const std::string& path);
    Result<void> removeBpfFile(const std::string& path);
    
    // Program tracking
    mutable std::mutex programsMutex_;
    std::vector<ProgramInfo> loadedPrograms_;
    
    // Disable copying
    XDPLoader(const XDPLoader&) = delete;
    XDPLoader& operator=(const XDPLoader&) = delete;
};

} // namespace beatrice

#endif // BEATRICE_XDPLOADER_HPP 