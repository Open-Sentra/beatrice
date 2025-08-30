#ifndef BEATRICE_TELEMETRY_HPP
#define BEATRICE_TELEMETRY_HPP

#include "beatrice/Metrics.hpp"
#include "beatrice/Logger.hpp"
#include <string>
#include <chrono>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <queue>
#include <condition_variable>
#include <nlohmann/json.hpp>

namespace beatrice {

enum class TelemetryLevel {
    BASIC,      // Basic metrics only
    STANDARD,   // Standard metrics + performance
    ADVANCED,   // Advanced metrics + detailed performance
    DEBUG       // Debug metrics + full tracing
};

enum class TelemetryBackend {
    PROMETHEUS,     // Prometheus metrics
    INFLUXDB,       // InfluxDB time series
    JAEGER,         // Jaeger tracing
    CUSTOM          // Custom backend
};

class TelemetryEvent {
public:
    enum class EventType {
        PACKET_RECEIVED,
        PACKET_PROCESSED,
        PACKET_DROPPED,
        BACKEND_INITIALIZED,
        BACKEND_ERROR,
        PLUGIN_LOADED,
        PLUGIN_ERROR,
        PERFORMANCE_MEASUREMENT,
        SYSTEM_HEALTH_CHECK,
        CUSTOM
    };

    TelemetryEvent(EventType type, const std::string& name, const std::string& description = "");
    
    void addLabel(const std::string& key, const std::string& value);
    void addMetric(const std::string& key, double value);
    void addTag(const std::string& key, const std::string& value);
    
    void setTimestamp(std::chrono::system_clock::time_point timestamp);
    void setDuration(std::chrono::microseconds duration);
    
    EventType getType() const { return type_; }
    const std::string& getName() const { return name_; }
    const std::string& getDescription() const { return description_; }
    std::chrono::system_clock::time_point getTimestamp() const { return timestamp_; }
    std::chrono::microseconds getDuration() const { return duration_; }
    
    const std::unordered_map<std::string, std::string>& getLabels() const { return labels_; }
    const std::unordered_map<std::string, double>& getMetrics() const { return metrics_; }
    const std::unordered_map<std::string, std::string>& getTags() const { return tags_; }
    
    nlohmann::json toJson() const;
    std::string toPrometheus() const;

private:
    EventType type_;
    std::string name_;
    std::string description_;
    std::chrono::system_clock::time_point timestamp_;
    std::chrono::microseconds duration_;
    std::unordered_map<std::string, std::string> labels_;
    std::unordered_map<std::string, double> metrics_;
    std::unordered_map<std::string, std::string> tags_;
};

class TelemetryCollector {
public:
    static TelemetryCollector& get();
    
    void setLevel(TelemetryLevel level);
    TelemetryLevel getLevel() const { return level_; }
    
    void enableBackend(TelemetryBackend backend, bool enabled = true);
    bool isBackendEnabled(TelemetryBackend backend) const;
    
    void setCustomBackend(std::function<void(const TelemetryEvent&)> callback);
    
    void collectEvent(const TelemetryEvent& event);
    void collectMetric(const std::string& name, double value, const std::string& description = "");
    void collectCounter(const std::string& name, uint64_t value, const std::string& description = "");
    
    void startTrace(const std::string& name);
    void endTrace(const std::string& name);
    
    void setContext(const std::string& key, const std::string& value);
    std::string getContext(const std::string& key) const;
    
    void flush();
    void clear();
    
    // Performance monitoring
    void startPerformanceMeasurement(const std::string& name);
    void endPerformanceMeasurement(const std::string& name);
    double getAveragePerformance(const std::string& name) const;
    
    // Health monitoring
    void reportHealth(const std::string& component, bool healthy, const std::string& message = "");
    bool isHealthy(const std::string& component) const;
    
    // Export functions
    std::string exportMetrics(TelemetryBackend backend = TelemetryBackend::PROMETHEUS) const;
    std::string exportEvents() const;
    std::string exportHealth() const;
    
    ~TelemetryCollector();

private:
    TelemetryCollector();
    TelemetryCollector(const TelemetryCollector&) = delete;
    TelemetryCollector& operator=(const TelemetryCollector&) = delete;
    
    void processEvent(const TelemetryEvent& event);
    void sendToBackend(const TelemetryEvent& event);
    void updateMetrics(const TelemetryEvent& event);
    
    TelemetryLevel level_;
    std::unordered_map<TelemetryBackend, bool> enabledBackends_;
    std::function<void(const TelemetryEvent&)> customBackend_;
    
    // Event processing
    std::queue<TelemetryEvent> eventQueue_;
    std::mutex eventMutex_;
    std::condition_variable eventCondition_;
    std::thread eventProcessor_;
    std::atomic<bool> running_;
    
    // Context and state
    std::unordered_map<std::string, std::string> context_;
    std::unordered_map<std::string, std::chrono::high_resolution_clock::time_point> activeTraces_;
    std::unordered_map<std::string, std::vector<double>> performanceMeasurements_;
    std::unordered_map<std::string, bool> healthStatus_;
    
    // Metrics integration
    std::shared_ptr<Counter> eventsProcessed_;
    std::shared_ptr<Counter> eventsDropped_;
    std::shared_ptr<Histogram> eventProcessingTime_;
    std::shared_ptr<Gauge> activeTracesCount_;
    
    mutable std::mutex collectorMutex_;
};

class TelemetrySpan {
public:
    TelemetrySpan(const std::string& name, const std::string& description = "");
    ~TelemetrySpan();
    
    void addLabel(const std::string& key, const std::string& value);
    void addMetric(const std::string& key, double value);
    void addTag(const std::string& key, const std::string& value);
    
    void setStatus(bool success, const std::string& message = "");

private:
    std::string name_;
    std::string description_;
    std::chrono::high_resolution_clock::time_point startTime_;
    TelemetryEvent event_;
    bool completed_;
};

// Convenience functions
namespace telemetry {

void setLevel(TelemetryLevel level);
TelemetryLevel getLevel();

void enableBackend(TelemetryBackend backend, bool enabled = true);
void setCustomBackend(std::function<void(const TelemetryEvent&)> callback);

void collectEvent(TelemetryEvent::EventType type, const std::string& name, const std::string& description = "");
void collectMetric(const std::string& name, double value, const std::string& description = "");
void collectCounter(const std::string& name, uint64_t value, const std::string& description = "");

void startTrace(const std::string& name);
void endTrace(const std::string& name);

void setContext(const std::string& key, const std::string& value);
std::string getContext(const std::string& key);

void flush();
void clear();

// Performance monitoring
void startPerformanceMeasurement(const std::string& name);
void endPerformanceMeasurement(const std::string& name);
double getAveragePerformance(const std::string& name);

// Health monitoring
void reportHealth(const std::string& component, bool healthy, const std::string& message = "");
bool isHealthy(const std::string& component);

// Export functions
std::string exportMetrics(TelemetryBackend backend = TelemetryBackend::PROMETHEUS);
std::string exportEvents();
std::string exportHealth();

// RAII wrapper for automatic tracing
class AutoSpan {
public:
    AutoSpan(const std::string& name, const std::string& description = "");
    ~AutoSpan();
    
    void addLabel(const std::string& key, const std::string& value);
    void addMetric(const std::string& key, double value);
    void addTag(const std::string& key, const std::string& value);
    void setStatus(bool success, const std::string& message = "");

private:
    std::unique_ptr<TelemetrySpan> span_;
};

// Macro for automatic tracing
#define TELEMETRY_SPAN(name, description) \
    beatrice::telemetry::AutoSpan telemetry_span_##__LINE__(name, description)

#define TELEMETRY_SPAN_LABEL(key, value) \
    telemetry_span_##__LINE__.addLabel(key, value)

#define TELEMETRY_SPAN_METRIC(key, value) \
    telemetry_span_##__LINE__.addMetric(key, value)

#define TELEMETRY_SPAN_TAG(key, value) \
    telemetry_span_##__LINE__.addTag(key, value)

#define TELEMETRY_SPAN_STATUS(success, message) \
    telemetry_span_##__LINE__.setStatus(success, message)

} // namespace telemetry

} // namespace beatrice

#endif // BEATRICE_TELEMETRY_HPP 