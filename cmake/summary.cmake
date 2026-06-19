# summary.cmake
# End-of-configure summary for monitoring_system.
#
# Prints a summary of the dependency chain, build configuration, and
# sanitizer/coverage selections. Included last by the top-level
# CMakeLists.txt so all option/dependency variables have settled.

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Monitoring System v${PROJECT_VERSION}")
message(STATUS "========================================")
message(STATUS "")
message(STATUS "Dependency Chain (Tier 3):")
message(STATUS "  monitoring_system")
message(STATUS "    ├── common_system (Tier 0): ${MONITORING_WITH_COMMON_SYSTEM} [REQUIRED]")
message(STATUS "    ├── thread_system (Tier 1): ${MONITORING_WITH_THREAD_SYSTEM} [REQUIRED]")
message(STATUS "    ├── logger_system (Tier 2): ${MONITORING_WITH_LOGGER_SYSTEM} [OPTIONAL - runtime via ILogger]")
message(STATUS "    ├── network_system (Tier 4): ${MONITORING_WITH_NETWORK_SYSTEM} [OPTIONAL]")
message(STATUS "    └── gRPC: ${MONITORING_WITH_GRPC} [OPTIONAL - OTLP gRPC transport]")
message(STATUS "")
message(STATUS "Build Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  C++20 Modules: ${MONITORING_ENABLE_MODULES}")
message(STATUS "  Build tests: ${MONITORING_BUILD_TESTS}")
message(STATUS "  Build integration tests: ${MONITORING_BUILD_INTEGRATION_TESTS}")
message(STATUS "  Build examples: ${MONITORING_BUILD_EXAMPLES}")
message(STATUS "  Build benchmarks: ${MONITORING_BUILD_BENCHMARKS}")
message(STATUS "  Hardware plugin: ${MONITORING_BUILD_HARDWARE_PLUGIN} [OPTIONAL - battery/power/temp/gpu]")
message(STATUS "  Container plugin: ${MONITORING_BUILD_CONTAINER_PLUGIN} [OPTIONAL - docker/k8s/cgroup]")
message(STATUS "")
message(STATUS "Sanitizers:")
message(STATUS "  Coverage reporting: ${MONITORING_ENABLE_COVERAGE}")
message(STATUS "  AddressSanitizer: ${MONITORING_ENABLE_ASAN}")
message(STATUS "  ThreadSanitizer: ${MONITORING_ENABLE_TSAN}")
message(STATUS "  UBSanitizer: ${MONITORING_ENABLE_UBSAN}")
message(STATUS "========================================")
