# CMake generated Testfile for 
# Source directory: /home/yasinldev/Documents/beatrice/tests
# Build directory: /home/yasinldev/Documents/beatrice/build/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[PacketTests]=] "/home/yasinldev/Documents/beatrice/build/tests/beatrice_tests" "--gtest_filter=PacketTest.*")
set_tests_properties([=[PacketTests]=] PROPERTIES  LABELS "unit" TIMEOUT "30" _BACKTRACE_TRIPLES "/home/yasinldev/Documents/beatrice/tests/CMakeLists.txt;36;add_test;/home/yasinldev/Documents/beatrice/tests/CMakeLists.txt;0;")
add_test([=[PluginManagerTests]=] "/home/yasinldev/Documents/beatrice/build/tests/beatrice_tests" "--gtest_filter=PluginManagerTest.*")
set_tests_properties([=[PluginManagerTests]=] PROPERTIES  LABELS "unit" TIMEOUT "30" _BACKTRACE_TRIPLES "/home/yasinldev/Documents/beatrice/tests/CMakeLists.txt;37;add_test;/home/yasinldev/Documents/beatrice/tests/CMakeLists.txt;0;")
add_test([=[BeatriceContextTests]=] "/home/yasinldev/Documents/beatrice/build/tests/beatrice_tests" "--gtest_filter=BeatriceContextTest.*")
set_tests_properties([=[BeatriceContextTests]=] PROPERTIES  LABELS "integration" TIMEOUT "30" _BACKTRACE_TRIPLES "/home/yasinldev/Documents/beatrice/tests/CMakeLists.txt;38;add_test;/home/yasinldev/Documents/beatrice/tests/CMakeLists.txt;0;")
add_test([=[AF_XDPBackendTests]=] "/home/yasinldev/Documents/beatrice/build/tests/beatrice_tests" "--gtest_filter=AF_XDPBackendTest.*")
set_tests_properties([=[AF_XDPBackendTests]=] PROPERTIES  LABELS "integration" TIMEOUT "60" _BACKTRACE_TRIPLES "/home/yasinldev/Documents/beatrice/tests/CMakeLists.txt;39;add_test;/home/yasinldev/Documents/beatrice/tests/CMakeLists.txt;0;")
add_test([=[MetricsTests]=] "/home/yasinldev/Documents/beatrice/build/tests/beatrice_tests" "--gtest_filter=MetricsTest.*")
set_tests_properties([=[MetricsTests]=] PROPERTIES  LABELS "unit" TIMEOUT "30" _BACKTRACE_TRIPLES "/home/yasinldev/Documents/beatrice/tests/CMakeLists.txt;40;add_test;/home/yasinldev/Documents/beatrice/tests/CMakeLists.txt;0;")
add_test([=[ConfigTests]=] "/home/yasinldev/Documents/beatrice/build/tests/beatrice_tests" "--gtest_filter=ConfigTest.*")
set_tests_properties([=[ConfigTests]=] PROPERTIES  LABELS "unit" TIMEOUT "30" _BACKTRACE_TRIPLES "/home/yasinldev/Documents/beatrice/tests/CMakeLists.txt;41;add_test;/home/yasinldev/Documents/beatrice/tests/CMakeLists.txt;0;")
add_test([=[ErrorTests]=] "/home/yasinldev/Documents/beatrice/build/tests/beatrice_tests" "--gtest_filter=ErrorTest.*")
set_tests_properties([=[ErrorTests]=] PROPERTIES  LABELS "unit" TIMEOUT "30" _BACKTRACE_TRIPLES "/home/yasinldev/Documents/beatrice/tests/CMakeLists.txt;42;add_test;/home/yasinldev/Documents/beatrice/tests/CMakeLists.txt;0;")
add_test([=[PerformanceTests]=] "/home/yasinldev/Documents/beatrice/build/tests/beatrice_performance_tests")
set_tests_properties([=[PerformanceTests]=] PROPERTIES  LABELS "performance" TIMEOUT "300" _BACKTRACE_TRIPLES "/home/yasinldev/Documents/beatrice/tests/CMakeLists.txt;96;add_test;/home/yasinldev/Documents/beatrice/tests/CMakeLists.txt;0;")
