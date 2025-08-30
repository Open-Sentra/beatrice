#ifndef BEATRICE_VERSION_HPP
#define BEATRICE_VERSION_HPP

#include <cstdint>

namespace beatrice {

/**
 * @brief Version information for Beatrice SDK
 */
struct Version {
    static constexpr std::uint32_t MAJOR = 1;
    static constexpr std::uint32_t MINOR = 0;
    static constexpr std::uint32_t PATCH = 0;
    
    static constexpr const char* STRING = "1.0.0";
    static constexpr const char* GIT_HASH = "";
    static constexpr const char* BUILD_DATE = "";
    
    /**
     * @brief Get version as string
     * @return Version string in format "major.minor.patch"
     */
    static constexpr const char* toString() noexcept {
        return STRING;
    }
    
    /**
     * @brief Get version as integer
     * @return Version as 32-bit integer (major << 16 | minor << 8 | patch)
     */
    static constexpr std::uint32_t toInteger() noexcept {
        return (MAJOR << 16) | (MINOR << 8) | PATCH;
    }
    
    /**
     * @brief Check if version is at least the specified version
     * @param major Major version number
     * @param minor Minor version number
     * @param patch Patch version number
     * @return true if current version is >= specified version
     */
    static constexpr bool isAtLeast(std::uint32_t major, std::uint32_t minor, std::uint32_t patch) noexcept {
        return (MAJOR > major) || 
               (MAJOR == major && MINOR > minor) || 
               (MAJOR == major && MINOR == minor && PATCH >= patch);
    }
};

} // namespace beatrice

#endif // BEATRICE_VERSION_HPP 
