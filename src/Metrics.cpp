#include "beatrice/Metrics.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace beatrice {

// Static member initialization
std::unique_ptr<MetricsRegistry> MetricsRegistry::instance_ = nullptr;

// Counter implementation
Counter::Counter(const std::string& name, const std::string& description)
    : Metric(name, MetricType::COUNTER, description) {
}

// Gauge implementation
Gauge::Gauge(const std::string& name, const std::string& description)
    : Metric(name, MetricType::GAUGE, description) {
}

// Histogram implementation
Histogram::Histogram(const std::string& name, const std::string& description)
    : Metric(name, MetricType::HISTOGRAM, description) {
}

void Histogram::observe(double value) {
    count_.fetch_add(1, std::memory_order_relaxed);
    
    double oldSum = sum_.load(std::memory_order_relaxed);
    double newSum;
    do {
        newSum = oldSum + value;
    } while (!sum_.compare_exchange_weak(oldSum, newSum, std::memory_order_relaxed));
    
    // Update min
    double oldMin = min_.load(std::memory_order_relaxed);
    while (value < oldMin && !min_.compare_exchange_weak(oldMin, value, std::memory_order_relaxed)) {}
    
    // Update max
    double oldMax = max_.load(std::memory_order_relaxed);
    while (value > oldMax && !max_.compare_exchange_weak(oldMax, value, std::memory_order_relaxed)) {}
    
    // Store value for quantile calculation
    std::lock_guard<std::mutex> lock(valuesMutex_);
    values_.push_back(value);
}

double Histogram::getQuantile(double quantile) const {
    if (quantile < 0.0 || quantile > 1.0) {
        return 0.0;
    }
    
    std::lock_guard<std::mutex> lock(valuesMutex_);
    if (values_.empty()) {
        return 0.0;
    }
    
    std::vector<double> sortedValues = values_;
    std::sort(sortedValues.begin(), sortedValues.end());
    
    size_t index = static_cast<size_t>(quantile * (sortedValues.size() - 1));
    return sortedValues[index];
}

// MetricsRegistry implementation
MetricsRegistry& MetricsRegistry::get() {
    if (!instance_) {
        instance_ = std::unique_ptr<MetricsRegistry>(new MetricsRegistry());
    }
    return *instance_;
}

std::shared_ptr<Counter> MetricsRegistry::createCounter(const std::string& name, const std::string& description) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    auto counter = std::make_shared<Counter>(name, description);
    metrics_[name] = counter;
    
    return counter;
}

std::shared_ptr<Gauge> MetricsRegistry::createGauge(const std::string& name, const std::string& description) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    auto gauge = std::make_shared<Gauge>(name, description);
    metrics_[name] = gauge;
    
    return gauge;
}

std::shared_ptr<Histogram> MetricsRegistry::createHistogram(const std::string& name, const std::string& description) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    auto histogram = std::make_shared<Histogram>(name, description);
    metrics_[name] = histogram;
    
    return histogram;
}

std::shared_ptr<Metric> MetricsRegistry::getMetric(const std::string& name) const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    auto it = metrics_.find(name);
    if (it != metrics_.end()) {
        return it->second;
    }
    
    return nullptr;
}

std::string MetricsRegistry::exportPrometheus() const {
    std::ostringstream oss;
    
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    for (const auto& pair : metrics_) {
        const auto& metric = pair.second;
        const std::string& name = metric->getName();
        const std::string& description = metric->getName();
        
        oss << "# HELP " << sanitizeMetricName(name) << " " << description << "\n";
        
        switch (metric->getType()) {
            case MetricType::COUNTER: {
                oss << "# TYPE " << sanitizeMetricName(name) << " counter\n";
                auto counter = std::dynamic_pointer_cast<Counter>(metric);
                if (counter) {
                    oss << sanitizeMetricName(name) << formatLabels(metric->getLabels()) << " " 
                        << std::fixed << std::setprecision(6) << counter->getValue() << "\n";
                }
                break;
            }
            case MetricType::GAUGE: {
                oss << "# TYPE " << sanitizeMetricName(name) << " gauge\n";
                auto gauge = std::dynamic_pointer_cast<Gauge>(metric);
                if (gauge) {
                    oss << sanitizeMetricName(name) << formatLabels(metric->getLabels()) << " " 
                        << std::fixed << std::setprecision(6) << gauge->getValue() << "\n";
                }
                break;
            }
            case MetricType::HISTOGRAM: {
                oss << "# TYPE " << sanitizeMetricName(name) << " histogram\n";
                auto histogram = std::dynamic_pointer_cast<Histogram>(metric);
                if (histogram) {
                    oss << sanitizeMetricName(name) << "_count" << formatLabels(metric->getLabels()) << " " 
                        << histogram->getCount() << "\n";
                    oss << sanitizeMetricName(name) << "_sum" << formatLabels(metric->getLabels()) << " " 
                        << std::fixed << std::setprecision(6) << histogram->getSum() << "\n";
                    oss << sanitizeMetricName(name) << "_min" << formatLabels(metric->getLabels()) << " " 
                        << std::fixed << std::setprecision(6) << histogram->getMin() << "\n";
                    oss << sanitizeMetricName(name) << "_max" << formatLabels(metric->getLabels()) << " " 
                        << std::fixed << std::setprecision(6) << histogram->getMax() << "\n";
                }
                break;
            }
            default:
                break;
        }
    }
    
    return oss.str();
}

std::string MetricsRegistry::exportJson() const {
    std::ostringstream oss;
    
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    oss << "{\n";
    oss << "  \"metrics\": [\n";
    
    bool first = true;
    for (const auto& pair : metrics_) {
        if (!first) oss << ",\n";
        
        const auto& metric = pair.second;
        oss << "    {\n";
        oss << "      \"name\": \"" << metric->getName() << "\",\n";
        oss << "      \"type\": \"";
        
        switch (metric->getType()) {
            case MetricType::COUNTER: oss << "counter"; break;
            case MetricType::GAUGE: oss << "gauge"; break;
            case MetricType::HISTOGRAM: oss << "histogram"; break;
            case MetricType::SUMMARY: oss << "summary"; break;
        }
        
        oss << "\",\n";
        oss << "      \"description\": \"" << metric->getDescription() << "\",\n";
        
        switch (metric->getType()) {
            case MetricType::COUNTER: {
                auto counter = std::dynamic_pointer_cast<Counter>(metric);
                if (counter) {
                    oss << "      \"value\": " << std::fixed << std::setprecision(6) << counter->getValue() << "\n";
                }
                break;
            }
            case MetricType::GAUGE: {
                auto gauge = std::dynamic_pointer_cast<Gauge>(metric);
                if (gauge) {
                    oss << "      \"value\": " << std::fixed << std::setprecision(6) << gauge->getValue() << "\n";
                }
                break;
            }
            case MetricType::HISTOGRAM: {
                auto histogram = std::dynamic_pointer_cast<Histogram>(metric);
                if (histogram) {
                    oss << "      \"count\": " << histogram->getCount() << ",\n";
                    oss << "      \"sum\": " << std::fixed << std::setprecision(6) << histogram->getSum() << ",\n";
                    oss << "      \"min\": " << std::fixed << std::setprecision(6) << histogram->getMin() << ",\n";
                    oss << "      \"max\": " << std::fixed << std::setprecision(6) << histogram->getMax() << "\n";
                }
                break;
            }
            default:
                oss << "      \"value\": 0\n";
                break;
        }
        
        oss << "    }";
        first = false;
    }
    
    oss << "\n  ],\n";
    oss << "  \"total_metrics\": " << metrics_.size() << "\n";
    oss << "}\n";
    
    return oss.str();
}

void MetricsRegistry::clear() {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    metrics_.clear();
}

std::string MetricsRegistry::sanitizeMetricName(const std::string& name) const {
    std::string sanitized = name;
    
    // Replace invalid characters with underscores
    std::replace_if(sanitized.begin(), sanitized.end(), 
                   [](char c) { return !std::isalnum(c) && c != '_' && c != ':' && c != '.'; }, '_');
    
    // Ensure it starts with a letter
    if (!sanitized.empty() && !std::isalpha(sanitized[0])) {
        sanitized = "metric_" + sanitized;
    }
    
    return sanitized;
}

std::string MetricsRegistry::formatLabels(const std::unordered_map<std::string, std::string>& labels) const {
    if (labels.empty()) {
        return "";
    }
    
    std::ostringstream oss;
    oss << "{";
    
    bool first = true;
    for (const auto& pair : labels) {
        if (!first) oss << ",";
        oss << pair.first << "=\"" << pair.second << "\"";
        first = false;
    }
    
    oss << "}";
    return oss.str();
}

// Convenience functions
namespace metrics {

std::shared_ptr<Counter> counter(const std::string& name, const std::string& description) {
    return MetricsRegistry::get().createCounter(name, description);
}

std::shared_ptr<Gauge> gauge(const std::string& name, const std::string& description) {
    return MetricsRegistry::get().createGauge(name, description);
}

std::shared_ptr<Histogram> histogram(const std::string& name, const std::string& description) {
    return MetricsRegistry::get().createHistogram(name, description);
}

} // namespace metrics

} // namespace beatrice 