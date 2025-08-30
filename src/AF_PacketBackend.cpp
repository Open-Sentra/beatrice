#include "beatrice/AF_PacketBackend.hpp"
#include "beatrice/Logger.hpp"
#include "beatrice/Error.hpp"
#include <chrono>
#include <algorithm>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <arpa/inet.h>
#include <sys/mman.h>

namespace beatrice {

AF_PacketBackend::AF_PacketBackend()
    : running_(false)
    , initialized_(false)
    , config_()
    , socketFd_(-1)
    , promiscuousMode_(true)
    , bufferSize_(65536)
    , blockingMode_(false)
    , zeroCopyEnabled_(false)
    , dmaAccessEnabled_(false)
    , dmaDevice_("")
    , dmaBufferSize_(0)
    , dmaBuffers_(nullptr)
    , dmaBufferCount_(0)
    , dmaFd_(-1)
    , processingThread_()
    , packetQueue_()
    , packetQueueMutex_()
    , packetCondition_()
    , packetCallback_(nullptr)
    , callbackMutex_()
    , statsMutex_()
    , stats_()
    , errorMutex_()
    , lastError_() {
}

AF_PacketBackend::~AF_PacketBackend() {
    shutdown();
}

Result<void> AF_PacketBackend::initialize(const Config& config) {
    if (initialized_) {
        return Result<void>::success();
    }

    config_ = config;
    
    if (!validateInterface(config_.interface)) {
        return Result<void>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "Invalid interface: " + config_.interface);
    }

    if (!createSocket()) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "Failed to create AF_PACKET socket");
    }

    if (!bindToInterface()) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "Failed to bind to interface");
    }

    if (!setSocketOptions()) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "Failed to set socket options");
    }

    initialized_ = true;
    return Result<void>::success();
}

Result<void> AF_PacketBackend::start() {
    if (!initialized_) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "AF_PACKET backend not initialized");
    }

    if (running_) {
        return Result<void>::success();
    }

    running_ = true;
    processingThread_ = std::thread(&AF_PacketBackend::packetProcessingLoop, this);

    return Result<void>::success();
}

Result<void> AF_PacketBackend::stop() {
    if (!running_) {
        return Result<void>::success();
    }

    running_ = false;
    packetCondition_.notify_all();

    if (processingThread_.joinable()) {
        processingThread_.join();
    }

    return Result<void>::success();
}

bool AF_PacketBackend::isRunning() const noexcept {
    return running_;
}

std::optional<Packet> AF_PacketBackend::nextPacket(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(packetQueueMutex_);
    
    if (packetCondition_.wait_for(lock, timeout, [this] { return !packetQueue_.empty(); })) {
        if (!packetQueue_.empty()) {
            Packet packet = packetQueue_.front();
            packetQueue_.pop();
            return packet;
        }
    }
    
    return std::nullopt;
}

std::vector<Packet> AF_PacketBackend::getPackets(size_t maxPackets, std::chrono::milliseconds timeout) {
    std::vector<Packet> packets;
    std::unique_lock<std::mutex> lock(packetQueueMutex_);
    
    if (packetCondition_.wait_for(lock, timeout, [this] { return !packetQueue_.empty(); })) {
        while (!packetQueue_.empty() && packets.size() < maxPackets) {
            packets.push_back(packetQueue_.front());
            packetQueue_.pop();
        }
    }
    
    return packets;
}

void AF_PacketBackend::setPacketCallback(std::function<void(Packet)> callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    packetCallback_ = callback;
}

void AF_PacketBackend::removePacketCallback() {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    packetCallback_ = nullptr;
}

AF_PacketBackend::Statistics AF_PacketBackend::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void AF_PacketBackend::resetStatistics() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_ = Statistics{};
}

std::string AF_PacketBackend::getName() const {
    return "AF_PACKET Backend";
}

std::string AF_PacketBackend::getVersion() const {
    return "AF_PACKET Backend v1.0.0";
}

std::vector<std::string> AF_PacketBackend::getSupportedFeatures() const {
    return {
        "Raw packet capture",
        "Promiscuous mode",
        "Configurable buffer size",
        "Blocking/non-blocking mode",
        "Real-time packet processing",
        "Statistics collection"
    };
}

bool AF_PacketBackend::isFeatureSupported(const std::string& feature) const {
    auto features = getSupportedFeatures();
    return std::find(features.begin(), features.end(), feature) != features.end();
}

AF_PacketBackend::Config AF_PacketBackend::getConfig() const {
    return config_;
}

Result<void> AF_PacketBackend::updateConfig(const Config& config) {
    if (running_) {
        return Result<void>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "Cannot update config while running");
    }

    config_ = config;
    return Result<void>::success();
}

std::string AF_PacketBackend::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

bool AF_PacketBackend::isHealthy() const {
    return initialized_ && socketFd_ >= 0;
}

Result<void> AF_PacketBackend::healthCheck() {
    if (!initialized_) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "Backend not initialized");
    }

    if (socketFd_ < 0) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "Socket not valid");
    }

    return Result<void>::success();
}

Result<void> AF_PacketBackend::setPromiscuousMode(bool enabled) {
    if (running_) {
        return Result<void>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "Cannot change promiscuous mode while running");
    }
    
    promiscuousMode_ = enabled;
    return Result<void>::success();
}

Result<void> AF_PacketBackend::setBufferSize(size_t size) {
    if (running_) {
        return Result<void>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "Cannot change buffer size while running");
    }
    
    bufferSize_ = size;
    return Result<void>::success();
}

Result<void> AF_PacketBackend::setBlockingMode(bool blocking) {
    if (running_) {
        return Result<void>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "Cannot change blocking mode while running");
    }
    
    blockingMode_ = blocking;
    return Result<void>::success();
}

bool AF_PacketBackend::isPromiscuousMode() const {
    return promiscuousMode_;
}

size_t AF_PacketBackend::getBufferSize() const {
    return bufferSize_;
}

bool AF_PacketBackend::isBlockingMode() const {
    return blockingMode_;
}

bool AF_PacketBackend::validateInterface(const std::string& interface) {
    // Basic interface name validation
    return !interface.empty() && interface.length() < IFNAMSIZ;
}

bool AF_PacketBackend::createSocket() {
    socketFd_ = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (socketFd_ < 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to create AF_PACKET socket: " + std::string(strerror(errno));
        return false;
    }
    return true;
}

bool AF_PacketBackend::bindToInterface() {
    struct sockaddr_ll addr = {};
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    
    // Get interface index
    struct ifreq ifr = {};
    strncpy(ifr.ifr_name, config_.interface.c_str(), IFNAMSIZ - 1);
    
    if (ioctl(socketFd_, SIOCGIFINDEX, &ifr) < 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to get interface index: " + std::string(strerror(errno));
        return false;
    }
    
    addr.sll_ifindex = ifr.ifr_ifindex;
    
    if (bind(socketFd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to bind to interface: " + std::string(strerror(errno));
        return false;
    }
    
    return true;
}

bool AF_PacketBackend::setSocketOptions() {
    // Set buffer size
    if (setsockopt(socketFd_, SOL_SOCKET, SO_RCVBUF, &bufferSize_, sizeof(bufferSize_)) < 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to set receive buffer size: " + std::string(strerror(errno));
        return false;
    }
    
    // Set non-blocking mode if requested
    if (!blockingMode_) {
        int flags = fcntl(socketFd_, F_GETFL, 0);
        if (flags < 0 || fcntl(socketFd_, F_SETFL, flags | O_NONBLOCK) < 0) {
            std::lock_guard<std::mutex> lock(errorMutex_);
            lastError_ = "Failed to set non-blocking mode: " + std::string(strerror(errno));
            return false;
        }
    }
    
    return true;
}

void AF_PacketBackend::packetProcessingLoop() {
    std::vector<uint8_t> buffer(bufferSize_);
    
    while (running_) {
        ssize_t bytesRead = recv(socketFd_, buffer.data(), buffer.size(), 0);
        
        if (bytesRead > 0) {
            // Create packet data
            auto dataPtr = std::make_shared<uint8_t[]>(bytesRead);
            std::copy(buffer.begin(), buffer.begin() + bytesRead, dataPtr.get());
            
            Packet packet(dataPtr, bytesRead);
            
            // Update statistics
            {
                std::lock_guard<std::mutex> lock(statsMutex_);
                stats_.packetsCaptured++;
                stats_.bytesCaptured += bytesRead;
                stats_.lastUpdate = std::chrono::steady_clock::now();
            }
            
            // Add to queue
            {
                std::lock_guard<std::mutex> lock(packetQueueMutex_);
                packetQueue_.push(packet);
                packetCondition_.notify_one();
            }
            
            // Call callback if set
            {
                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (packetCallback_) {
                    packetCallback_(packet);
                }
            }
        } else if (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // Error occurred
            std::lock_guard<std::mutex> lock(errorMutex_);
            lastError_ = "Error reading from socket: " + std::string(strerror(errno));
            break;
        }
        
        // Small delay to prevent busy waiting
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

void AF_PacketBackend::processPackets() {
    // This method is called from the processing thread
    // The actual processing is done in packetProcessingLoop
}

void AF_PacketBackend::shutdown() {
    stop();
    
    if (socketFd_ >= 0) {
        close(socketFd_);
        socketFd_ = -1;
    }
    
    // Free DMA buffers if allocated
    if (dmaBuffers_) {
        freeDMABuffers();
    }
    
    initialized_ = false;
}

// Zero-copy DMA access methods implementation
bool AF_PacketBackend::isZeroCopyEnabled() const {
    return zeroCopyEnabled_;
}

bool AF_PacketBackend::isDMAAccessEnabled() const {
    return dmaAccessEnabled_;
}

Result<void> AF_PacketBackend::enableZeroCopy(bool enabled) {
    if (running_) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, 
                                 "Cannot change zero-copy mode while running");
    }
    
    zeroCopyEnabled_ = enabled;
    BEATRICE_INFO("Zero-copy mode {}", enabled ? "enabled" : "disabled");
    return Result<void>::success();
}

Result<void> AF_PacketBackend::enableDMAAccess(bool enabled, const std::string& device) {
    if (running_) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, 
                                 "Cannot change DMA access while running");
    }
    
    if (enabled && !device.empty()) {
        dmaDevice_ = device;
        dmaAccessEnabled_ = true;
        BEATRICE_INFO("DMA access enabled for device: {}", device);
    } else {
        dmaAccessEnabled_ = false;
        dmaDevice_.clear();
        BEATRICE_INFO("DMA access disabled");
    }
    
    return Result<void>::success();
}

Result<void> AF_PacketBackend::setDMABufferSize(size_t size) {
    if (running_) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, 
                                 "Cannot change DMA buffer size while running");
    }
    
    if (size == 0) {
        // Auto-size based on socket buffer size
        dmaBufferSize_ = bufferSize_;
        BEATRICE_INFO("DMA buffer size set to auto ({} bytes)", dmaBufferSize_);
    } else {
        dmaBufferSize_ = size;
        BEATRICE_INFO("DMA buffer size set to {} bytes", size);
    }
    
    return Result<void>::success();
}

size_t AF_PacketBackend::getDMABufferSize() const {
    return dmaBufferSize_;
}

std::string AF_PacketBackend::getDMADevice() const {
    return dmaDevice_;
}

Result<void> AF_PacketBackend::allocateDMABuffers(size_t count) {
    if (!dmaAccessEnabled_) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, 
                                 "DMA access not enabled");
    }
    
    if (dmaBuffers_) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, 
                                 "DMA buffers already allocated");
    }
    
    try {
        // For AF_PACKET, DMA access is limited but we can implement memory-mapped buffers
        // Open DMA device if specified
        if (!dmaDevice_.empty()) {
            dmaFd_ = open(dmaDevice_.c_str(), O_RDWR);
            if (dmaFd_ < 0) {
                return Result<void>::error(ErrorCode::INITIALIZATION_FAILED, 
                                         "Failed to open DMA device: " + std::string(strerror(errno)));
            }
        }
        
        // Calculate total buffer size
        size_t totalSize = count * dmaBufferSize_;
        
        // Allocate memory-mapped buffers
        dmaBuffers_ = mmap(nullptr, totalSize, PROT_READ | PROT_WRITE, 
                           MAP_SHARED | MAP_LOCKED, dmaFd_ >= 0 ? dmaFd_ : -1, 0);
        
        if (dmaBuffers_ == MAP_FAILED) {
            if (dmaFd_ >= 0) {
                close(dmaFd_);
                dmaFd_ = -1;
            }
            return Result<void>::error(ErrorCode::INITIALIZATION_FAILED, 
                                     "Failed to allocate DMA buffers: " + std::string(strerror(errno)));
        }
        
        dmaBufferCount_ = count;
        BEATRICE_INFO("Allocated {} AF_PACKET DMA buffers ({} bytes total)", count, totalSize);
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        if (dmaFd_ >= 0) {
            close(dmaFd_);
            dmaFd_ = -1;
        }
        return Result<void>::error(ErrorCode::INITIALIZATION_FAILED, e.what());
    }
}

Result<void> AF_PacketBackend::freeDMABuffers() {
    if (!dmaBuffers_) {
        return Result<void>::success();
    }
    
    try {
        // Unmap DMA buffers
        size_t totalSize = dmaBufferCount_ * dmaBufferSize_;
        if (munmap(dmaBuffers_, totalSize) < 0) {
            BEATRICE_WARN("Failed to unmap DMA buffers: {}", strerror(errno));
        }
        
        // Close DMA device
        if (dmaFd_ >= 0) {
            close(dmaFd_);
            dmaFd_ = -1;
        }
        
        dmaBuffers_ = nullptr;
        dmaBufferCount_ = 0;
        BEATRICE_INFO("AF_PACKET DMA buffers freed successfully");
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        BEATRICE_ERROR("Exception during AF_PACKET DMA buffer cleanup: {}", e.what());
        return Result<void>::error(ErrorCode::CLEANUP_FAILED, e.what());
    }
}

} // namespace beatrice 