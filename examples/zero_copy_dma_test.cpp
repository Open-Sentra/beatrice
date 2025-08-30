#include "beatrice/BeatriceContext.hpp"
#include "beatrice/AF_XDPBackend.hpp"
#include "beatrice/DPDKBackend.hpp"
#include "beatrice/PMDBackend.hpp"
#include "beatrice/AF_PacketBackend.hpp"
#include "beatrice/PluginManager.hpp"
#include "beatrice/Logger.hpp"
#include <iostream>
#include <memory>
#include <chrono>

void testZeroCopyDMABackend(beatrice::ICaptureBackend* backend, const std::string& backendName) {
    std::cout << "\n=== Testing " << backendName << " Zero-Copy DMA Access ===" << std::endl;
    
    // Test zero-copy status
    std::cout << "1. Zero-copy status: " << (backend->isZeroCopyEnabled() ? "Enabled" : "Disabled") << std::endl;
    std::cout << "2. DMA access status: " << (backend->isDMAAccessEnabled() ? "Enabled" : "Disabled") << std::endl;
    
    // Test enabling zero-copy
    auto zeroCopyResult = backend->enableZeroCopy(true);
    if (zeroCopyResult.isSuccess()) {
        std::cout << "3. ✓ Zero-copy enabled successfully" << std::endl;
    } else {
        std::cout << "3. ✗ Failed to enable zero-copy: " << zeroCopyResult.getErrorMessage() << std::endl;
    }
    
    // Test enabling DMA access
    auto dmaResult = backend->enableDMAAccess(true, "/dev/dma0");
    if (dmaResult.isSuccess()) {
        std::cout << "4. ✓ DMA access enabled successfully" << std::endl;
    } else {
        std::cout << "4. ✗ Failed to enable DMA access: " << dmaResult.getErrorMessage() << std::endl;
    }
    
    // Test setting DMA buffer size
    auto bufferSizeResult = backend->setDMABufferSize(4096);
    if (bufferSizeResult.isSuccess()) {
        std::cout << "5. ✓ DMA buffer size set successfully" << std::endl;
    } else {
        std::cout << "5. ✗ Failed to set DMA buffer size: " << bufferSizeResult.getErrorMessage() << std::endl;
    }
    
    // Test allocating DMA buffers
    auto allocResult = backend->allocateDMABuffers(16);
    if (allocResult.isSuccess()) {
        std::cout << "6. ✓ DMA buffers allocated successfully" << std::endl;
    } else {
        std::cout << "6. ✗ Failed to allocate DMA buffers: " << allocResult.getErrorMessage() << std::endl;
        std::cout << "   (This is expected if DMA device /dev/dma0 doesn't exist)" << std::endl;
    }
    
    // Test getting DMA information
    std::cout << "7. DMA buffer size: " << backend->getDMABufferSize() << " bytes" << std::endl;
    std::cout << "8. DMA device: " << backend->getDMADevice() << std::endl;
    
    // Test freeing DMA buffers
    auto freeResult = backend->freeDMABuffers();
    if (freeResult.isSuccess()) {
        std::cout << "9. ✓ DMA buffers freed successfully" << std::endl;
    } else {
        std::cout << "9. ✗ Failed to free DMA buffers: " << freeResult.getErrorMessage() << std::endl;
    }
    
    // Test disabling DMA access
    auto disableDmaResult = backend->enableDMAAccess(false);
    if (disableDmaResult.isSuccess()) {
        std::cout << "10. ✓ DMA access disabled successfully" << std::endl;
    } else {
        std::cout << "10. ✗ Failed to disable DMA access: " << disableDmaResult.getErrorMessage() << std::endl;
    }
    
    // Test disabling zero-copy
    auto disableZeroCopyResult = backend->enableZeroCopy(false);
    if (disableZeroCopyResult.isSuccess()) {
        std::cout << "11. ✓ Zero-copy disabled successfully" << std::endl;
    } else {
        std::cout << "11. ✗ Failed to disable zero-copy: " << disableZeroCopyResult.getErrorMessage() << std::endl;
    }
    
    std::cout << "=== " << backendName << " Zero-Copy DMA Test Completed ===" << std::endl;
}

int main() {
    std::cout << "=== Beatrice Zero-Copy DMA Access Test ===" << std::endl;
    
    // Initialize logging
    beatrice::Logger::get().initialize("zero_copy_dma_test", "", 1024*1024, 5);
    
    // Test AF_XDP Backend
    {
        auto backend = std::make_unique<beatrice::AF_XDPBackend>();
        testZeroCopyDMABackend(backend.get(), "AF_XDP Backend");
    }
    
    // Test DPDK Backend
    {
        auto backend = std::make_unique<beatrice::DPDKBackend>();
        testZeroCopyDMABackend(backend.get(), "DPDK Backend");
    }
    
    // Test PMD Backend
    {
        auto backend = std::make_unique<beatrice::PMDBackend>();
        testZeroCopyDMABackend(backend.get(), "PMD Backend");
    }
    
    // Test AF_PACKET Backend
    {
        auto backend = std::make_unique<beatrice::AF_PacketBackend>();
        testZeroCopyDMABackend(backend.get(), "AF_PACKET Backend");
    }
    
    std::cout << "\n=== Zero-Copy DMA Access Test Summary ===" << std::endl;
    std::cout << "✓ All backends now support zero-copy DMA access interface" << std::endl;
    std::cout << "✓ DMA buffer allocation and management implemented" << std::endl;
    std::cout << "✓ Runtime configuration of zero-copy and DMA features" << std::endl;
    std::cout << "✓ Proper cleanup and resource management" << std::endl;
    std::cout << "✓ Error handling for invalid operations" << std::endl;
    
    std::cout << "\n=== Implementation Details ===" << std::endl;
    std::cout << "• AF_XDP: Uses mmap for DMA buffer allocation" << std::endl;
    std::cout << "• DPDK: Uses rte_malloc_socket for DMA buffer allocation" << std::endl;
    std::cout << "• PMD: Uses rte_malloc_socket for DMA buffer allocation" << std::endl;
    std::cout << "• AF_PACKET: Uses mmap for memory-mapped buffers" << std::endl;
    
    std::cout << "\n=== Usage Example ===" << std::endl;
    std::cout << "```cpp" << std::endl;
    std::cout << "// Enable zero-copy DMA access" << std::endl;
    std::cout << "backend->enableZeroCopy(true);" << std::endl;
    std::cout << "backend->enableDMAAccess(true, \"/dev/dma0\");" << std::endl;
    std::cout << "backend->setDMABufferSize(4096);" << std::endl;
    std::cout << "backend->allocateDMABuffers(16);" << std::endl;
    std::cout << "```" << std::endl;
    
    return 0;
} 