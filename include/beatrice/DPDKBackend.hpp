#ifndef BEATRICE_DPDKBACKEND_HPP
#define BEATRICE_DPDKBACKEND_HPP

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

class DPDKBackend : public ICaptureBackend {
public:
    DPDKBackend();
    ~DPDKBackend();
    
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

    Result<void> setDPDKArgs(const std::vector<std::string>& args);
    Result<void> setEALConfig(const std::string& config);
    bool isDPDKInitialized() const;

private:
    bool running_;
    bool initialized_;
    bool dpdkInitialized_;
    Config config_;
    
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
    
    std::vector<std::string> dpdkArgs_;
    std::string ealConfig_;

    // DMA and zero-copy members
    bool zeroCopyEnabled_;
    bool dmaAccessEnabled_;
    std::string dmaDevice_;
    size_t dmaBufferSize_;
    void* dmaBuffers_;
    size_t dmaBufferCount_;
    int dmaFd_;
    
    bool validateInterface(const std::string& interface);
    bool initializeDPDK();
    bool initializePort();
    bool setupQueues();
    bool startPort();
    void stopPort();
    void packetProcessingLoop();
    void processPackets();
    void shutdown();
    
    DPDKBackend(const DPDKBackend&) = delete;
    DPDKBackend& operator=(const DPDKBackend&) = delete;
};

} // namespace beatrice

#endif // BEATRICE_DPDKBACKEND_HPP 