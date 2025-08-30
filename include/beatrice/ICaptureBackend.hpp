#ifndef BEATRICE_ICAPTUREBACKEND_HPP
#define BEATRICE_ICAPTUREBACKEND_HPP

#include "Packet.hpp"
#include "Error.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <chrono>

namespace beatrice {

class ICaptureBackend {
public:
    virtual ~ICaptureBackend() = default;

    struct Config {
        std::string interface;           ///< Network interface name
        size_t bufferSize = 4096;        ///< Buffer size in bytes
        size_t numBuffers = 1024;        ///< Number of buffers
        bool promiscuous = true;         ///< Enable promiscuous mode
        int timeout = 1000;              ///< Timeout in milliseconds
        size_t batchSize = 64;           ///< Batch size for processing
        bool enableTimestamping = true;  ///< Enable hardware timestamping
        std::vector<int> cpuAffinity;    ///< CPU affinity for capture threads
        bool enableZeroCopy = true;      ///< Enable zero-copy mode
        size_t maxPacketSize = 65535;    ///< Maximum packet size
    };

    struct Statistics {
        uint64_t packetsCaptured = 0;    ///< Total packets captured
        uint64_t packetsDropped = 0;     ///< Total packets dropped
        uint64_t bytesCaptured = 0;      ///< Total bytes captured
        uint64_t bytesDropped = 0;       ///< Total bytes dropped
        double captureRate = 0.0;        ///< Packets per second
        double dropRate = 0.0;           ///< Drop rate percentage
        std::chrono::steady_clock::time_point lastUpdate; ///< Last statistics update
    };

    virtual Result<void> initialize(const Config& config) = 0;
    virtual Result<void> start() = 0;
    virtual Result<void> stop() = 0;
    virtual bool isRunning() const noexcept = 0;
    virtual std::optional<Packet> nextPacket(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) = 0;
    virtual std::vector<Packet> getPackets(size_t maxPackets = 64, 
                                          std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) = 0;
    virtual void setPacketCallback(std::function<void(Packet)> callback) = 0;
    virtual void removePacketCallback() = 0;

    virtual Statistics getStatistics() const = 0;
    virtual void resetStatistics() = 0;

    virtual std::string getName() const = 0;
    virtual std::string getVersion() const = 0;
    virtual std::vector<std::string> getSupportedFeatures() const = 0;
    virtual bool isFeatureSupported(const std::string& feature) const = 0;
    virtual Config getConfig() const = 0;
    virtual Result<void> updateConfig(const Config& config) = 0;
    virtual std::string getLastError() const = 0;
    virtual bool isHealthy() const = 0;
    virtual Result<void> healthCheck() = 0;

protected:
    ICaptureBackend() = default;

    ICaptureBackend(const ICaptureBackend&) = default;

    ICaptureBackend(ICaptureBackend&&) noexcept = default;

    ICaptureBackend& operator=(const ICaptureBackend&) = default;

    /**
     * @brief Protected move assignment operator
     */
    ICaptureBackend& operator=(ICaptureBackend&&) noexcept = default;
};

} // namespace beatrice

#endif // BEATRICE_ICAPTUREBACKEND_HPP
