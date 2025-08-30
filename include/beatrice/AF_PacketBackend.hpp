#ifndef BEATRICE_AF_PACKETBACKEND_HPP
#define BEATRICE_AF_PACKETBACKEND_HPP

#include "beatrice/ICaptureBackend.hpp"
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <vector>
#include <string>

namespace beatrice {

class AF_PacketBackend : public ICaptureBackend {
public:
    AF_PacketBackend();
    ~AF_PacketBackend();
    
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

    // Zero-copy DMA access methods
    bool isZeroCopyEnabled() const override;
    bool isDMAAccessEnabled() const override;
    Result<void> enableZeroCopy(bool enabled) override;
    Result<void> enableDMAAccess(bool enabled, const std::string& device = "") override;
    Result<void> setDMABufferSize(size_t size) override;
    size_t getDMABufferSize() const override;
    std::string getDMADevice() const override;
    Result<void> allocateDMABuffers(size_t count) override;
    Result<void> freeDMABuffers() override;

    // AF_PACKET specific methods
    Result<void> setPromiscuousMode(bool enabled);
    Result<void> setBufferSize(size_t size);
    Result<void> setBlockingMode(bool blocking);
    bool isPromiscuousMode() const;
    size_t getBufferSize() const;
    bool isBlockingMode() const;

private:
    bool running_;
    bool initialized_;
    Config config_;
    
    // AF_PACKET specific members
    int socketFd_;
    bool promiscuousMode_;
    size_t bufferSize_;
    bool blockingMode_;

    // DMA and zero-copy members
    bool zeroCopyEnabled_;
    bool dmaAccessEnabled_;
    std::string dmaDevice_;
    size_t dmaBufferSize_;
    void* dmaBuffers_;
    size_t dmaBufferCount_;
    int dmaFd_;
    
    std::thread processingThread_;
    
    std::queue<Packet> packetQueue_;
    std::mutex packetQueueMutex_;
    std::condition_variable packetCondition_;
    
    std::function<void(Packet)> packetCallback_;
    std::mutex callbackMutex_;
    
    mutable std::mutex statsMutex_;
    Statistics stats_;
    
    mutable std::mutex errorMutex_;
    std::string lastError_;
    
    bool validateInterface(const std::string& interface);
    bool createSocket();
    bool bindToInterface();
    bool setSocketOptions();
    void packetProcessingLoop();
    void processPackets();
    void shutdown();
    
    AF_PacketBackend(const AF_PacketBackend&) = delete;
    AF_PacketBackend& operator=(const AF_PacketBackend&) = delete;
};

} // namespace beatrice

#endif // BEATRICE_AF_PACKETBACKEND_HPP 