cmake_minimum_required(VERSION 3.20)
project(monitoring_system VERSION 2.0.0 LANGUAGES CXX)

# C++20 is required for Result pattern and modern features
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(MONITORING_BUILD_TESTS "Build unit tests" ON)
option(MONITORING_BUILD_EXAMPLES "Build example programs" ON)
option(MONITORING_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(MONITORING_USE_THREAD_SYSTEM "Enable thread_system integration" OFF)
option(MONITORING_USE_LOGGER_SYSTEM "Enable logger_system integration" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

# Export compile commands for tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find dependencies
find_package(Threads REQUIRED)

# Optional dependencies
if(MONITORING_USE_THREAD_SYSTEM)
    find_package(thread_system CONFIG)
    if(thread_system_FOUND)
        message(STATUS "Found thread_system: integration enabled")
        add_compile_definitions(MONITORING_HAS_THREAD_SYSTEM)
    else()
        message(WARNING "thread_system not found: integration disabled")
        set(MONITORING_USE_THREAD_SYSTEM OFF)
    endif()
endif()

if(MONITORING_USE_LOGGER_SYSTEM)
    find_package(logger_system CONFIG)
    if(logger_system_FOUND)
        message(STATUS "Found logger_system: integration enabled")
        add_compile_definitions(MONITORING_HAS_LOGGER_SYSTEM)
    else()
        message(WARNING "logger_system not found: integration disabled")
        set(MONITORING_USE_LOGGER_SYSTEM OFF)
    endif()
endif()

# Create interface library for headers
add_library(monitoring_system_interface INTERFACE)
target_include_directories(monitoring_system_interface
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sources>
        $<INSTALL_INTERFACE:include>
)

# Set compile features
target_compile_features(monitoring_system_interface INTERFACE cxx_std_20)

# Add compile options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(monitoring_system_interface INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Werror=return-type
        -Werror=uninitialized
        -Werror=unused-result
    )
elseif(MSVC)
    target_compile_options(monitoring_system_interface INTERFACE
        /W4
        /WX
        /permissive-
        /Zc:__cplusplus
    )
endif()

# Sanitizers
if(ENABLE_ASAN)
    target_compile_options(monitoring_system_interface INTERFACE -fsanitize=address)
    target_link_options(monitoring_system_interface INTERFACE -fsanitize=address)
endif()

if(ENABLE_TSAN)
    target_compile_options(monitoring_system_interface INTERFACE -fsanitize=thread)
    target_link_options(monitoring_system_interface INTERFACE -fsanitize=thread)
endif()

if(ENABLE_UBSAN)
    target_compile_options(monitoring_system_interface INTERFACE -fsanitize=undefined)
    target_link_options(monitoring_system_interface INTERFACE -fsanitize=undefined)
endif()

# Create monitoring_system library
add_library(monitoring_system STATIC
    sources/monitoring/context/thread_context.cpp
    sources/monitoring/tracing/distributed_tracer.cpp
    sources/monitoring/performance/performance_monitor.cpp
    sources/monitoring/adaptive/adaptive_monitor.cpp
    sources/monitoring/health/health_monitor.cpp
)

target_link_libraries(monitoring_system PUBLIC
    monitoring_system_interface
    Threads::Threads
)

target_include_directories(monitoring_system
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sources>
        $<INSTALL_INTERFACE:include>
)

# Link optional dependencies
if(MONITORING_USE_THREAD_SYSTEM)
    target_link_libraries(monitoring_system INTERFACE thread_system::thread_system)
endif()

if(MONITORING_USE_LOGGER_SYSTEM)
    target_link_libraries(monitoring_system INTERFACE logger_system::logger_system)
endif()

# Tests
if(MONITORING_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Examples
if(MONITORING_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Benchmarks
if(MONITORING_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS monitoring_system monitoring_system_interface
    EXPORT monitoring_system_targets
)

install(DIRECTORY sources/monitoring
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT monitoring_system_targets
    FILE monitoring_system-targets.cmake
    NAMESPACE monitoring_system::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/monitoring_system
)

# Create package configuration
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/monitoring_system-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/monitoring_system-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/monitoring_system
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/monitoring_system-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/monitoring_system-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/monitoring_system-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/monitoring_system
)

# Summary
message(STATUS "")
message(STATUS "monitoring_system configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build tests: ${MONITORING_BUILD_TESTS}")
message(STATUS "  Build examples: ${MONITORING_BUILD_EXAMPLES}")
message(STATUS "  Build benchmarks: ${MONITORING_BUILD_BENCHMARKS}")
message(STATUS "  Thread system integration: ${MONITORING_USE_THREAD_SYSTEM}")
message(STATUS "  Logger system integration: ${MONITORING_USE_LOGGER_SYSTEM}")
message(STATUS "  AddressSanitizer: ${ENABLE_ASAN}")
message(STATUS "  ThreadSanitizer: ${ENABLE_TSAN}")
message(STATUS "  UBSanitizer: ${ENABLE_UBSAN}")
message(STATUS "")