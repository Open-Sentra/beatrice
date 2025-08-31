#include "beatrice/Telemetry.hpp"
#include "beatrice/Metrics.hpp"
#include "beatrice/Logger.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>

namespace beatrice {

// TelemetryEvent implementation
TelemetryEvent::TelemetryEvent(EventType type, const std::string& name, const std::string& description)
    : type_(type), name_(name), description_(description), 
      timestamp_(std::chrono::system_clock::now()), duration_(0) {
}

void TelemetryEvent::addLabel(const std::string& key, const std::string& value) {
    labels_[key] = value;
}

void TelemetryEvent::addMetric(const std::string& key, double value) {
    metrics_[key] = value;
}

void TelemetryEvent::addTag(const std::string& key, const std::string& value) {
    tags_[key] = value;
}

void TelemetryEvent::setTimestamp(std::chrono::system_clock::time_point timestamp) {
    timestamp_ = timestamp;
}

void TelemetryEvent::setDuration(std::chrono::microseconds duration) {
    duration_ = duration;
}

nlohmann::json TelemetryEvent::toJson() const {
    nlohmann::json j;
    
    j["type"] = static_cast<int>(type_);
    j["name"] = name_;
    j["description"] = description_;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp_.time_since_epoch()).count();
    j["duration_us"] = duration_.count();
    j["labels"] = labels_;
    j["metrics"] = metrics_;
    j["tags"] = tags_;
    
    return j;
}

std::string TelemetryEvent::toPrometheus() const {
    std::ostringstream oss;
    
    // Convert event type to string
    std::string typeStr;
    switch (type_) {
        case EventType::PACKET_RECEIVED: typeStr = "packet_received"; break;
        case EventType::PACKET_PROCESSED: typeStr = "packet_processed"; break;
        case EventType::PACKET_DROPPED: typeStr = "packet_dropped"; break;
        case EventType::BACKEND_INITIALIZED: typeStr = "backend_initialized"; break;
        case EventType::BACKEND_ERROR: typeStr = "backend_error"; break;
        case EventType::PLUGIN_LOADED: typeStr = "plugin_loaded"; break;
        case EventType::PLUGIN_ERROR: typeStr = "plugin_error"; break;
        case EventType::PERFORMANCE_MEASUREMENT: typeStr = "performance_measurement"; break;
        case EventType::SYSTEM_HEALTH_CHECK: typeStr = "system_health_check"; break;
        case EventType::CUSTOM: typeStr = "custom"; break;
    }
    
    // Format labels
    std::string labelStr;
    if (!labels_.empty()) {
        labelStr = "{";
        bool first = true;
        for (const auto& [key, value] : labels_) {
            if (!first) labelStr += ",";
            labelStr += key + "=\"" + value + "\"";
            first = false;
        }
        labelStr += "}";
    }
    
    // Export metrics
    for (const auto& [key, value] : metrics_) {
        oss << "beatrice_telemetry_" << typeStr << "_" << key << labelStr << " " 
            << std::fixed << std::setprecision(6) << value << "\n";
    }
    
    // Export duration
    oss << "beatrice_telemetry_" << typeStr << "_duration_us" << labelStr << " " 
        << duration_.count() << "\n";
    
    return oss.str();
}

// TelemetryCollector implementation
TelemetryCollector::TelemetryCollector()
    : level_(TelemetryLevel::STANDARD), running_(true) {
    
    // Initialize metrics
    eventsProcessed_ = metrics::counter("telemetry_events_processed", "Total telemetry events processed");
    eventsDropped_ = metrics::counter("telemetry_events_dropped", "Total telemetry events dropped");
    eventProcessingTime_ = metrics::histogram("telemetry_event_processing_time", "Telemetry event processing time");
    activeTracesCount_ = metrics::gauge("telemetry_active_traces", "Number of active traces");
    
    // Initialize backends
    enabledBackends_[TelemetryBackend::PROMETHEUS] = true;
    enabledBackends_[TelemetryBackend::INFLUXDB] = false;
    enabledBackends_[TelemetryBackend::JAEGER] = false;
    enabledBackends_[TelemetryBackend::CUSTOM] = false;
    
    // Start event processor thread
    eventProcessor_ = std::thread([this]() {
        while (running_) {
            std::unique_lock<std::mutex> lock(eventMutex_);
            eventCondition_.wait(lock, [this]() { 
                return !eventQueue_.empty() || !running_; 
            });
            
            while (!eventQueue_.empty() && running_) {
                TelemetryEvent event = eventQueue_.front();
                eventQueue_.pop();
                lock.unlock();
                
                processEvent(event);
                
                lock.lock();
            }
        }
    });
}

TelemetryCollector::~TelemetryCollector() {
    running_ = false;
    eventCondition_.notify_all();
    
    if (eventProcessor_.joinable()) {
        eventProcessor_.join();
    }
}

TelemetryCollector& TelemetryCollector::get() {
    static TelemetryCollector instance;
    return instance;
}

void TelemetryCollector::setLevel(TelemetryLevel level) {
    std::lock_guard<std::mutex> lock(collectorMutex_);
    level_ = level;
}

void TelemetryCollector::enableBackend(TelemetryBackend backend, bool enabled) {
    std::lock_guard<std::mutex> lock(collectorMutex_);
    enabledBackends_[backend] = enabled;
}

bool TelemetryCollector::isBackendEnabled(TelemetryBackend backend) const {
    std::lock_guard<std::mutex> lock(collectorMutex_);
    auto it = enabledBackends_.find(backend);
    return it != enabledBackends_.end() && it->second;
}

void TelemetryCollector::setCustomBackend(std::function<void(const TelemetryEvent&)> callback) {
    std::lock_guard<std::mutex> lock(collectorMutex_);
    customBackend_ = callback;
    enabledBackends_[TelemetryBackend::CUSTOM] = true;
}

void TelemetryCollector::collectEvent(const TelemetryEvent& event) {
    std::lock_guard<std::mutex> lock(eventMutex_);
    
    if (eventQueue_.size() < 10000) {  // Prevent memory issues
        eventQueue_.push(event);
        eventCondition_.notify_one();
    } else {
        eventsDropped_->increment();
    }
}

void TelemetryCollector::collectMetric(const std::string& name, double value, const std::string& description) {
    auto gauge = metrics::gauge(name, description);
    gauge->set(value);
    
    // Create telemetry event
    TelemetryEvent event(TelemetryEvent::EventType::PERFORMANCE_MEASUREMENT, name, description);
    event.addMetric("value", value);
    collectEvent(event);
}

void TelemetryCollector::collectCounter(const std::string& name, uint64_t value, const std::string& description) {
    auto counter = metrics::counter(name, description);
    counter->increment(value);
    
    // Create telemetry event
    TelemetryEvent event(TelemetryEvent::EventType::PERFORMANCE_MEASUREMENT, name, description);
    event.addMetric("value", static_cast<double>(value));
    collectEvent(event);
}

void TelemetryCollector::startTrace(const std::string& name) {
    std::lock_guard<std::mutex> lock(collectorMutex_);
    activeTraces_[name] = std::chrono::high_resolution_clock::now();
    activeTracesCount_->set(activeTraces_.size());
}

void TelemetryCollector::endTrace(const std::string& name) {
    std::lock_guard<std::mutex> lock(collectorMutex_);
    
    auto it = activeTraces_.find(name);
    if (it != activeTraces_.end()) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - it->second);
        
        TelemetryEvent event(TelemetryEvent::EventType::PERFORMANCE_MEASUREMENT, name);
        event.setDuration(duration);
        event.addMetric("duration_us", static_cast<double>(duration.count()));
        
        collectEvent(event);
        
        activeTraces_.erase(it);
        activeTracesCount_->set(activeTraces_.size());
    }
}

void TelemetryCollector::setContext(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(collectorMutex_);
    context_[key] = value;
}

std::string TelemetryCollector::getContext(const std::string& key) const {
    std::lock_guard<std::mutex> lock(collectorMutex_);
    auto it = context_.find(key);
    return it != context_.end() ? it->second : "";
}

void TelemetryCollector::flush() {
    std::unique_lock<std::mutex> lock(eventMutex_);
    while (!eventQueue_.empty()) {
        TelemetryEvent event = eventQueue_.front();
        eventQueue_.pop();
        lock.unlock();
        
        processEvent(event);
        
        lock.lock();
    }
}

void TelemetryCollector::clear() {
    std::lock_guard<std::mutex> lock(collectorMutex_);
    context_.clear();
    activeTraces_.clear();
    performanceMeasurements_.clear();
    healthStatus_.clear();
    
    std::lock_guard<std::mutex> eventLock(eventMutex_);
    while (!eventQueue_.empty()) {
        eventQueue_.pop();
    }
}

void TelemetryCollector::startPerformanceMeasurement(const std::string& name) {
    std::lock_guard<std::mutex> lock(collectorMutex_);
    performanceMeasurements_[name].push_back(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
}

void TelemetryCollector::endPerformanceMeasurement(const std::string& name) {
    std::lock_guard<std::mutex> lock(collectorMutex_);
    
    auto it = performanceMeasurements_.find(name);
    if (it != performanceMeasurements_.end() && !it->second.empty()) {
        auto endTime = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        auto startTime = it->second.back();
        it->second.pop_back();
        
        double duration = static_cast<double>(endTime - startTime) / 1000.0;  // Convert to microseconds
        
        TelemetryEvent event(TelemetryEvent::EventType::PERFORMANCE_MEASUREMENT, name);
        event.setDuration(std::chrono::microseconds(static_cast<long>(duration)));
        event.addMetric("duration_us", duration);
        
        collectEvent(event);
    }
}

double TelemetryCollector::getAveragePerformance(const std::string& name) const {
    std::lock_guard<std::mutex> lock(collectorMutex_);
    
    auto it = performanceMeasurements_.find(name);
    if (it != performanceMeasurements_.end() && !it->second.empty()) {
        double sum = 0.0;
        for (double value : it->second) {
            sum += value;
        }
        return sum / it->second.size();
    }
    
    return 0.0;
}

void TelemetryCollector::reportHealth(const std::string& component, bool healthy, const std::string& message) {
    std::lock_guard<std::mutex> lock(collectorMutex_);
    healthStatus_[component] = healthy;
    
    TelemetryEvent event(TelemetryEvent::EventType::SYSTEM_HEALTH_CHECK, component);
    event.addLabel("status", healthy ? "healthy" : "unhealthy");
    if (!message.empty()) {
        event.addLabel("message", message);
    }
    
    collectEvent(event);
}

bool TelemetryCollector::isHealthy(const std::string& component) const {
    std::lock_guard<std::mutex> lock(collectorMutex_);
    auto it = healthStatus_.find(component);
    return it != healthStatus_.end() ? it->second : true;
}

std::string TelemetryCollector::exportMetrics(TelemetryBackend backend) const {
    switch (backend) {
        case TelemetryBackend::PROMETHEUS:
            return MetricsRegistry::get().exportPrometheus();
        case TelemetryBackend::INFLUXDB:
                // TODO: Implement InfluxDB format (Priority: MEDIUM)
    // See TODO.md for details
    return "InfluxDB format not yet implemented";
        default:
            return MetricsRegistry::get().exportJson();
    }
}

std::string TelemetryCollector::exportEvents() const {
    // This would typically export recent events
    // For now, return a summary
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"total_events_processed\": " << eventsProcessed_->getValue() << ",\n";
    oss << "  \"total_events_dropped\": " << eventsDropped_->getValue() << ",\n";
    oss << "  \"active_traces\": " << activeTracesCount_->getValue() << "\n";
    oss << "}\n";
    return oss.str();
}

std::string TelemetryCollector::exportHealth() const {
    std::lock_guard<std::mutex> lock(collectorMutex_);
    
    nlohmann::json j;
    j["overall_health"] = true;
    j["components"] = nlohmann::json::object();
    
    for (const auto& [component, healthy] : healthStatus_) {
        j["components"][component] = healthy;
        if (!healthy) {
            j["overall_health"] = false;
        }
    }
    
    return j.dump(2);
}

void TelemetryCollector::processEvent(const TelemetryEvent& event) {
    eventsProcessed_->increment();
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    sendToBackend(event);
    updateMetrics(event);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    eventProcessingTime_->observe(static_cast<double>(duration.count()));
}

void TelemetryCollector::sendToBackend(const TelemetryEvent& event) {
    std::lock_guard<std::mutex> lock(collectorMutex_);
    
    if (enabledBackends_[TelemetryBackend::PROMETHEUS]) {
        // Prometheus metrics are already handled by the metrics system
    }
    
    if (enabledBackends_[TelemetryBackend::CUSTOM] && customBackend_) {
        try {
            customBackend_(event);
        } catch (const std::exception& e) {
            BEATRICE_ERROR("Custom telemetry backend error: {}", e.what());
        }
    }
}

void TelemetryCollector::updateMetrics(const TelemetryEvent& event) {
    // Update specific metrics based on event type
    switch (event.getType()) {
        case TelemetryEvent::EventType::PACKET_RECEIVED:
            metrics::counter("packets_received_total", "Total packets received")->increment();
            break;
        case TelemetryEvent::EventType::PACKET_PROCESSED:
            metrics::counter("packets_processed_total", "Total packets processed")->increment();
            break;
        case TelemetryEvent::EventType::PACKET_DROPPED:
            metrics::counter("packets_dropped_total", "Total packets dropped")->increment();
            break;
        case TelemetryEvent::EventType::BACKEND_ERROR:
            metrics::counter("backend_errors_total", "Total backend errors")->increment();
            break;
        case TelemetryEvent::EventType::PLUGIN_ERROR:
            metrics::counter("plugin_errors_total", "Total plugin errors")->increment();
            break;
        default:
            break;
    }
}

// TelemetrySpan implementation
TelemetrySpan::TelemetrySpan(const std::string& name, const std::string& description)
    : name_(name), description_(description), startTime_(std::chrono::high_resolution_clock::now()),
      event_(TelemetryEvent::EventType::PERFORMANCE_MEASUREMENT, name, description), completed_(false) {
    
    TelemetryCollector::get().startTrace(name);
}

TelemetrySpan::~TelemetrySpan() {
    if (!completed_) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime_);
        
        event_.setDuration(duration);
        TelemetryCollector::get().collectEvent(event_);
        TelemetryCollector::get().endTrace(name_);
    }
}

void TelemetrySpan::addLabel(const std::string& key, const std::string& value) {
    event_.addLabel(key, value);
}

void TelemetrySpan::addMetric(const std::string& key, double value) {
    event_.addMetric(key, value);
}

void TelemetrySpan::addTag(const std::string& key, const std::string& value) {
    event_.addTag(key, value);
}

void TelemetrySpan::setStatus(bool success, const std::string& message) {
    event_.addLabel("success", success ? "true" : "false");
    if (!message.empty()) {
        event_.addLabel("message", message);
    }
    completed_ = true;
}

// Convenience functions implementation
namespace telemetry {

void setLevel(TelemetryLevel level) {
    TelemetryCollector::get().setLevel(level);
}

TelemetryLevel getLevel() {
    return TelemetryCollector::get().getLevel();
}

void enableBackend(TelemetryBackend backend, bool enabled) {
    TelemetryCollector::get().enableBackend(backend, enabled);
}

void setCustomBackend(std::function<void(const TelemetryEvent&)> callback) {
    TelemetryCollector::get().setCustomBackend(callback);
}

void collectEvent(TelemetryEvent::EventType type, const std::string& name, const std::string& description) {
    TelemetryEvent event(type, name, description);
    TelemetryCollector::get().collectEvent(event);
}

void collectMetric(const std::string& name, double value, const std::string& description) {
    TelemetryCollector::get().collectMetric(name, value, description);
}

void collectCounter(const std::string& name, uint64_t value, const std::string& description) {
    TelemetryCollector::get().collectCounter(name, value, description);
}

void startTrace(const std::string& name) {
    TelemetryCollector::get().startTrace(name);
}

void endTrace(const std::string& name) {
    TelemetryCollector::get().endTrace(name);
}

void setContext(const std::string& key, const std::string& value) {
    TelemetryCollector::get().setContext(key, value);
}

std::string getContext(const std::string& key) {
    return TelemetryCollector::get().getContext(key);
}

void flush() {
    TelemetryCollector::get().flush();
}

void clear() {
    TelemetryCollector::get().clear();
}

void startPerformanceMeasurement(const std::string& name) {
    TelemetryCollector::get().startPerformanceMeasurement(name);
}

void endPerformanceMeasurement(const std::string& name) {
    TelemetryCollector::get().endPerformanceMeasurement(name);
}

double getAveragePerformance(const std::string& name) {
    return TelemetryCollector::get().getAveragePerformance(name);
}

void reportHealth(const std::string& component, bool healthy, const std::string& message) {
    TelemetryCollector::get().reportHealth(component, healthy, message);
}

bool isHealthy(const std::string& component) {
    return TelemetryCollector::get().isHealthy(component);
}

std::string exportMetrics(TelemetryBackend backend) {
    return TelemetryCollector::get().exportMetrics(backend);
}

std::string exportEvents() {
    return TelemetryCollector::get().exportEvents();
}

std::string exportHealth() {
    return TelemetryCollector::get().exportHealth();
}

// AutoSpan implementation
AutoSpan::AutoSpan(const std::string& name, const std::string& description)
    : span_(std::make_unique<TelemetrySpan>(name, description)) {
}

AutoSpan::~AutoSpan() = default;

void AutoSpan::addLabel(const std::string& key, const std::string& value) {
    if (span_) span_->addLabel(key, value);
}

void AutoSpan::addMetric(const std::string& key, double value) {
    if (span_) span_->addMetric(key, value);
}

void AutoSpan::addTag(const std::string& key, const std::string& value) {
    if (span_) span_->addTag(key, value);
}

void AutoSpan::setStatus(bool success, const std::string& message) {
    if (span_) span_->setStatus(success, message);
}

} // namespace telemetry

} // namespace beatrice 