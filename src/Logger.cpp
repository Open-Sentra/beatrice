#include "beatrice/Logger.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/async.h>
#include <iostream>
#include <memory>

namespace beatrice {

// Static member initialization
std::unique_ptr<Logger> Logger::instance_ = nullptr;
std::mutex Logger::mutex_;

Logger::~Logger() {
    try {
        if (logger_) {
            // Don't call spdlog::shutdown() as it causes memory corruption
            // Just clear the logger reference
            logger_.reset();
        }
        // Let spdlog handle its own cleanup
    } catch (...) {
        // Ignore any exceptions during cleanup
    }
}

Result<void> Logger::initialize(const std::string& logFile, const std::string& logLevel, 
                              size_t maxFileSize, size_t maxFiles) {
    try {
        if (initialized_) {
            return Result<void>::success();
        }

        // Convert log level string to spdlog level
        auto level = spdlog::level::info;
        if (logLevel == "trace") level = spdlog::level::trace;
        else if (logLevel == "debug") level = spdlog::level::debug;
        else if (logLevel == "info") level = spdlog::level::info;
        else if (logLevel == "warn") level = spdlog::level::warn;
        else if (logLevel == "error") level = spdlog::level::err;
        else if (logLevel == "critical") level = spdlog::level::critical;

        // Create console sink
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(level);

        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(console_sink);

        // Create file sink if logFile is specified
        if (!logFile.empty()) {
            try {
                auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    logFile, maxFileSize, maxFiles);
                file_sink->set_level(level);
                sinks.push_back(file_sink);
            } catch (const spdlog::spdlog_ex& e) {
                std::cerr << "Failed to create file sink: " << e.what() << std::endl;
                // Continue with console sink only
            }
        }

        // Create logger
        logger_ = std::make_shared<spdlog::logger>("beatrice", sinks.begin(), sinks.end());
        logger_->set_level(level);
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

        // Set as default logger
        spdlog::set_default_logger(logger_);
        spdlog::flush_on(level);

        initialized_ = true;
        return Result<void>::success();

    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::INITIALIZATION_FAILED, e.what());
    }
}

Logger& Logger::get() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!instance_) {
        instance_ = std::unique_ptr<Logger>(new Logger());
    }
    return *instance_;
}

void Logger::setLevel(LogLevel level) {
    if (!logger_) return;

    spdlog::level::level_enum spdlog_level;
    switch (level) {
        case LogLevel::TRACE: spdlog_level = spdlog::level::trace; break;
        case LogLevel::DEBUG: spdlog_level = spdlog::level::debug; break;
        case LogLevel::INFO: spdlog_level = spdlog::level::info; break;
        case LogLevel::WARN: spdlog_level = spdlog::level::warn; break;
        case LogLevel::ERROR: spdlog_level = spdlog::level::err; break;
        case LogLevel::CRITICAL: spdlog_level = spdlog::level::critical; break;
        default: spdlog_level = spdlog::level::info; break;
    }

    logger_->set_level(spdlog_level);
}

LogLevel Logger::getLevel() const {
    if (!logger_) return LogLevel::INFO;

    auto level = logger_->level();
    if (level == spdlog::level::trace) return LogLevel::TRACE;
    if (level == spdlog::level::debug) return LogLevel::DEBUG;
    if (level == spdlog::level::info) return LogLevel::INFO;
    if (level == spdlog::level::warn) return LogLevel::WARN;
    if (level == spdlog::level::err) return LogLevel::ERROR;
    if (level == spdlog::level::critical) return LogLevel::CRITICAL;
    
    return LogLevel::INFO;
}

void Logger::flush() {
    if (logger_) {
        logger_->flush();
    }
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (instance_) {
        spdlog::shutdown();
        instance_.reset();
    }
}

} // namespace beatrice 