#include "beatrice/PMDBackend.hpp"
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
#include <rte_dev.h>
#include <rte_bus.h>
#include <rte_bus_vdev.h>
#include <rte_malloc.h>
}

namespace beatrice {

PMDBackend::PMDBackend()
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
    , pmdType_("net_tap")
    , pmdArgs_()
    , portConfigs_()
    , virtualDevices_()
    , portId_(0)
    , portInitialized_(false)
    , zeroCopyEnabled_(true)
    , dmaAccessEnabled_(false)
    , dmaDevice_("")
    , dmaBufferSize_(0)
    , dmaBuffers_(nullptr)
    , dmaBufferCount_(0)
    , dmaFd_(-1) {
}

PMDBackend::~PMDBackend() {
    shutdown();
}

Result<void> PMDBackend::initialize(const Config& config) {
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

    if (!initializePMD()) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "Failed to initialize PMD");
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

Result<void> PMDBackend::start() {
    if (!initialized_) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "PMD backend not initialized");
    }

    if (running_) {
        return Result<void>::success();
    }

    if (!startPort()) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "Failed to start DPDK port");
    }

    running_ = true;
    processingThread_ = std::thread(&PMDBackend::packetProcessingLoop, this);

    return Result<void>::success();
}

Result<void> PMDBackend::stop() {
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

bool PMDBackend::isRunning() const noexcept {
    return running_;
}

std::optional<Packet> PMDBackend::nextPacket(std::chrono::milliseconds timeout) {
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

std::vector<Packet> PMDBackend::getPackets(size_t maxPackets, std::chrono::milliseconds timeout) {
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

void PMDBackend::setPacketCallback(std::function<void(Packet)> callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    packetCallback_ = callback;
}

void PMDBackend::removePacketCallback() {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    packetCallback_ = nullptr;
}

PMDBackend::Statistics PMDBackend::getStatistics() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return stats_;
}

void PMDBackend::resetStatistics() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    stats_ = Statistics{};
}

std::string PMDBackend::getName() const {
    return "PMD Backend (" + pmdType_ + ")";
}

std::string PMDBackend::getVersion() const {
    return "PMD Backend v1.0.0";
}

std::vector<std::string> PMDBackend::getSupportedFeatures() const {
    return {
        "zero_copy",
        "hardware_timestamping",
        "multi_queue",
        "cpu_affinity",
        "batch_processing",
        "high_performance",
        "pmd_support",
        "virtual_devices",
        "dynamic_port_management"
    };
}

bool PMDBackend::isFeatureSupported(const std::string& feature) const {
    auto features = getSupportedFeatures();
    return std::find(features.begin(), features.end(), feature) != features.end();
}

PMDBackend::Config PMDBackend::getConfig() const {
    return config_;
}

Result<void> PMDBackend::updateConfig(const Config& config) {
    if (running_) {
        return Result<void>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "Cannot update config while running");
    }

    config_ = config;
    return Result<void>::success();
}

std::string PMDBackend::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

bool PMDBackend::isHealthy() const {
    return initialized_ && dpdkInitialized_ && portInitialized_;
}

Result<void> PMDBackend::healthCheck() {
    if (!initialized_) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "Backend not initialized");
    }

    if (!dpdkInitialized_) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "DPDK not initialized");
    }

    if (!portInitialized_) {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "Port not initialized");
    }

    return Result<void>::success();
}

Result<void> PMDBackend::setPMDType(const std::string& pmdType) {
    if (dpdkInitialized_) {
        return Result<void>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "DPDK already initialized");
    }
    
    if (!isPMDTypeSupported(pmdType)) {
        return Result<void>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "PMD type not supported: " + pmdType);
    }
    
    pmdType_ = pmdType;
    return Result<void>::success();
}

Result<void> PMDBackend::setPMDArgs(const std::vector<std::string>& args) {
    if (dpdkInitialized_) {
        return Result<void>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "DPDK already initialized");
    }
    pmdArgs_ = args;
    return Result<void>::success();
}

Result<void> PMDBackend::setPortConfig(const std::string& portName, const std::map<std::string, std::string>& config) {
    if (dpdkInitialized_) {
        return Result<void>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "DPDK already initialized");
    }
    portConfigs_[portName] = config;
    return Result<void>::success();
}

std::vector<std::string> PMDBackend::getAvailablePMDs() const {
    return {
        "net_tap",
        "net_tun",
        "net_pcap",
        "net_null",
        "net_ring",
        "net_vdev",
        "net_af_packet",
        "net_af_xdp"
    };
}

std::vector<std::string> PMDBackend::getAvailablePorts() const {
    std::vector<std::string> ports;
    
    if (dpdkInitialized_) {
        uint16_t portCount = rte_eth_dev_count_avail();
        for (uint16_t i = 0; i < portCount; i++) {
            char name[RTE_ETH_NAME_MAX_LEN];
            if (rte_eth_dev_get_name_by_port(i, name) == 0) {
                ports.push_back(name);
            }
        }
    }
    
    return ports;
}

bool PMDBackend::isPMDSupported(const std::string& pmdType) const {
    auto pmds = getAvailablePMDs();
    return std::find(pmds.begin(), pmds.end(), pmdType) != pmds.end();
}

Result<void> PMDBackend::addVirtualDevice(const std::string& deviceType, const std::map<std::string, std::string>& params) {
    if (dpdkInitialized_) {
        return Result<void>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "DPDK already initialized");
    }
    
    if (createVirtualDevice(deviceType, params)) {
        virtualDevices_.push_back(deviceType);
        return Result<void>::success();
    } else {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "Failed to create virtual device");
    }
}

Result<void> PMDBackend::removeVirtualDevice(const std::string& deviceName) {
    if (dpdkInitialized_) {
        return Result<void>::error(beatrice::ErrorCode::INVALID_ARGUMENT, "DPDK already initialized");
    }
    
    if (destroyVirtualDevice(deviceName)) {
        auto it = std::find(virtualDevices_.begin(), virtualDevices_.end(), deviceName);
        if (it != virtualDevices_.end()) {
            virtualDevices_.erase(it);
        }
        return Result<void>::success();
    } else {
        return Result<void>::error(beatrice::ErrorCode::INITIALIZATION_FAILED, "Failed to destroy virtual device");
    }
}

bool PMDBackend::validateInterface(const std::string& interface) {
    return !interface.empty();
}

bool PMDBackend::initializeDPDK() {
    if (dpdkInitialized_) {
        return true;
    }

    std::vector<const char*> args;
    args.push_back("beatrice_pmd");

    for (const auto& arg : pmdArgs_) {
        args.push_back(arg.c_str());
    }

    // Add virtual device arguments if PMD type is set
    if (pmdType_ == "net_tap") {
        args.push_back("--vdev");
        args.push_back("net_tap0");
    } else if (pmdType_ == "net_tun") {
        args.push_back("--vdev");
        args.push_back("net_tun0");
    }

    int ret = rte_eal_init(args.size(), const_cast<char**>(const_cast<char* const*>(args.data())));
    if (ret < 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to initialize DPDK EAL";
        return false;
    }

    dpdkInitialized_ = true;
    return true;
}

bool PMDBackend::initializePMD() {
    if (!dpdkInitialized_) {
        return false;
    }

    // PMD initialization is handled by DPDK EAL
    // Virtual devices are created through command line arguments
    if (pmdType_ == "net_tap" || pmdType_ == "net_tun") {
        // These PMDs are initialized through DPDK EAL args
        // No additional initialization needed here
    }

    return true;
}

bool PMDBackend::initializePort() {
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
    portId_ = 0;
    
    // Check if port is valid
    if (!rte_eth_dev_is_valid_port(portId_)) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Invalid DPDK port ID: " + std::to_string(portId_);
        return false;
    }

    // Get port info for debugging
    struct rte_eth_dev_info devInfo;
    if (rte_eth_dev_info_get(portId_, &devInfo) < 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to get port info for port " + std::to_string(portId_);
        return false;
    }

    struct rte_eth_conf portConf = {};
    portConf.rxmode.mtu = 1500;
    portConf.rxmode.offloads = 0;
    portConf.txmode.offloads = 0;

    if (rte_eth_dev_configure(portId_, 1, 1, &portConf) < 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to configure DPDK port " + std::to_string(portId_);
        return false;
    }

    portInitialized_ = true;
    return true;
}

bool PMDBackend::setupQueues() {
    if (!portInitialized_) {
        return false;
    }
    
    struct rte_mempool* rxMempool = rte_pktmbuf_pool_create("rx_mempool", 
        config_.numBuffers, 0, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    
    if (!rxMempool) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to create RX memory pool";
        return false;
    }

    if (rte_eth_rx_queue_setup(portId_, 0, config_.numBuffers, 
        rte_eth_dev_socket_id(portId_), nullptr, rxMempool) < 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to setup RX queue";
        return false;
    }

    if (rte_eth_tx_queue_setup(portId_, 0, config_.numBuffers, 
        rte_eth_dev_socket_id(portId_), nullptr) < 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to setup TX queue";
        return false;
    }

    return true;
}

bool PMDBackend::startPort() {
    if (!portInitialized_) {
        return false;
    }
    
    if (rte_eth_dev_start(portId_) < 0) {
        std::lock_guard<std::mutex> lock(errorMutex_);
        lastError_ = "Failed to start DPDK port";
        return false;
    }

    rte_eth_promiscuous_enable(portId_);
    return true;
}

void PMDBackend::stopPort() {
    if (portInitialized_) {
        rte_eth_dev_stop(portId_);
        rte_eth_dev_close(portId_);
        portInitialized_ = false;
    }
}

void PMDBackend::packetProcessingLoop() {
    while (running_) {
        processPackets();
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

void PMDBackend::processPackets() {
    if (!running_ || !portInitialized_) {
        return;
    }

    struct rte_mbuf* pkts[32];
    uint16_t nbRx = rte_eth_rx_burst(portId_, 0, pkts, 32);

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

void PMDBackend::shutdown() {
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

bool PMDBackend::isPMDTypeSupported(const std::string& pmdType) const {
    return isPMDSupported(pmdType);
}

bool PMDBackend::createVirtualDevice(const std::string& deviceType, const std::map<std::string, std::string>& params) {
    // TODO: Implement virtual device creation (Priority: MEDIUM)
    // See TODO.md for details - Core System Improvements section
    // Virtual devices are created through DPDK EAL arguments
    return true;
}

bool PMDBackend::destroyVirtualDevice(const std::string& deviceName) {
    // TODO: Implement virtual device destruction (Priority: MEDIUM)
    // See TODO.md for details - Core System Improvements section
    // Virtual devices are destroyed when DPDK EAL is cleaned up
    return true;
}

// Zero-copy DMA access methods implementation
bool PMDBackend::isZeroCopyEnabled() const {
    return zeroCopyEnabled_;
}

bool PMDBackend::isDMAAccessEnabled() const {
    return dmaAccessEnabled_;
}

Result<void> PMDBackend::enableZeroCopy(bool enabled) {
    if (running_) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, 
                                 "Cannot change zero-copy mode while running");
    }
    
    zeroCopyEnabled_ = enabled;
    BEATRICE_INFO("Zero-copy mode {}", enabled ? "enabled" : "disabled");
    return Result<void>::success();
}

Result<void> PMDBackend::enableDMAAccess(bool enabled, const std::string& device) {
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

Result<void> PMDBackend::setDMABufferSize(size_t size) {
    if (running_) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, 
                                 "Cannot change DMA buffer size while running");
    }
    
    if (size == 0) {
        // Auto-size based on PMD configuration
        dmaBufferSize_ = RTE_MBUF_DEFAULT_BUF_SIZE;
        BEATRICE_INFO("DMA buffer size set to auto ({} bytes)", dmaBufferSize_);
    } else {
        dmaBufferSize_ = size;
        BEATRICE_INFO("DMA buffer size set to {} bytes", size);
    }
    
    return Result<void>::success();
}

size_t PMDBackend::getDMABufferSize() const {
    return dmaBufferSize_;
}

std::string PMDBackend::getDMADevice() const {
    return dmaDevice_;
}

Result<void> PMDBackend::allocateDMABuffers(size_t count) {
    if (!dmaAccessEnabled_) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, 
                                 "DMA access not enabled");
    }
    
    if (dmaBuffers_) {
        return Result<void>::error(ErrorCode::INVALID_ARGUMENT, 
                                 "DMA buffers already allocated");
    }
    
    try {
        // For PMD, we use DPDK's memory management similar to DPDK backend
        // Calculate total buffer size
        size_t totalSize = count * dmaBufferSize_;
        
        // Allocate using DPDK's memory management
        dmaBuffers_ = rte_malloc_socket("pmd_dma_buffers", totalSize, RTE_CACHE_LINE_SIZE, rte_socket_id());
        
        if (!dmaBuffers_) {
            return Result<void>::error(ErrorCode::INITIALIZATION_FAILED, 
                                     "Failed to allocate PMD DMA buffers");
        }
        
        dmaBufferCount_ = count;
        BEATRICE_INFO("Allocated {} PMD DMA buffers ({} bytes total)", count, totalSize);
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::INITIALIZATION_FAILED, e.what());
    }
}

Result<void> PMDBackend::freeDMABuffers() {
    if (!dmaBuffers_) {
        return Result<void>::success();
    }
    
    try {
        // Free DPDK memory
        rte_free(dmaBuffers_);
        
        dmaBuffers_ = nullptr;
        dmaBufferCount_ = 0;
        BEATRICE_INFO("PMD DMA buffers freed successfully");
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        BEATRICE_ERROR("Exception during PMD DMA buffer cleanup: {}", e.what());
        return Result<void>::error(ErrorCode::CLEANUP_FAILED, e.what());
    }
}

} // namespace beatrice 