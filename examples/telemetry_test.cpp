#include "beatrice/Telemetry.hpp"
#include "beatrice/Logger.hpp"
#include "beatrice/Metrics.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

using namespace beatrice;

void testBasicTelemetry() {
    std::cout << "=== Testing Basic Telemetry ===" << std::endl;
    
    // Set telemetry level
    telemetry::setLevel(TelemetryLevel::STANDARD);
    
    // Collect basic events
    telemetry::collectEvent(TelemetryEvent::EventType::PACKET_RECEIVED, "test_packet", "Test packet received");
    telemetry::collectEvent(TelemetryEvent::EventType::PACKET_PROCESSED, "test_packet", "Test packet processed");
    
    // Collect metrics
    telemetry::collectMetric("cpu_usage", 75.5, "CPU usage percentage");
    telemetry::collectMetric("memory_usage", 1024.0, "Memory usage in MB");
    
    // Collect counters
    telemetry::collectCounter("packets_total", 1000, "Total packets processed");
    
    std::cout << "Basic telemetry test completed" << std::endl;
}

void testPerformanceMonitoring() {
    std::cout << "\n=== Testing Performance Monitoring ===" << std::endl;
    
    // Start performance measurement
    telemetry::startPerformanceMeasurement("packet_processing");
    
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // End performance measurement
    telemetry::endPerformanceMeasurement("packet_processing");
    
    // Get average performance
    double avgPerformance = telemetry::getAveragePerformance("packet_processing");
    std::cout << "Average packet processing time: " << avgPerformance << " microseconds" << std::endl;
    
    std::cout << "Performance monitoring test completed" << std::endl;
}

void testTracing() {
    std::cout << "\n=== Testing Tracing ===" << std::endl;
    
    // Start trace
    telemetry::startTrace("packet_flow");
    
    // Simulate packet processing steps
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // End trace
    telemetry::endTrace("packet_flow");
    
    std::cout << "Tracing test completed" << std::endl;
}

void testHealthMonitoring() {
    std::cout << "\n=== Testing Health Monitoring ===" << std::endl;
    
    // Report health status
    telemetry::reportHealth("network_interface", true, "Interface is healthy");
    telemetry::reportHealth("packet_processor", true, "Processor is running normally");
    telemetry::reportHealth("memory_manager", false, "Memory usage is high");
    
    // Check health status
    std::cout << "Network interface health: " << (telemetry::isHealthy("network_interface") ? "OK" : "FAIL") << std::endl;
    std::cout << "Packet processor health: " << (telemetry::isHealthy("packet_processor") ? "OK" : "FAIL") << std::endl;
    std::cout << "Memory manager health: " << (telemetry::isHealthy("memory_manager") ? "OK" : "FAIL") << std::endl;
    
    std::cout << "Health monitoring test completed" << std::endl;
}

void testContextAndLabels() {
    std::cout << "\n=== Testing Context and Labels ===" << std::endl;
    
    // Set context
    telemetry::setContext("session_id", "test_session_123");
    telemetry::setContext("user_id", "test_user");
    
    // Get context
    std::cout << "Session ID: " << telemetry::getContext("session_id") << std::endl;
    std::cout << "User ID: " << telemetry::getContext("user_id") << std::endl;
    
    std::cout << "Context and labels test completed" << std::endl;
}

void testCustomBackend() {
    std::cout << "\n=== Testing Custom Backend ===" << std::endl;
    
    // Set custom backend
    telemetry::setCustomBackend([](const TelemetryEvent& event) {
        std::cout << "Custom backend received event: " << event.getName() 
                  << " (Type: " << static_cast<int>(event.getType()) << ")" << std::endl;
        
        // Print event details
        std::cout << "  Description: " << event.getDescription() << std::endl;
        std::cout << "  Duration: " << event.getDuration().count() << " microseconds" << std::endl;
        
        // Print labels
        if (!event.getLabels().empty()) {
            std::cout << "  Labels: ";
            for (const auto& [key, value] : event.getLabels()) {
                std::cout << key << "=" << value << " ";
            }
            std::cout << std::endl;
        }
        
        // Print metrics
        if (!event.getMetrics().empty()) {
            std::cout << "  Metrics: ";
            for (const auto& [key, value] : event.getMetrics()) {
                std::cout << key << "=" << value << " ";
            }
            std::cout << std::endl;
        }
    });
    
    // Collect some events to trigger custom backend
    telemetry::collectEvent(TelemetryEvent::EventType::CUSTOM, "custom_test", "Testing custom backend");
    telemetry::collectMetric("custom_metric", 42.0, "Custom metric value");
    
    std::cout << "Custom backend test completed" << std::endl;
}

void testTelemetrySpan() {
    std::cout << "\n=== Testing Telemetry Span ===" << std::endl;
    
    // Create a span
    TelemetrySpan span("packet_processing_span", "Processing a network packet");
    
    // Add labels and metrics
    span.addLabel("packet_type", "TCP");
    span.addLabel("source_ip", "192.168.1.100");
    span.addLabel("destination_ip", "192.168.1.200");
    span.addMetric("packet_size", 1500.0);
    span.addTag("priority", "high");
    
    // Simulate some work
    std::this_thread::sleep_for(std::chrono::milliseconds(75));
    
    // Set status and complete
    span.setStatus(true, "Packet processed successfully");
    
    std::cout << "Telemetry span test completed" << std::endl;
}

void testAutoSpan() {
    std::cout << "\n=== Testing Auto Span ===" << std::endl;
    
    // Use the convenience macro
    TELEMETRY_SPAN("auto_packet_processing", "Automatic packet processing span");
    
    // Add some context
    TELEMETRY_SPAN_LABEL("packet_type", "UDP");
    TELEMETRY_SPAN_METRIC("packet_size", 512.0);
    TELEMETRY_SPAN_TAG("priority", "normal");
    
    // Simulate work
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    
    // Set status
    TELEMETRY_SPAN_STATUS(true, "Auto span completed successfully");
    
    std::cout << "Auto span test completed" << std::endl;
}

void testMetricsIntegration() {
    std::cout << "\n=== Testing Metrics Integration ===" << std::endl;
    
    // Create some metrics
    auto packetCounter = metrics::counter("test_packets_total", "Total test packets");
    auto latencyGauge = metrics::gauge("test_latency_ms", "Test latency in milliseconds");
    auto sizeHistogram = metrics::histogram("test_packet_size", "Test packet size distribution");
    
    // Update metrics
    packetCounter->increment(10);
    latencyGauge->set(15.5);
    sizeHistogram->observe(1500.0);
    sizeHistogram->observe(512.0);
    sizeHistogram->observe(1024.0);
    
    // Export metrics
    std::cout << "Prometheus metrics:" << std::endl;
    std::cout << telemetry::exportMetrics(TelemetryBackend::PROMETHEUS) << std::endl;
    
    std::cout << "JSON metrics:" << std::endl;
    std::cout << telemetry::exportMetrics(TelemetryBackend::CUSTOM) << std::endl;
    
    std::cout << "Metrics integration test completed" << std::endl;
}

void testExportFunctions() {
    std::cout << "\n=== Testing Export Functions ===" << std::endl;
    
    // Export different formats
    std::cout << "Events export:" << std::endl;
    std::cout << telemetry::exportEvents() << std::endl;
    
    std::cout << "Health export:" << std::endl;
    std::cout << telemetry::exportHealth() << std::endl;
    
    std::cout << "Export functions test completed" << std::endl;
}

void testStressTest() {
    std::cout << "\n=== Testing Stress Test ===" << std::endl;
    
    const int numEvents = 1000;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(1.0, 1000.0);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numEvents; ++i) {
        telemetry::collectEvent(TelemetryEvent::EventType::PACKET_PROCESSED, 
                               "stress_test_packet_" + std::to_string(i), 
                               "Stress test packet");
        telemetry::collectMetric("stress_metric_" + std::to_string(i), dis(gen), "Stress test metric");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Processed " << numEvents << " events in " << duration.count() << " ms" << std::endl;
    std::cout << "Rate: " << (numEvents * 1000.0 / duration.count()) << " events/second" << std::endl;
    
    std::cout << "Stress test completed" << std::endl;
}

int main() {
    std::cout << "Beatrice Telemetry Test Suite" << std::endl;
    std::cout << "=============================" << std::endl;
    
    try {
        // Run all tests
        testBasicTelemetry();
        testPerformanceMonitoring();
        testTracing();
        testHealthMonitoring();
        testContextAndLabels();
        testCustomBackend();
        testTelemetrySpan();
        testAutoSpan();
        testMetricsIntegration();
        testExportFunctions();
        testStressTest();
        
        // Flush telemetry data
        telemetry::flush();
        
        std::cout << "\n=== All Tests Completed Successfully ===" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 