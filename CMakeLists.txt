cmake_minimum_required(VERSION 3.20)
project(monitoring_system VERSION 1.0.0 LANGUAGES CXX)

# C++ standard requirements
set(CMAKE_CXX_STANDARD 20)
message(STATUS "Monitoring System: Using C++20 mode")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Make existing helper modules (MonitoringCompatibility, package config) discoverable
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Per-concern modules (decomposed from monolithic CMakeLists.txt - issue #678).
# Each include below has a single responsibility; load order matters: options
# must precede dependency detection (BUILD_WITH_* toggles drive find_package),
# compile_options creates monitoring_system_interface used by targets, sources
# must populate file lists before targets consume them, install must run after
# targets are defined, and summary runs last so all variables have settled.
include(MonitoringCompatibility)
include(cmake/options.cmake)

# Configure compatibility features based on C++ standard. These functions come
# from MonitoringCompatibility.cmake and add MONITORING_HAS_* / MONITORING_USE_*
# definitions that the dependencies and targets modules below expect.
configure_monitoring_compatibility()
setup_performance_features()
setup_threading_features()

include(cmake/dependencies.cmake)
include(cmake/compile_options.cmake)
include(cmake/sources.cmake)
include(cmake/targets.cmake)

# tests/, examples/, benchmarks/, integration_tests/ are split between
# test.cmake (tests + integration_tests) and samples.cmake (examples +
# benchmarks). The original CMakeLists interleaved these as
# tests->examples->benchmarks->integration_tests; the only ordering that
# matters is that all subdirectories run after targets.cmake (so
# monitoring_system is linkable from each one) and before install.cmake.
include(cmake/test.cmake)
include(cmake/samples.cmake)

include(cmake/install.cmake)
include(cmake/summary.cmake)
