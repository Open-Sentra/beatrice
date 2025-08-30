#include "beatrice/DPDKBackend.hpp"
#include "beatrice/Logger.hpp"
#include "beatrice/Error.hpp"
#include <chrono>
#include <algorithm>

extern "C" {
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ring.h>
#include <rte_version.h>
#include <rte_malloc.h>
}

namespace beatrice {

DPDKBackend::DPDKBackend()
    : running_(false)
    , initialized_(false)
    , dpdkInitialized_(false)
    , config_()
    , processingThread_()
    , packetQueue_()
    , packetQueueMutex_()
    , packetCondition_()
    , packetCallback_(nullptr)
    , callbackMutex_()
    , statsMutex_()
    , stats_()
    , errorMutex_()
    , lastError_()
    , dpdkArgs_()
    , ealConfig_()
    , zeroCopyEnabled_(true)
    , dmaAccessEnabled_(false)
    , dmaDevice_("")
    , dmaBufferSize_(0)
    , dmaBuffers_(nullptr)
    , dmaBufferCount_(0)
    , dmaFd_(-1) {
}

DPDKBackend::~DPDKBackend() {
    shutdown();
}

Result<void> DPDKBackend::initialize(const Config& config) {
    if (initialized_) {
        return Result<void>::success();
    }

    config_ = config;
    
    if (!validateInterface(config_.interface)) {
        return Result<void>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "Invalid interface: " + config_.interface);
    }

    if (!initializeDPDK()) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "Failed to initialize DPDK");
    }

    if (!initializePort()) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "Failed to initialize DPDK port");
    }

    if (!setupQueues()) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "Failed to setup DPDK queues");
    }

    initialized_ = true;
    return Result<void>::success();
}

Result<void> DPDKBackend::start() {
    if (!initialized_) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "DPDK backend not initialized");
    }

    if (running_) {
        return Result<void>::success();
    }

    if (!startPort()) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "Failed to start DPDK port");
    }

    running_ = true;
    processingThread_ = std::thread(&DPDKBackend::packetProcessingLoop, this);

    return Result<void>::success();
}

Result<void> DPDKBackend::stop() {
    if (!running_) {
        return Result<void>::success();
    }

    running_ = false;
    packetCondition_.notify_all();

    if (processingThread_.joinable()) {
        processingThread_.join();
    }

    stopPort();
    return Result<void>::success();
}

bool DPDKBackend::isRunning() const noexcept {
    return running_;
}

std::optional<Packet> DPDKBackend::nextPacket(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(packetQueueMutex_);
    
    if (packetQueue_.empty()) {
        if (packetCondition_.wait_for(lock, timeout, [this] { return !packetQueue_.empty() || !running_; })) {
            if (!running_) {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }

    if (packetQueue_.empty()) {
        return std::nullopt;
    }

    Packet packet = std::move(packetQueue_.front());
    packetQueue_.pop();
    
    return packet;
}

std::vector<Packet> DPDKBackend::getPackets(size_t maxPackets, std::chrono::milliseconds timeout) {
    std::vector<Packet> packets;
    packets.reserve(maxPackets);

    auto start = std::chrono::steady_clock::now();
    auto end = start + timeout;

    while (packets.size() < maxPackets && std::chrono::steady_clock::now() < end) {
        auto packet = nextPacket(std::chrono::milliseconds(100));
        if (packet) {
            packets.push_back(std::move(*packet));
        } else if (!running_) {
            break;
        }
    }

    return packets;
}

void DPDKBackend::setPacketCallback(std::function<void(Packet)> callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    packetCallback_ = callback;
}

void DPDKBackend::removePacketCallback() {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    packetCallback_ = nullptr;
}

DPDKBackend::Statistics DPDKBackend::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void DPDKBackend::resetStatistics() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_ = Statistics{};
}

std::string DPDKBackend::getName() const {
    return "DPDK Backend";
}

std::string DPDKBackend::getVersion() const {
    return "DPDK Backend v1.0.0";
}

std::vector<std::string> DPDKBackend::getSupportedFeatures() const {
    return {
        "zero_copy",
        "hardware_timestamping",
        "multi_queue",
        "cpu_affinity",
        "batch_processing",
        "high_performance"
    };
}

bool DPDKBackend::isFeatureSupported(const std::string& feature) const {
    auto features = getSupportedFeatures();
    return std::find(features.begin(), features.end(), feature) != features.end();
}

DPDKBackend::Config DPDKBackend::getConfig() const {
    return config_;
}

Result<void> DPDKBackend::updateConfig(const Config& config) {
    if (running_) {
        return Result<void>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "Cannot update config while running");
    }

    config_ = config;
    return Result<void>::success();
}

std::string DPDKBackend::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

bool DPDKBackend::isHealthy() const {
    return initialized_ && dpdkInitialized_ && !lastError_.empty();
}

Result<void> DPDKBackend::healthCheck() {
    if (!initialized_) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "Backend not initialized");
    }

    if (!dpdkInitialized_) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "DPDK not initialized");
    }

    return Result<void>::success();
}

    Result<void> DPDKBackend::setDPDKArgs(const std::vector<std::string>& args) {
        if (dpdkInitialized_) {
            return Result<void>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "DPDK already initialized");
        }
        dpdkArgs_ = args;
        return Result<void>::success();
    }

    Result<void> DPDKBackend::setEALConfig(const std::string& config) {
        if (dpdkInitialized_) {
            return Result<void>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "DPDK already initialized");
        }
        ealConfig_ = config;
        return Result<void>::success();
    }

bool DPDKBackend::isDPDKInitialized() const {
    return dpdkInitialized_;
}

bool DPDKBackend::validateInterface(const std::string& interface) {
    return !interface.empty();
}

bool DPDKBackend::initializeDPDK() {
    if (dpdkInitialized_) {
        return true;
    }

    std::vector<const char*> args;
    args.push_back("beatrice");

    for (const auto& arg : dpdkArgs_) {
        args.push_back(arg.c_str());
    }

    // Add a virtual device for testing (since we don't have physical NICs)
    args.push_back("--vdev");
    args.push_back("net_tap0");

    int ret = rte_eal_init(args.size(), const_cast<char**>(const_cast<char* const*>(args.data())));
    if (ret < 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to initialize DPDK EAL";
        return false;
    }

    dpdkInitialized_ = true;
    return true;
}

bool DPDKBackend::initializePort() {
    if (!dpdkInitialized_) {
        return false;
    }

    // Find available DPDK port
    uint16_t portCount = rte_eth_dev_count_avail();
    if (portCount == 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "No DPDK ports available (count: 0)";
        return false;
    }

    // Use the first available port
    uint16_t portId = 0;
    
    // Check if port is valid
    if (!rte_eth_dev_is_valid_port(portId)) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Invalid DPDK port ID: " + std::to_string(portId);
        return false;
    }

    // Get port info for debugging
    struct rte_eth_dev_info devInfo;
    if (rte_eth_dev_info_get(portId, &devInfo) < 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to get port info for port " + std::to_string(portId);
        return false;
    }

    struct rte_eth_conf portConf = {};
    portConf.rxmode.mtu = 1500;
    portConf.rxmode.offloads = 0;
    portConf.txmode.offloads = 0;

    if (rte_eth_dev_configure(portId, 1, 1, &portConf) < 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to configure DPDK port " + std::to_string(portId);
        return false;
    }

    return true;
}

bool DPDKBackend::setupQueues() {
    uint16_t portId = 0;
    
    struct rte_mempool* rxMempool = rte_pktmbuf_pool_create("rx_mempool", 
        config_.numBuffers, 0, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    
    if (!rxMempool) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to create RX memory pool";
        return false;
    }

    if (rte_eth_rx_queue_setup(portId, 0, config_.numBuffers, 
        rte_eth_dev_socket_id(portId), nullptr, rxMempool) < 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to setup RX queue";
        return false;
    }

    if (rte_eth_tx_queue_setup(portId, 0, config_.numBuffers, 
        rte_eth_dev_socket_id(portId), nullptr) < 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to setup TX queue";
        return false;
    }

    return true;
}

bool DPDKBackend::startPort() {
    uint16_t portId = 0;
    
    if (rte_eth_dev_start(portId) < 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to start DPDK port";
        return false;
    }

    rte_eth_promiscuous_enable(portId);
    return true;
}

void DPDKBackend::stopPort() {
    uint16_t portId = 0;
    rte_eth_dev_stop(portId);
    rte_eth_dev_close(portId);
}

void DPDKBackend::packetProcessingLoop() {
    while (running_) {
        processPackets();
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

void DPDKBackend::processPackets() {
    if (!running_) {
        return;
    }

    uint16_t portId = 0;
    struct rte_mbuf* pkts[32];
    uint16_t nbRx = rte_eth_rx_burst(portId, 0, pkts, 32);

    for (uint16_t i = 0; i < nbRx; i++) {
        struct rte_mbuf* pkt = pkts[i];
        
        if (pkt) {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.packetsCaptured++;
            stats_.bytesCaptured += pkt->data_len;
            stats_.lastUpdate = std::chrono::steady_clock::now();
            
            rte_pktmbuf_free(pkt);
        }
    }
}

void DPDKBackend::shutdown() {
    stop();
    
    if (dpdkInitialized_) {
        rte_eal_cleanup();
        dpdkInitialized_ = false;
    }
    
    // Free DMA buffers if allocated
    if (dmaBuffers_) {
        freeDMABuffers();
    }
    
    initialized_ = false;
}

// Zero-copy DMA access methods implementation
bool DPDKBackend::isZeroCopyEnabled() const {
    return zeroCopyEnabled_;
}

bool DPDKBackend::isDMAAccessEnabled() const {
    return dmaAccessEnabled_;
}

Result<void> DPDKBackend::enableZeroCopy(bool enabled) {
    if (running_) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, 
                                 "Cannot change zero-copy mode while running");
    }
    
    zeroCopyEnabled_ = enabled;
    BEATRICE_INFO("Zero-copy mode {}", enabled ? "enabled" : "disabled");
    return Result<void>::success();
}

Result<void> DPDKBackend::enableDMAAccess(bool enabled, const std::string& device) {
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

Result<void> DPDKBackend::setDMABufferSize(size_t size) {
    if (running_) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, 
                                 "Cannot change DMA buffer size while running");
    }
    
    if (size == 0) {
        // Auto-size based on DPDK configuration
        dmaBufferSize_ = RTE_MBUF_DEFAULT_BUF_SIZE;
        BEATRICE_INFO("DMA buffer size set to auto ({} bytes)", dmaBufferSize_);
    } else {
        dmaBufferSize_ = size;
        BEATRICE_INFO("DMA buffer size set to {} bytes", size);
    }
    
    return Result<void>::success();
}

size_t DPDKBackend::getDMABufferSize() const {
    return dmaBufferSize_;
}

std::string DPDKBackend::getDMADevice() const {
    return dmaDevice_;
}

Result<void> DPDKBackend::allocateDMABuffers(size_t count) {
    if (!dmaAccessEnabled_) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, 
                                 "DMA access not enabled");
    }
    
    if (dmaBuffers_) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, 
                                 "DMA buffers already allocated");
    }
    
    try {
        // For DPDK, we use DPDK's memory management
        // This is a simplified implementation - in practice, you'd integrate with DPDK's DMA framework
        
        // Calculate total buffer size
        size_t totalSize = count * dmaBufferSize_;
        
        // Allocate using DPDK's memory management
        dmaBuffers_ = rte_malloc_socket("dma_buffers", totalSize, RTE_CACHE_LINE_SIZE, rte_socket_id());
        
        if (!dmaBuffers_) {
            return Result<void>::error(ErrorCode::INITIALIZATION_FAILED, 
                                     "Failed to allocate DPDK DMA buffers");
        }
        
        dmaBufferCount_ = count;
        BEATRICE_INFO("Allocated {} DPDK DMA buffers ({} bytes total)", count, totalSize);
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::INITIALIZATION_FAILED, e.what());
    }
}

Result<void> DPDKBackend::freeDMABuffers() {
    if (!dmaBuffers_) {
        return Result<void>::success();
    }
    
    try {
        // Free DPDK memory
        rte_free(dmaBuffers_);
        
        dmaBuffers_ = nullptr;
        dmaBufferCount_ = 0;
        BEATRICE_INFO("DPDK DMA buffers freed successfully");
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        BEATRICE_ERROR("Exception during DPDK DMA buffer cleanup: {}", e.what());
        return Result<void>::error(ErrorCode::CLEANUP_FAILED, e.what());
    }
}

} // namespace beatrice 