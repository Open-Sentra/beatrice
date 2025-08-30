cmake_minimum_required(VERSION 3.20)
project(Beatrice VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(ENABLE_SANITIZERS)
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic -fsanitize=address,undefined")
else()
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG -Wall -Wextra")

# Project options
option(BUILD_TESTS "Build test suite" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_DOCS "Build documentation" OFF)
option(ENABLE_SANITIZERS "Enable sanitizers" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
include(GNUInstallDirs)
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)

# Find libbpf
pkg_check_modules(LIBBPF REQUIRED libbpf)

# Find DPDK
option(ENABLE_DPDK "Enable DPDK support" ON)
if(ENABLE_DPDK)
    pkg_check_modules(DPDK REQUIRED libdpdk)
    if(DPDK_FOUND)
        message(STATUS "DPDK found: ${DPDK_VERSION}")
        add_definitions(-DENABLE_DPDK)
    else()
        message(WARNING "DPDK not found, DPDK backend will be disabled")
        set(ENABLE_DPDK OFF)
    endif()
endif()

# Find BPF tools
find_program(CLANG clang)
find_program(LLVM_STRIP llvm-strip)

# BPF program compilation
if(CLANG AND LLVM_STRIP)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/xdp_program.o
        COMMAND ${CLANG} -O2 -g -target bpf -c ${CMAKE_CURRENT_SOURCE_DIR}/src/xdp_program.c -o ${CMAKE_CURRENT_BINARY_DIR}/xdp_program.o
        COMMAND ${LLVM_STRIP} -g ${CMAKE_CURRENT_BINARY_DIR}/xdp_program.o
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/xdp_program.c
        COMMENT "Compiling BPF XDP program with BTF support"
        VERBATIM
    )
    
    add_custom_target(xdp_program ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/xdp_program.o)
else()
    message(WARNING "BPF tools not found, XDP program compilation disabled")
endif()

# Include directories
include_directories(include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Create version header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/beatrice/version.hpp.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/beatrice/version.hpp"
)

# Core library
add_library(beatrice_core SHARED
    src/PluginManager.cpp
    src/BeatriceContext.cpp
    src/AF_XDPBackend.cpp
    src/DPDKBackend.cpp
    src/PMDBackend.cpp
    src/AF_PacketBackend.cpp
    src/Logger.cpp
    src/Config.cpp
    src/Metrics.cpp
    src/Packet.cpp
    src/XDPLoader.cpp
)

# Set library properties
set_target_properties(beatrice_core PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME beatrice_core
    PUBLIC_HEADER "include/beatrice/BeatriceContext.hpp;include/beatrice/ICaptureBackend.hpp;include/beatrice/IPacketPlugin.hpp;include/beatrice/Packet.hpp;include/beatrice/PluginManager.hpp;include/beatrice/AF_XDPBackend.hpp;include/beatrice/DPDKBackend.hpp;include/beatrice/PMDBackend.hpp;include/beatrice/AF_PacketBackend.hpp"
)

# Link libraries
target_link_libraries(beatrice_core 
    PRIVATE 
        Threads::Threads
        ${CMAKE_DL_LIBS}
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        ${LIBBPF_LIBRARIES}
)

# Link DPDK if enabled
if(ENABLE_DPDK AND DPDK_FOUND)
    target_link_libraries(beatrice_core PRIVATE ${DPDK_LIBRARIES})
    target_include_directories(beatrice_core PRIVATE ${DPDK_INCLUDE_DIRS})
    target_compile_options(beatrice_core PRIVATE ${DPDK_CFLAGS_OTHER})
endif()

# Compiler-specific flags
target_compile_features(beatrice_core PRIVATE cxx_std_20)

# Main executable
add_executable(beatrice src/main.cpp)
target_link_libraries(beatrice 
    PRIVATE 
        beatrice_core
        fmt::fmt
)

# Set executable properties
set_target_properties(beatrice PROPERTIES
    OUTPUT_NAME beatrice
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Documentation
if(BUILD_DOCS)
    add_subdirectory(docs)
endif()

# Install rules
install(TARGETS beatrice_core beatrice
    EXPORT BeatriceTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/beatrice
)

# Install headers
install(DIRECTORY include/beatrice/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/beatrice
    FILES_MATCHING PATTERN "*.hpp"
)

# Export targets
install(EXPORT BeatriceTargets
    FILE BeatriceTargets.cmake
    NAMESPACE Beatrice::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Beatrice
)

# Create config file
include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BeatriceConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/BeatriceConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Beatrice
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/BeatriceConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/BeatriceConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/BeatriceConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Beatrice
)

target_include_directories(beatrice_core 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LIBBPF_INCLUDE_DIRS}
)
