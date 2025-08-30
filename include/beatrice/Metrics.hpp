#ifndef BEATRICE_METRICS_HPP
#define BEATRICE_METRICS_HPP

#include <string>
#include <chrono>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <vector>
#include <cstdint>
#include <limits>

namespace beatrice {

enum class MetricType {
    COUNTER,    // Monotonically increasing value
    GAUGE,      // Current value that can go up or down
    HISTOGRAM,  // Distribution of values
    SUMMARY     // Quantiles of values
};

class Metric {
public:
    virtual ~Metric() = default;
    const std::string& getName() const { return name_; }
    MetricType getType() const { return type_; }
    const std::string& getDescription() const { return description_; }
    const std::unordered_map<std::string, std::string>& getLabels() const { return labels_; }

protected:
    Metric(const std::string& name, MetricType type, const std::string& description = "")
        : name_(name), type_(type), description_(description) {}
    
    std::string name_;
    MetricType type_;
    std::string description_;
    std::unordered_map<std::string, std::string> labels_;
};

class Counter : public Metric {
public:
    explicit Counter(const std::string& name, const std::string& description = "");
    
    void increment() { value_.fetch_add(1, std::memory_order_relaxed); }
    void increment(double amount) { value_.fetch_add(amount, std::memory_order_relaxed); }
    
    double getValue() const { return value_.load(std::memory_order_relaxed); }
    void reset() { value_.store(0, std::memory_order_relaxed); }

private:
    std::atomic<double> value_{0.0};
};

class Gauge : public Metric {
public:
    explicit Gauge(const std::string& name, const std::string& description = "");
    
    void set(double value) { value_.store(value, std::memory_order_relaxed); }
    void increment(double amount) { value_.fetch_add(amount, std::memory_order_relaxed); }
    
    void decrement(double amount) { value_.fetch_sub(amount, std::memory_order_relaxed); }
    
    double getValue() const { return value_.load(std::memory_order_relaxed); }

private:
    std::atomic<double> value_{0.0};
};

class Histogram : public Metric {
public:
    explicit Histogram(const std::string& name, const std::string& description = "");
    
    void observe(double value);
    
    uint64_t getCount() const { return count_.load(std::memory_order_relaxed); }
    
    double getSum() const { return sum_.load(std::memory_order_relaxed); }
    
    double getMin() const { return min_.load(std::memory_order_relaxed); }
    
    double getMax() const { return max_.load(std::memory_order_relaxed); }
    
    double getQuantile(double quantile) const;

private:
    std::atomic<uint64_t> count_{0};
    std::atomic<double> sum_{0.0};
    std::atomic<double> min_{std::numeric_limits<double>::max()};
    std::atomic<double> max_{std::numeric_limits<double>::lowest()};
    mutable std::mutex valuesMutex_;
    std::vector<double> values_;
};

class MetricsRegistry {
public:
    static MetricsRegistry& get();
    std::shared_ptr<Counter> createCounter(const std::string& name, const std::string& description = "");
    std::shared_ptr<Gauge> createGauge(const std::string& name, const std::string& description = "");
    
    std::shared_ptr<Histogram> createHistogram(const std::string& name, const std::string& description = "");
    
    std::shared_ptr<Metric> getMetric(const std::string& name) const;
    
    const std::unordered_map<std::string, std::shared_ptr<Metric>>& getAllMetrics() const { return metrics_; }
    
    std::string exportPrometheus() const;
    
    std::string exportJson() const;
    
    void clear();

    ~MetricsRegistry() = default;

private:
    MetricsRegistry() = default;
    MetricsRegistry(const MetricsRegistry&) = delete;
    MetricsRegistry& operator=(const MetricsRegistry&) = delete;
    
    static std::unique_ptr<MetricsRegistry> instance_;
    
    mutable std::mutex metricsMutex_;
    std::unordered_map<std::string, std::shared_ptr<Metric>> metrics_;
    
    std::string sanitizeMetricName(const std::string& name) const;
    std::string formatLabels(const std::unordered_map<std::string, std::string>& labels) const;
};

// Convenience functions for common metrics
namespace metrics {

std::shared_ptr<Counter> counter(const std::string& name, const std::string& description = "");

std::shared_ptr<Gauge> gauge(const std::string& name, const std::string& description = "");

std::shared_ptr<Histogram> histogram(const std::string& name, const std::string& description = "");

} // namespace metrics

} // namespace beatrice

#endif // BEATRICE_METRICS_HPP 