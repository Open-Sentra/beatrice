#include "beatrice/XDPLoader.hpp"
#include "beatrice/Logger.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/if_link.h>
#include <linux/bpf.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <errno.h>

namespace beatrice {

XDPLoader::XDPLoader() {
    BEATRICE_INFO("=== XDPLoader constructor called ===");
    BEATRICE_DEBUG("XDPLoader created");
    
    // Test if basic operations work
    BEATRICE_INFO("TEST: Testing basic operations...");
    loadedPrograms_.clear();  // Test vector operations
    BEATRICE_INFO("TEST: Vector operations OK");
    
    // Test if mutex works
    std::lock_guard<std::mutex> lock(programsMutex_);
    BEATRICE_INFO("TEST: Mutex operations OK");
}

XDPLoader::~XDPLoader() {
    BEATRICE_DEBUG("XDPLoader destroying");
    cleanup();
}

Result<void> XDPLoader::loadProgram(const Config& config) {
    BEATRICE_INFO("=== XDPLoader::loadProgram METHOD CALLED ===");
    BEATRICE_INFO("=== XDPLoader::loadProgram START ===");
    BEATRICE_INFO("Program path: {}", config.programPath);
    BEATRICE_INFO("Program name: {}", config.programName);
    
    try {
        // STEP 1: Load BPF program into kernel
        BEATRICE_INFO("STEP 1: Loading BPF program into kernel...");
        auto programFdResult = loadBpfProgram(config.programPath, config.programName);
        if (!programFdResult.isSuccess()) {
            BEATRICE_ERROR("Failed to load BPF program: {}", programFdResult.getErrorMessage());
            return Result<void>::error(programFdResult.getErrorCode(), 
                                     "Failed to load BPF program: " + programFdResult.getErrorMessage());
        }
        
        int programFd = programFdResult.getValue();
        BEATRICE_INFO("STEP 1: BPF program loaded successfully, FD: {}", programFd);
        
        // STEP 2: Create BPF map for XSK
        BEATRICE_INFO("STEP 2: Creating BPF map for XSK...");
        auto mapFdResult = createBpfMap();
        if (!mapFdResult.isSuccess()) {
            BEATRICE_ERROR("Failed to create BPF map: {}", mapFdResult.getErrorMessage());
            close(programFd);
            return Result<void>::error(mapFdResult.getErrorCode(), 
                                     "Failed to create BPF map: " + mapFdResult.getErrorMessage());
        }
        
        int mapFd = mapFdResult.getValue();
        BEATRICE_INFO("STEP 2: BPF map created successfully, FD: {}", mapFd);
        
        // STEP 3: Pin program and map to BPF filesystem
        BEATRICE_INFO("STEP 3: Pinning program and map to BPF filesystem...");
        auto pinProgramResult = pinProgram(config.programName, programFd);
        if (!pinProgramResult.isSuccess()) {
            BEATRICE_ERROR("Failed to pin program: {}", pinProgramResult.getErrorMessage());
            close(programFd);
            close(mapFd);
            return Result<void>::error(pinProgramResult.getErrorCode(), 
                                     "Failed to pin program: " + pinProgramResult.getErrorMessage());
        }
        
        auto pinMapResult = pinMap(config.programName + "_map", mapFd);
        if (!pinMapResult.isSuccess()) {
            BEATRICE_ERROR("Failed to pin map: {}", pinMapResult.getErrorMessage());
            close(programFd);
            close(mapFd);
            removeBpfFile("/sys/fs/bpf/" + config.programName);
            return Result<void>::error(pinMapResult.getErrorCode(), 
                                     "Failed to pin map: " + pinMapResult.getErrorMessage());
        }
        
        BEATRICE_INFO("STEP 3: Program and map pinned successfully");
        
        // STEP 4: Create program info and add to tracking
        BEATRICE_INFO("STEP 4: Adding program to tracking system...");
        ProgramInfo programInfo;
        programInfo.programFd = programFd;
        programInfo.mapFd = mapFd;
        programInfo.programName = config.programName;
        programInfo.interface = config.interface;
        programInfo.isAttached = false;
        programInfo.pinPath = "/sys/fs/bpf/" + config.programName;
        
        {
            std::lock_guard<std::mutex> lock(programsMutex_);
            loadedPrograms_.push_back(programInfo);
        }
        
        BEATRICE_INFO("STEP 4: Program added to tracking system successfully");
        BEATRICE_INFO("XDP program '{}' loaded successfully with FD: {}", config.programName, programFd);
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        BEATRICE_ERROR("Exception during program loading: {}", e.what());
        return Result<void>::error(ErrorCode::INITIALIZATION_FAILED, 
                                 "Exception during program loading: " + std::string(e.what()));
    }
}

Result<void> XDPLoader::attachProgram(const std::string& interface, int programFd, const std::string& xdpMode) {
    try {
        BEATRICE_INFO("Attaching XDP program to interface: {} in {} mode", interface, xdpMode);
        
        // Get interface index
        auto ifIndex = getInterfaceIndex(interface);
        if (!ifIndex.isSuccess()) {
            return Result<void>::error(ErrorCode::INITIALIZATION_FAILED,
                                     "Failed to get interface index: " + ifIndex.getErrorMessage());
        }
        
        // Determine XDP flags based on mode
        __u32 xdpFlags = 0;
        if (xdpMode == "driver") {
            xdpFlags = XDP_FLAGS_DRV_MODE;
            BEATRICE_INFO("Using XDP driver mode");
        } else if (xdpMode == "skb" || xdpMode == "generic") {
            xdpFlags = XDP_FLAGS_SKB_MODE;
            BEATRICE_INFO("Using XDP SKB/generic mode");
        } else {
            BEATRICE_ERROR("Invalid XDP mode: {}", xdpMode);
            return Result<void>::error(ErrorCode::INITIALIZATION_FAILED,
                                     "Invalid XDP mode: " + xdpMode);
        }
        
        // Attach program to interface
        int ret = bpf_xdp_attach(ifIndex.getValue(), programFd, xdpFlags, nullptr);
        if (ret < 0) {
            BEATRICE_ERROR("Failed to attach XDP program to interface {} in {} mode: {} (errno: {})", 
                          interface, xdpMode, strerror(errno), errno);
            return Result<void>::error(ErrorCode::INITIALIZATION_FAILED,
                                     "Failed to attach XDP program: " + std::string(strerror(errno)));
        }
        
        BEATRICE_INFO("XDP program attached to interface {} in {} mode successfully", interface, xdpMode);
        
        // Update program info to mark as attached
        {
            std::lock_guard<std::mutex> lock(programsMutex_);
            for (auto& program : loadedPrograms_) {
                if (program.interface == interface) {
                    program.isAttached = true;
                    BEATRICE_INFO("Updated program {} as attached to interface {}", 
                                  program.programName, interface);
                    break;
                }
            }
        }
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        BEATRICE_ERROR("Exception during program attachment: {}", e.what());
        return Result<void>::error(ErrorCode::INITIALIZATION_FAILED,
                                 "Exception during program attachment: " + std::string(e.what()));
    }
}

Result<void> XDPLoader::detachProgram(const std::string& interface) {
    try {
        // Get interface index
        auto ifIndex = getInterfaceIndex(interface);
        if (!ifIndex.isSuccess()) {
            return Result<void>::error(ErrorCode::INVALID_ARGUMENT, 
                                     "Invalid interface: " + interface);
        }
        
        // Detach XDP program
        int ret = bpf_xdp_detach(ifIndex.getValue(), 0, nullptr);
        if (ret < 0) {
            BEATRICE_ERROR("Failed to detach XDP program: {}", strerror(errno));
            return Result<void>::error(ErrorCode::INTERNAL_ERROR, 
                                     "bpf_xdp_detach failed: " + std::string(strerror(errno)));
        }
        
        // Disable XDP mode on interface
        setInterfaceXdpMode(interface, false);
        
        BEATRICE_INFO("XDP program detached from interface {} successfully", interface);
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        BEATRICE_ERROR("Exception during program detachment: {}", e.what());
        return Result<void>::error(ErrorCode::INTERNAL_ERROR, e.what());
    }
}

Result<void> XDPLoader::unloadProgram(const std::string& programName) {
    try {
        auto programInfo = getProgramInfo(programName);
        if (!programInfo) {
            BEATRICE_WARN("Program {} not found", programName);
            return Result<void>::success();
        }
        
        // Detach from interface
        if (programInfo->isAttached) {
            detachProgram(programInfo->interface);
        }
        
        // Close file descriptors
        if (programInfo->programFd >= 0) {
            close(programInfo->programFd);
        }
        if (programInfo->mapFd >= 0) {
            close(programInfo->mapFd);
        }
        
        // Remove pinned files
        removeBpfFile(programInfo->pinPath);
        removeBpfFile(programInfo->pinPath + "_map");
        
        // Remove from tracking
        {
            std::lock_guard<std::mutex> lock(programsMutex_);
            loadedPrograms_.erase(
                std::remove_if(loadedPrograms_.begin(), loadedPrograms_.end(),
                              [&](const ProgramInfo& info) { return info.programName == programName; }),
                loadedPrograms_.end()
            );
        }
        
        BEATRICE_INFO("Program {} unloaded successfully", programName);
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        BEATRICE_ERROR("Exception during program unloading: {}", e.what());
        return Result<void>::error(ErrorCode::INTERNAL_ERROR, e.what());
    }
}

std::optional<XDPLoader::ProgramInfo> XDPLoader::getProgramInfo(const std::string& programName) const {
    std::lock_guard<std::mutex> lock(programsMutex_);
    
    for (const auto& program : loadedPrograms_) {
        if (program.programName == programName) {
            return program;
        }
    }
    
    return std::nullopt;
}

std::vector<XDPLoader::ProgramInfo> XDPLoader::listPrograms() {
    std::lock_guard<std::mutex> lock(programsMutex_);
    return loadedPrograms_;
}

bool XDPLoader::isProgramAttached(const std::string& interface) const {
    std::lock_guard<std::mutex> lock(programsMutex_);
    
    for (const auto& program : loadedPrograms_) {
        if (program.interface == interface && program.isAttached) {
            return true;
        }
    }
    return false;
}

std::string XDPLoader::getProgramStats(const std::string& interface) {
    // This would read statistics from BPF maps
    // For now, return basic information
    return "XDP program statistics for interface: " + interface;
}

void XDPLoader::cleanup() {
    BEATRICE_INFO("Cleaning up XDP loader");
    
    std::lock_guard<std::mutex> lock(programsMutex_);
    
    for (const auto& program : loadedPrograms_) {
        try {
            if (program.isAttached) {
                detachProgram(program.interface);
            }
            if (program.programFd >= 0) {
                close(program.programFd);
            }
            if (program.mapFd >= 0) {
                close(program.mapFd);
            }
        } catch (const std::exception& e) {
            BEATRICE_ERROR("Exception during cleanup of program {}: {}", program.programName, e.what());
        }
    }
    
    loadedPrograms_.clear();
}

// Private implementation methods

Result<int> XDPLoader::loadBpfProgram(const std::string& programPath, const std::string& programName) {
    BEATRICE_DEBUG("Loading BPF program from: {} with name: {}", programPath, programName);
    
    // Load BPF program using libbpf
    struct bpf_object* obj = bpf_object__open(programPath.c_str());
    if (!obj) {
        BEATRICE_ERROR("Failed to open BPF object: {} (errno: {})", programPath, strerror(errno));
        return Result<int>::error(ErrorCode::RESOURCE_UNAVAILABLE, 
                                 "Failed to open BPF object: " + std::string(strerror(errno)));
    }
    
    BEATRICE_DEBUG("BPF object opened successfully");
    
    // Load program into kernel
    int ret = bpf_object__load(obj);
    if (ret < 0) {
        BEATRICE_ERROR("Failed to load BPF object: {} (ret: {}, errno: {})", programPath, ret, strerror(errno));
        bpf_object__close(obj);
        return Result<int>::error(ErrorCode::INITIALIZATION_FAILED, 
                                 "Failed to load BPF object: " + std::string(strerror(errno)));
    }
    
    BEATRICE_DEBUG("BPF object loaded into kernel successfully");
    
    // Try to find program by the specified name first
    struct bpf_program* prog = bpf_object__find_program_by_name(obj, programName.c_str());
    if (!prog) {
        // Fallback: try to find any XDP program
        BEATRICE_WARN("Program '{}' not found, trying to find any XDP program", programName);
        prog = bpf_object__find_program_by_name(obj, "xdp_prog");
        if (!prog) {
            // Last resort: try to find any program
            BEATRICE_WARN("XDP program 'xdp_prog' not found, trying to find any program");
            prog = bpf_object__find_program_by_name(obj, "xdp");
            if (!prog) {
                // List all available programs for debugging
                BEATRICE_ERROR("No suitable program found in object. Available programs:");
                bpf_object__for_each_program(prog, obj) {
                    BEATRICE_ERROR("  - {}", bpf_program__name(prog));
                }
                bpf_object__close(obj);
                return Result<int>::error(ErrorCode::INITIALIZATION_FAILED, 
                                         "No suitable program found in object");
            }
        }
    }
    
    BEATRICE_INFO("Found program: {} in object", bpf_program__name(prog));
    
    int progFd = bpf_program__fd(prog);
    if (progFd < 0) {
        BEATRICE_ERROR("Failed to get program file descriptor: {} (errno: {})", progFd, strerror(errno));
        bpf_object__close(obj);
        return Result<int>::error(ErrorCode::INITIALIZATION_FAILED, 
                                 "Failed to get program file descriptor");
    }
    
    BEATRICE_DEBUG("Program file descriptor obtained: {}", progFd);
    
    // Store the object for later cleanup
    // Note: In a production system, you'd want to manage this more carefully
    // For now, we'll keep it simple and let the kernel handle cleanup
    
    return Result<int>::success(progFd);
}

Result<int> XDPLoader::createBpfMap() {
    // Create XSK map for redirecting packets to user space
    int mapFd = bpf_map_create(BPF_MAP_TYPE_XSKMAP, nullptr, sizeof(int), sizeof(int), 1, nullptr);
    if (mapFd < 0) {
        return Result<int>::error(ErrorCode::INITIALIZATION_FAILED, 
                                 "Failed to create XSK map: " + std::string(strerror(errno)));
    }
    
    return Result<int>::success(mapFd);
}

Result<void> XDPLoader::pinProgram(const std::string& programName, int programFd) {
    std::string pinPath = "/sys/fs/bpf/" + programName;
    
    if (!createBpfDirectory("/sys/fs/bpf").isSuccess()) {
        return Result<void>::error(ErrorCode::RESOURCE_UNAVAILABLE, "Failed to create BPF directory");
    }
    
    // Create a BPF link to pin the program
    // This is the proper way to pin an XDP program
    struct bpf_link_create_opts opts = {};
    opts.flags = 0;
    
    int linkFd = bpf_link_create(programFd, 0, BPF_XDP, &opts);
    if (linkFd < 0) {
        BEATRICE_WARN("Failed to create BPF link for program {}: {} (errno: {})", 
                      programName, strerror(errno), errno);
        
        // Fallback: try to pin the program directly (may not work)
        BEATRICE_INFO("Attempting fallback pinning method...");
        int ret = bpf_obj_pin(programFd, pinPath.c_str());
        if (ret < 0) {
            BEATRICE_WARN("Fallback pinning also failed: {} (errno: {})", strerror(errno), errno);
            BEATRICE_INFO("Program pinning skipped - program {} is loaded in kernel with FD: {}", programName, programFd);
            BEATRICE_INFO("Program can be accessed via /proc/{}/fd/{}", getpid(), programFd);
            return Result<void>::success();
        }
    } else {
        // Pin the link instead of the program
        std::string linkPinPath = pinPath + "_link";
        int ret = bpf_obj_pin(linkFd, linkPinPath.c_str());
        if (ret < 0) {
            BEATRICE_WARN("Failed to pin BPF link: {} (errno: {})", strerror(errno), errno);
        } else {
            BEATRICE_INFO("BPF link pinned successfully at: {}", linkPinPath);
        }
        close(linkFd);
    }
    
    BEATRICE_INFO("Program {} pinning completed", programName);
    return Result<void>::success();
}

Result<void> XDPLoader::pinMap(const std::string& mapName, int mapFd) {
    std::string pinPath = "/sys/fs/bpf/" + mapName;
    
    int ret = bpf_obj_pin(mapFd, pinPath.c_str());
    if (ret < 0) {
        return Result<void>::error(ErrorCode::INTERNAL_ERROR, 
                                 "Failed to pin map: " + std::string(strerror(errno)));
    }
    
    return Result<void>::success();
}

Result<void> XDPLoader::setInterfaceXdpMode(const std::string& interface, bool enable) {
    // Note: XDP mode is managed by attaching/detaching programs, not by interface flags
    // This method is kept for future use but currently does nothing
    (void)interface;
    (void)enable;
    
    return Result<void>::success();
}

Result<int> XDPLoader::getInterfaceIndex(const std::string& interface) {
    BEATRICE_DEBUG("Getting interface index for: {}", interface);
    
    // Method 1: Try if_nametoindex
    unsigned int ifIndex = if_nametoindex(interface.c_str());
    if (ifIndex != 0) {
        BEATRICE_DEBUG("Interface index found via if_nametoindex: {}", ifIndex);
        return Result<int>::success(static_cast<int>(ifIndex));
    }
    
    BEATRICE_WARN("if_nametoindex failed for interface: {} (errno: {})", interface, strerror(errno));
    
    // Method 2: Try reading from /sys/class/net
    std::string sysPath = "/sys/class/net/" + interface + "/ifindex";
    std::ifstream ifIndexFile(sysPath);
    if (ifIndexFile.is_open()) {
        int index;
        ifIndexFile >> index;
        ifIndexFile.close();
        if (index > 0) {
            BEATRICE_DEBUG("Interface index found via sysfs: {}", index);
            return Result<int>::success(index);
        }
    }
    
    BEATRICE_WARN("Failed to read interface index from sysfs: {}", sysPath);
    
    // Method 3: Try socket ioctl
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1);
        
        if (ioctl(sock, SIOCGIFINDEX, &ifr) >= 0) {
            close(sock);
            BEATRICE_DEBUG("Interface index found via ioctl: {}", ifr.ifr_ifindex);
            return Result<int>::success(ifr.ifr_ifindex);
        }
        
        BEATRICE_WARN("ioctl SIOCGIFINDEX failed: {}", strerror(errno));
        close(sock);
    }
    
    BEATRICE_ERROR("All methods failed to get interface index for: {}", interface);
    return Result<int>::error(ErrorCode::INVALID_ARGUMENT, 
                             "Interface not found: " + interface);
}

Result<void> XDPLoader::createBpfDirectory(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        try {
            std::filesystem::create_directories(path);
        } catch (const std::exception& e) {
            return Result<void>::error(ErrorCode::RESOURCE_UNAVAILABLE, 
                                     "Failed to create directory: " + std::string(e.what()));
        }
    }
    
    return Result<void>::success();
}

Result<void> XDPLoader::removeBpfFile(const std::string& path) {
    if (std::filesystem::exists(path)) {
        try {
            std::filesystem::remove(path);
        } catch (const std::exception& e) {
            BEATRICE_WARN("Failed to remove BPF file {}: {}", path, e.what());
        }
    }
    
    return Result<void>::success();
}

} // namespace beatrice 