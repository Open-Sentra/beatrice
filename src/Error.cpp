#include "beatrice/Error.hpp"

namespace beatrice {

BeatriceException::BeatriceException(const std::string& message, ErrorCode code)
    : std::runtime_error(message), code_(code), message_(message) {
}

BeatriceException::BeatriceException(const char* message, ErrorCode code)
    : std::runtime_error(message), code_(code), message_(message) {
}

InitializationException::InitializationException(const std::string& message)
    : BeatriceException(message, ErrorCode::INITIALIZATION_FAILED) {
}

InitializationException::InitializationException(const char* message)
    : BeatriceException(message, ErrorCode::INITIALIZATION_FAILED) {
}

PluginException::PluginException(const std::string& message, ErrorCode code)
    : BeatriceException(message, code) {
}

PluginException::PluginException(const char* message, ErrorCode code)
    : BeatriceException(message, code) {
}

BackendException::BackendException(const std::string& message)
    : BeatriceException(message, ErrorCode::BACKEND_ERROR) {
}

BackendException::BackendException(const char* message)
    : BeatriceException(message, ErrorCode::BACKEND_ERROR) {
}

NetworkException::NetworkException(const std::string& message)
    : BeatriceException(message, ErrorCode::NETWORK_ERROR) {
}

NetworkException::NetworkException(const char* message)
    : BeatriceException(message, ErrorCode::NETWORK_ERROR) {
}

} // namespace beatrice 