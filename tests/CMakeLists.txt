# Testing configuration for Beatrice SDK

# Find GTest
find_package(GTest REQUIRED)

# Enable testing
enable_testing()

# Test executable
add_executable(beatrice_tests
    test_main.cpp
    test_packet.cpp
    test_plugin_manager.cpp
    test_beatrice_context.cpp
    test_af_xdp_backend.cpp
    test_metrics.cpp
    test_config.cpp
    test_error.cpp
)

# Link libraries
target_link_libraries(beatrice_tests
    PRIVATE
        beatrice_core
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        fmt::fmt
)

# Set properties
set_target_properties(beatrice_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

# Add tests
add_test(NAME PacketTests COMMAND beatrice_tests --gtest_filter=PacketTest.*)
add_test(NAME PluginManagerTests COMMAND beatrice_tests --gtest_filter=PluginManagerTest.*)
add_test(NAME BeatriceContextTests COMMAND beatrice_tests --gtest_filter=BeatriceContextTest.*)
add_test(NAME AF_XDPBackendTests COMMAND beatrice_tests --gtest_filter=AF_XDPBackendTest.*)
add_test(NAME MetricsTests COMMAND beatrice_tests --gtest_filter=MetricsTest.*)
add_test(NAME ConfigTests COMMAND beatrice_tests --gtest_filter=ConfigTest.*)
add_test(NAME ErrorTests COMMAND beatrice_tests --gtest_filter=ErrorTest.*)

# Set test properties
set_tests_properties(PacketTests PROPERTIES
    TIMEOUT 30
    LABELS "unit"
)

set_tests_properties(PluginManagerTests PROPERTIES
    TIMEOUT 30
    LABELS "unit"
)

set_tests_properties(BeatriceContextTests PROPERTIES
    TIMEOUT 30
    LABELS "integration"
)

set_tests_properties(AF_XDPBackendTests PROPERTIES
    TIMEOUT 60
    LABELS "integration"
)

set_tests_properties(MetricsTests PROPERTIES
    TIMEOUT 30
    LABELS "unit"
)

set_tests_properties(ConfigTests PROPERTIES
    TIMEOUT 30
    LABELS "unit"
)

set_tests_properties(ErrorTests PROPERTIES
    TIMEOUT 30
    LABELS "unit"
)

# Performance tests
add_executable(beatrice_performance_tests
    performance_tests.cpp
)

target_link_libraries(beatrice_performance_tests
    PRIVATE
        beatrice_core
        GTest::gtest
        GTest::gtest_main
)

set_target_properties(beatrice_performance_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)

add_test(NAME PerformanceTests COMMAND beatrice_performance_tests)
set_tests_properties(PerformanceTests PROPERTIES
    TIMEOUT 300
    LABELS "performance"
)

# Memory leak tests (with Valgrind)
find_program(VALGRIND "valgrind")
if(VALGRIND)
    add_test(NAME MemoryLeakTests COMMAND ${VALGRIND} --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt $<TARGET_FILE:beatrice_tests>)
    set_tests_properties(MemoryLeakTests PROPERTIES
        TIMEOUT 300
        LABELS "memory"
    )
endif()

# Coverage configuration
if(ENABLE_COVERAGE)
    target_compile_options(beatrice_tests PRIVATE --coverage)
    target_link_options(beatrice_tests PRIVATE --coverage)
    
    # Coverage target
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E echo "Generating coverage report..."
        COMMAND lcov --capture --directory . --output-file coverage.info
        COMMAND lcov --remove coverage.info '/usr/*' --output-file coverage.info
        COMMAND lcov --remove coverage.info '*/tests/*' --output-file coverage.info
        COMMAND genhtml coverage.info --output-directory coverage_report
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating coverage report"
    )
endif()

# Install test binaries
install(TARGETS beatrice_tests beatrice_performance_tests
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/tests
) 