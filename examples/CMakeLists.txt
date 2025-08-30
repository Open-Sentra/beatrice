# Examples configuration for Beatrice SDK

# Simple plugin example
add_library(simple_plugin SHARED
    simple_plugin.cpp
)

# Set properties
set_target_properties(simple_plugin PROPERTIES
    OUTPUT_NAME simple_plugin
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)

# Link libraries
target_link_libraries(simple_plugin
    PRIVATE
        beatrice_core
)

# Set include directories
target_include_directories(simple_plugin
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Set compile features
target_compile_features(simple_plugin PRIVATE cxx_std_20)

# Install plugin
install(TARGETS simple_plugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/beatrice/plugins
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/beatrice/plugins
)

# Plugin test executable
add_executable(plugin_test
    plugin_test.cpp
)

# Set properties
set_target_properties(plugin_test PROPERTIES
    OUTPUT_NAME plugin_test
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)

# Link libraries
target_link_libraries(plugin_test
    PRIVATE
        beatrice_core
)

# Set include directories
target_include_directories(plugin_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Set compile features
target_compile_features(plugin_test PRIVATE cxx_std_20)

# Install test executable
install(TARGETS plugin_test
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/beatrice/examples
)

# Performance test example
add_executable(performance_test
    performance_test.cpp
)

# Set properties
set_target_properties(performance_test PROPERTIES
    OUTPUT_NAME performance_test
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)

# Link libraries
target_link_libraries(performance_test
    PRIVATE
        beatrice_core
)

# Set include directories
target_include_directories(performance_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Set compile features
target_compile_features(performance_test PRIVATE cxx_std_20)

# Install performance test
install(TARGETS performance_test
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/beatrice/examples
)

# Configuration example
add_executable(config_example
    config_example.cpp
)

# Set properties
set_target_properties(config_example PROPERTIES
    OUTPUT_NAME config_example
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)

# Link libraries
target_link_libraries(config_example
    PRIVATE
        beatrice_core
)

# Set include directories
target_include_directories(config_example
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Set compile features
target_compile_features(config_example PRIVATE cxx_std_20)

# Install config example
install(TARGETS config_example
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/beatrice/examples
)
