#ifndef BEATRICE_ERROR_HPP
#define BEATRICE_ERROR_HPP

#include <stdexcept>
#include <string>
#include <system_error>
#include <memory>

namespace beatrice {

enum class ErrorCode {
    SUCCESS = 0,
    INVALID_ARGUMENT,
    INITIALIZATION_FAILED,
    RESOURCE_UNAVAILABLE,
    PERMISSION_DENIED,
    TIMEOUT,
    NETWORK_ERROR,
    PLUGIN_LOAD_FAILED,
    PLUGIN_EXECUTION_FAILED,
    BACKEND_ERROR,
    INTERNAL_ERROR,
    NOT_IMPLEMENTED,
    CLEANUP_FAILED,
    UNKNOWN_ERROR
};


class BeatriceException : public std::runtime_error {
public:
    explicit BeatriceException(const std::string& message, ErrorCode code = ErrorCode::UNKNOWN_ERROR);
    explicit BeatriceException(const char* message, ErrorCode code = ErrorCode::UNKNOWN_ERROR);
    
    ErrorCode getCode() const noexcept { return code_; }
    

    const std::string& getMessage() const noexcept { return message_; }

private:
    ErrorCode code_;
    std::string message_;
};


class InitializationException : public BeatriceException {
public:
    explicit InitializationException(const std::string& message);
    explicit InitializationException(const char* message);
};


class PluginException : public BeatriceException {
public:
    explicit PluginException(const std::string& message, ErrorCode code = ErrorCode::PLUGIN_EXECUTION_FAILED);
    explicit PluginException(const char* message, ErrorCode code = ErrorCode::PLUGIN_EXECUTION_FAILED);
};


class BackendException : public BeatriceException {
public:
    explicit BackendException(const std::string& message);
    explicit BackendException(const char* message);
};


class NetworkException : public BeatriceException {
public:
    explicit NetworkException(const std::string& message);
    explicit NetworkException(const char* message);
};


template<typename T>
class Result {
public:

    static Result<T> success(T value) {
        return Result<T>(std::move(value));
    }

    static Result<T> error(ErrorCode error, std::string message = "") {
        return Result<T>(error, std::move(message));
    }

    bool isSuccess() const noexcept { return errorCode_ == ErrorCode::SUCCESS; }

    bool isError() const noexcept { return !isSuccess(); }
    
    const T& getValue() const {
        if (isError()) {
            throw std::runtime_error("Attempting to get value from error result");
        }
        return value_;
    }
    
    ErrorCode getErrorCode() const noexcept { return errorCode_; }
    
    const std::string& getErrorMessage() const noexcept { return errorMessage_; }

private:
    Result(T value) : value_(std::move(value)), errorCode_(ErrorCode::SUCCESS) {}
    Result(ErrorCode error, std::string message) : errorCode_(error), errorMessage_(std::move(message)) {}
    
    T value_;
    ErrorCode errorCode_;
    std::string errorMessage_;
};


template<>
class Result<void> {
public:
    static Result<void> success() { return Result<void>(); }
    static Result<void> error(ErrorCode error, std::string message = "") {
        return Result<void>(error, std::move(message));
    }
    
    bool isSuccess() const noexcept { return errorCode_ == ErrorCode::SUCCESS; }
    bool isError() const noexcept { return !isSuccess(); }
    ErrorCode getErrorCode() const noexcept { return errorCode_; }
    const std::string& getErrorMessage() const noexcept { return errorMessage_; }

private:
    Result() : errorCode_(ErrorCode::SUCCESS) {}
    Result(ErrorCode error, std::string message) : errorCode_(error), errorMessage_(std::move(message)) {}
    
    ErrorCode errorCode_;
    std::string errorMessage_;
};

} // namespace beatrice

#endif // BEATRICE_ERROR_HPP 