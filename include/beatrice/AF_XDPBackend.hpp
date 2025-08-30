#ifndef BEATRICE_AF_XDPBACKEND_HPP
#define BEATRICE_AF_XDPBACKEND_HPP

#include "beatrice/ICaptureBackend.hpp"
#include "beatrice/XDPLoader.hpp"
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

// Forward declarations for Linux kernel structures
struct xdp_ring;
struct xdp_umem_reg;
struct xdp_mmap_offsets;

namespace beatrice {

class AF_XDPBackend : public ICaptureBackend {
public:
    AF_XDPBackend();
    ~AF_XDPBackend();
    
    // ICaptureBackend interface implementation
    Result<void> initialize(const Config& config) override;
    Result<void> start() override;
    Result<void> stop() override;
    bool isRunning() const noexcept override;
    std::optional<Packet> nextPacket(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) override;
    std::vector<Packet> getPackets(size_t maxPackets = 64, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) override;
    void setPacketCallback(std::function<void(Packet)> callback) override;
    void removePacketCallback() override;
    Statistics getStatistics() const override;
    void resetStatistics() override;
    std::string getName() const override;
    std::string getVersion() const override;
    std::vector<std::string> getSupportedFeatures() const override;
    bool isFeatureSupported(const std::string& feature) const override;
    Config getConfig() const override;
    Result<void> updateConfig(const Config& config) override;
    std::string getLastError() const override;
    bool isHealthy() const override;
    Result<void> healthCheck() override;

    // XDP-specific methods
    /**
     * @brief Load XDP program from file
     * @param programPath Path to XDP program file
     * @param programName Name of the program
     * @param xdpMode XDP mode (driver/skb/generic)
     * @return Result indicating success or failure
     */
    Result<void> loadXdpProgram(const std::string& programPath, 
                               const std::string& programName,
                               const std::string& xdpMode = "driver");
    Result<void> unloadXdpProgram();
    bool isXdpProgramLoaded() const;
    std::string getXdpProgramStats() const;

private:
    // XDP-specific members
    int socket_;
    void* umem_;
    size_t umemSize_;
    struct xdp_ring* fillQueue_;
    struct xdp_ring* completionQueue_;
    struct xdp_ring* rxQueue_;
    struct xdp_ring* txQueue_;
    uint32_t idx_;
    
    // XDP program loader
    std::unique_ptr<XDPLoader> xdpLoader_;
    std::string xdpProgramName_;
    bool xdpProgramLoaded_;
    
    // State management
    bool running_;
    bool initialized_;
    Config config_;
    
    // Threading
    std::thread processingThread_;
    
    // Packet queue
    std::queue<Packet> packetQueue_;
    std::mutex packetQueueMutex_;
    std::condition_variable packetCondition_;
    
    // Callback management
    std::function<void(Packet)> packetCallback_;
    std::mutex callbackMutex_;
    
    // Backend-specific metrics
    mutable std::mutex statsMutex_;
    Statistics stats_;
    
    // Error handling
    mutable std::mutex errorMutex_;
    std::string lastError_;
    
    // Private implementation methods
    bool validateInterface(const std::string& interface);
    bool initializeRealMode();  // Initialize real AF_XDP mode after XDP program is attached
    
    // Get interface index using ioctl
    int getInterfaceIndex(const std::string& interfaceName);
    bool initializeUmem();
    bool createSocket();
    bool bindToInterface();
    bool initializeRingBuffers();
    
    // Packet processing
    void packetProcessingLoop();
    void processRxQueue();
    void processCompletionQueue();
    void refillQueue();
    Packet parsePacketMetadata(const uint8_t* data, size_t length);
    
    // Test packet generation (stub mode)
    void generateTestPackets();
    Packet createTestPacket();
    
    // Cleanup
    void shutdown();
    
    // Disable copying
    AF_XDPBackend(const AF_XDPBackend&) = delete;
    AF_XDPBackend& operator=(const AF_XDPBackend&) = delete;
};

} // namespace beatrice

#endif // BEATRICE_AF_XDPBACKEND_HPP
