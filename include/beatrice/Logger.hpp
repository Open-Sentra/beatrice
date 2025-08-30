#ifndef BEATRICE_LOGGER_HPP
#define BEATRICE_LOGGER_HPP

#include "Error.hpp"
#include <string>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace beatrice {

/**
 * @brief Log levels for Beatrice SDK
 */
enum class LogLevel {
    TRACE = SPDLOG_LEVEL_TRACE,
    DEBUG = SPDLOG_LEVEL_DEBUG,
    INFO = SPDLOG_LEVEL_INFO,
    WARN = SPDLOG_LEVEL_WARN,
    ERROR = SPDLOG_LEVEL_ERROR,
    CRITICAL = SPDLOG_LEVEL_CRITICAL
};

/**
 * @brief Logger class for Beatrice SDK
 * 
 * Provides centralized logging functionality with multiple output sinks
 * and configurable log levels.
 */
class Logger {
public:
    Logger() = default;
    ~Logger();

    /**
     * @brief Initialize the logger
     * @param logFile Path to log file (optional)
     * @param logLevel Log level as string
     * @param maxFileSize Maximum log file size in bytes
     * @param maxFiles Maximum number of log files to keep
     */
    Result<void> initialize(const std::string& logFile = "",
                           const std::string& logLevel = "info",
                           size_t maxFileSize = 10 * 1024 * 1024, // 10MB
                           size_t maxFiles = 5);

    /**
     * @brief Get the logger instance (singleton)
     * @return Reference to logger instance
     */
    static Logger& get();

    /**
     * @brief Set log level
     * @param level New log level
     */
    void setLevel(LogLevel level);

    /**
     * @brief Get current log level
     * @return Current log level
     */
    LogLevel getLevel() const;

    /**
     * @brief Flush all log messages
     */
    void flush();

    /**
     * @brief Shutdown the logger
     */
    static void shutdown();

    /**
     * @brief Get the underlying spdlog logger
     * @return Shared pointer to spdlog logger
     */
    std::shared_ptr<spdlog::logger> getLogger() const { return logger_; }

    /**
     * @brief Check if logger is initialized
     * @return True if initialized
     */
    bool isInitialized() const { return initialized_; }

private:
    static std::unique_ptr<Logger> instance_;
    static std::mutex mutex_;
    
    std::shared_ptr<spdlog::logger> logger_;
    bool initialized_;
};

// Convenience macros for logging
#define BEATRICE_TRACE(...)    SPDLOG_LOGGER_TRACE(beatrice::Logger::get().getLogger(), __VA_ARGS__)
#define BEATRICE_DEBUG(...)    SPDLOG_LOGGER_DEBUG(beatrice::Logger::get().getLogger(), __VA_ARGS__)
#define BEATRICE_INFO(...)     SPDLOG_LOGGER_INFO(beatrice::Logger::get().getLogger(), __VA_ARGS__)
#define BEATRICE_WARN(...)     SPDLOG_LOGGER_WARN(beatrice::Logger::get().getLogger(), __VA_ARGS__)
#define BEATRICE_ERROR(...)    SPDLOG_LOGGER_ERROR(beatrice::Logger::get().getLogger(), __VA_ARGS__)
#define BEATRICE_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(beatrice::Logger::get().getLogger(), __VA_ARGS__)

} // namespace beatrice

#endif // BEATRICE_LOGGER_HPP 