cmake_minimum_required(VERSION 3.20)
project(monitoring_system VERSION 1.0.0 LANGUAGES CXX)

# C++ standard requirements
set(CMAKE_CXX_STANDARD 20)
message(STATUS "Monitoring System: Using C++20 mode")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include compatibility module for C++17/C++20 feature detection
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(MonitoringCompatibility)

# Configure compatibility features based on C++ standard
configure_monitoring_compatibility()
setup_performance_features()
setup_threading_features()

# Options - All prefixed with MONITORING_ for consistency
option(MONITORING_BUILD_TESTS "Build unit tests" ON)
option(MONITORING_BUILD_INTEGRATION_TESTS "Build integration tests" ON)
option(MONITORING_BUILD_EXAMPLES "Build example programs" ON)
option(MONITORING_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(MONITORING_WITH_COMMON_SYSTEM "Enable common_system integration" ON)
option(MONITORING_WITH_THREAD_SYSTEM "Enable thread_system integration" OFF)
option(MONITORING_WITH_LOGGER_SYSTEM "Enable logger_system integration" OFF)
option(MONITORING_WITH_NETWORK_SYSTEM "Enable network_system integration for HTTP transport" OFF)
option(MONITORING_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(MONITORING_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(MONITORING_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(MONITORING_ENABLE_COVERAGE "Enable coverage reporting" OFF)

# Backward compatibility for legacy option names (can be removed in future versions)
include(MonitoringLegacyOptions OPTIONAL)

# Export compile commands for tools
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find dependencies
find_package(Threads REQUIRED)

# common_system dependency (mandatory when MONITORING_WITH_COMMON_SYSTEM is ON)
if(MONITORING_WITH_COMMON_SYSTEM)
find_package(common_system CONFIG QUIET)
if(NOT common_system_FOUND)
    # Check for common_system in multiple locations
    # Priority: Environment variable, then relative paths as fallback
    set(_MONITORING_COMMON_PATHS
        "$ENV{COMMON_SYSTEM_ROOT}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common_system/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/common_system/include"
    )

    foreach(_path ${_MONITORING_COMMON_PATHS})
        if(EXISTS "${_path}/kcenon/common/interfaces/monitoring_interface.h")
            message(STATUS "Found common_system at: ${_path}")
            set(COMMON_SYSTEM_INCLUDE_DIR "${_path}")
            break()
        endif()
    endforeach()

    if(NOT DEFINED COMMON_SYSTEM_INCLUDE_DIR)
        message(FATAL_ERROR "common_system is required but was not found. Please ensure common_system is available.")
    endif()
else()
    message(STATUS "Found common_system package")
endif()
endif() # MONITORING_WITH_COMMON_SYSTEM

# Optional dependencies
set(MONITORING_THREAD_TARGET "")
if(MONITORING_WITH_THREAD_SYSTEM)
    foreach(_candidate ThreadSystem utilities thread_system::thread_system)
        if(TARGET ${_candidate})
            set(MONITORING_THREAD_TARGET ${_candidate})
            break()
        endif()
    endforeach()

    if(NOT MONITORING_THREAD_TARGET)
        find_package(thread_system CONFIG QUIET)
        if(thread_system_FOUND)
            set(MONITORING_THREAD_TARGET thread_system::thread_system)
        endif()
    endif()

    if(MONITORING_THREAD_TARGET)
        message(STATUS "Found thread_system: integration enabled (${MONITORING_THREAD_TARGET})")
        add_compile_definitions(MONITORING_HAS_THREAD_SYSTEM)
    else()
        message(WARNING "thread_system not found: integration disabled")
        set(MONITORING_WITH_THREAD_SYSTEM OFF)
    endif()
endif()

set(MONITORING_LOGGER_TARGET "")
if(MONITORING_WITH_LOGGER_SYSTEM)
    foreach(_candidate LoggerSystem logger logger_system::logger_system)
        if(TARGET ${_candidate})
            set(MONITORING_LOGGER_TARGET ${_candidate})
            break()
        endif()
    endforeach()

    if(NOT MONITORING_LOGGER_TARGET)
        find_package(logger_system CONFIG QUIET)
        if(logger_system_FOUND)
            set(MONITORING_LOGGER_TARGET logger_system::logger_system)
        endif()
    endif()

    if(MONITORING_LOGGER_TARGET)
        message(STATUS "Found logger_system: integration enabled (${MONITORING_LOGGER_TARGET})")
        add_compile_definitions(MONITORING_HAS_LOGGER_SYSTEM)
    else()
        message(WARNING "logger_system not found: integration disabled")
        set(MONITORING_WITH_LOGGER_SYSTEM OFF)
    endif()
endif()

set(MONITORING_NETWORK_TARGET "")
if(MONITORING_WITH_NETWORK_SYSTEM)
    foreach(_candidate NetworkSystem network_system network_system::network_system)
        if(TARGET ${_candidate})
            set(MONITORING_NETWORK_TARGET ${_candidate})
            break()
        endif()
    endforeach()

    if(NOT MONITORING_NETWORK_TARGET)
        find_package(network_system CONFIG QUIET)
        if(network_system_FOUND)
            set(MONITORING_NETWORK_TARGET network_system::network_system)
        endif()
    endif()

    # Try to find network_system include directory if package not found
    if(NOT MONITORING_NETWORK_TARGET)
        set(_MONITORING_NETWORK_PATHS
            "$ENV{NETWORK_SYSTEM_ROOT}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/../network_system/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/network_system/include"
        )

        foreach(_path ${_MONITORING_NETWORK_PATHS})
            if(EXISTS "${_path}/kcenon/network/core/http_client.h")
                message(STATUS "Found network_system headers at: ${_path}")
                set(NETWORK_SYSTEM_INCLUDE_DIR "${_path}")
                break()
            endif()
        endforeach()
    endif()

    if(MONITORING_NETWORK_TARGET OR DEFINED NETWORK_SYSTEM_INCLUDE_DIR)
        message(STATUS "Found network_system: HTTP transport enabled")
        add_compile_definitions(MONITORING_HAS_NETWORK_SYSTEM)
    else()
        message(WARNING "network_system not found: HTTP transport will use stub implementation")
        set(MONITORING_WITH_NETWORK_SYSTEM OFF)
    endif()
endif()

# Create interface library for headers
add_library(monitoring_system_interface INTERFACE)
target_include_directories(monitoring_system_interface
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sources>
        $<INSTALL_INTERFACE:include>
)

# common_system integration is now mandatory (Phase 2.3)
message(STATUS "Monitoring System: Using common_system interfaces")

# Add common_system include path
if(common_system_FOUND)
    target_link_libraries(monitoring_system_interface INTERFACE kcenon::common_system)
else()
    # Use sibling directory
    target_include_directories(monitoring_system_interface
        INTERFACE
            $<BUILD_INTERFACE:${COMMON_SYSTEM_INCLUDE_DIR}>
    )
endif()

target_compile_definitions(monitoring_system_interface
    INTERFACE
        BUILD_WITH_COMMON_SYSTEM
        MONITORING_USING_COMMON_INTERFACES
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
    
    # Add AVX2 support if available (with fallback)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
    if(COMPILER_SUPPORTS_AVX2)
        target_compile_options(monitoring_system_interface INTERFACE -mavx2)
        target_compile_definitions(monitoring_system_interface INTERFACE SIMD_AVX2_AVAILABLE)
    endif()
elseif(MSVC)
    target_compile_options(monitoring_system_interface INTERFACE
        /W4
        /WX
        /permissive-
        /Zc:__cplusplus
    )
    
    # Enable AVX2 on MSVC if supported
    target_compile_options(monitoring_system_interface INTERFACE /arch:AVX2)
    target_compile_definitions(monitoring_system_interface INTERFACE SIMD_AVX2_AVAILABLE)
endif()

# Sanitizers
if(MONITORING_ENABLE_ASAN)
    target_compile_options(monitoring_system_interface INTERFACE -fsanitize=address)
    target_link_options(monitoring_system_interface INTERFACE -fsanitize=address)
endif()

if(MONITORING_ENABLE_TSAN)
    target_compile_options(monitoring_system_interface INTERFACE -fsanitize=thread)
    target_link_options(monitoring_system_interface INTERFACE -fsanitize=thread)
endif()

if(MONITORING_ENABLE_UBSAN)
    target_compile_options(monitoring_system_interface INTERFACE -fsanitize=undefined)
    target_link_options(monitoring_system_interface INTERFACE -fsanitize=undefined)
endif()

# Check for new structure
set(MONITORING_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(MONITORING_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(EXISTS ${MONITORING_INCLUDE_DIR}/kcenon/monitoring AND EXISTS ${MONITORING_SOURCE_DIR})
    message(STATUS "Monitoring System: Using new directory structure")

    # Collect source files from new structure
    file(GLOB_RECURSE MONITORING_HEADERS
        ${MONITORING_INCLUDE_DIR}/kcenon/monitoring/*.h
    )

    file(GLOB_RECURSE MONITORING_SOURCES
        ${MONITORING_SOURCE_DIR}/*.cpp
    )

    # Check if new structure has sources
    list(LENGTH MONITORING_SOURCES SOURCE_COUNT)
    if(SOURCE_COUNT GREATER 0)
        # Create library with new structure
        add_library(monitoring_system STATIC
            ${MONITORING_SOURCES}
            ${MONITORING_HEADERS}
        )

        target_include_directories(monitoring_system
            PUBLIC
                $<BUILD_INTERFACE:${MONITORING_INCLUDE_DIR}>
                $<BUILD_INTERFACE:${MONITORING_SOURCE_DIR}>
                $<INSTALL_INTERFACE:include>
        )
    else()
        # Fall back to legacy structure
        message(STATUS "Monitoring System: New structure incomplete, using src sources")
        add_library(monitoring_system STATIC
            src/context/thread_context.cpp
            src/impl/tracing/distributed_tracer.cpp
            src/core/performance_monitor.cpp
            src/core/thread_local_buffer.cpp
            src/core/central_collector.cpp
            src/impl/adaptive_monitor.cpp
            src/platform/linux_metrics.cpp
            src/platform/windows_metrics.cpp
        )

        target_include_directories(monitoring_system
            PUBLIC
                $<BUILD_INTERFACE:${MONITORING_SOURCE_DIR}>
                $<BUILD_INTERFACE:${MONITORING_INCLUDE_DIR}>
                $<INSTALL_INTERFACE:include>
        )
    endif()
else()
    # Use legacy structure
    message(STATUS "Monitoring System: Using legacy directory structure")
    add_library(monitoring_system STATIC
        src/context/thread_context.cpp
        src/impl/tracing/distributed_tracer.cpp
        src/core/performance_monitor.cpp
        src/core/thread_local_buffer.cpp
        src/core/central_collector.cpp
        src/impl/adaptive_monitor.cpp
        src/platform/linux_metrics.cpp
        src/platform/windows_metrics.cpp
    )

    target_include_directories(monitoring_system
        PUBLIC
            $<BUILD_INTERFACE:${MONITORING_SOURCE_DIR}>
            $<BUILD_INTERFACE:${MONITORING_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:include>
    )
endif()

target_link_libraries(monitoring_system PUBLIC
    monitoring_system_interface
    Threads::Threads
)

# Setup formatting library (std::format, fmt, or basic fallback)
if(COMMAND setup_monitoring_formatting)
    setup_monitoring_formatting(monitoring_system)
endif()

# Link optional dependencies with proper target detection
set(MONITORING_CAN_INSTALL TRUE)

if(MONITORING_WITH_THREAD_SYSTEM)
    if(MONITORING_THREAD_TARGET AND TARGET ${MONITORING_THREAD_TARGET})
        target_link_libraries(monitoring_system PUBLIC ${MONITORING_THREAD_TARGET})
        message(STATUS "Monitoring System: Linked to ${MONITORING_THREAD_TARGET} target")
        get_target_property(_thread_imported ${MONITORING_THREAD_TARGET} IMPORTED)
        if(NOT _thread_imported)
            set(MONITORING_CAN_INSTALL FALSE)
        endif()
    else()
        message(FATAL_ERROR "MONITORING_WITH_THREAD_SYSTEM is ON but no thread_system target found.")
    endif()
endif()

if(MONITORING_WITH_LOGGER_SYSTEM)
    if(MONITORING_LOGGER_TARGET AND TARGET ${MONITORING_LOGGER_TARGET})
        target_link_libraries(monitoring_system PUBLIC ${MONITORING_LOGGER_TARGET})
        message(STATUS "Monitoring System: Linked to ${MONITORING_LOGGER_TARGET} target")
        get_target_property(_logger_imported ${MONITORING_LOGGER_TARGET} IMPORTED)
        if(NOT _logger_imported)
            set(MONITORING_CAN_INSTALL FALSE)
        endif()
    else()
        message(WARNING "MONITORING_WITH_LOGGER_SYSTEM is ON but no logger_system target found. "
                        "Logger integration will be disabled.")
        set(MONITORING_WITH_LOGGER_SYSTEM OFF)
    endif()
endif()

if(MONITORING_WITH_NETWORK_SYSTEM)
    if(MONITORING_NETWORK_TARGET AND TARGET ${MONITORING_NETWORK_TARGET})
        target_link_libraries(monitoring_system PUBLIC ${MONITORING_NETWORK_TARGET})
        message(STATUS "Monitoring System: Linked to ${MONITORING_NETWORK_TARGET} target")
        get_target_property(_network_imported ${MONITORING_NETWORK_TARGET} IMPORTED)
        if(NOT _network_imported)
            set(MONITORING_CAN_INSTALL FALSE)
        endif()
    elseif(DEFINED NETWORK_SYSTEM_INCLUDE_DIR)
        # Header-only integration
        target_include_directories(monitoring_system PUBLIC ${NETWORK_SYSTEM_INCLUDE_DIR})
        message(STATUS "Monitoring System: Using network_system headers from ${NETWORK_SYSTEM_INCLUDE_DIR}")
    else()
        message(WARNING "MONITORING_WITH_NETWORK_SYSTEM is ON but no network_system found. "
                        "Network integration will be disabled.")
        set(MONITORING_WITH_NETWORK_SYSTEM OFF)
    endif()
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

# Integration Tests
if(MONITORING_BUILD_INTEGRATION_TESTS)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/integration_tests/CMakeLists.txt)
        enable_testing()
        add_subdirectory(integration_tests)
    else()
        message(STATUS "Integration tests requested, but CMakeLists.txt not found in integration_tests directory. Skipping.")
    endif()
endif()

if(MONITORING_CAN_INSTALL)
    # Installation
    include(GNUInstallDirs)

    install(TARGETS monitoring_system monitoring_system_interface
        EXPORT monitoring_system_targets
    )

    install(DIRECTORY include/kcenon/monitoring
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kcenon
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

    # Install adapter headers if MONITORING_WITH_COMMON_SYSTEM is enabled
    if(MONITORING_WITH_COMMON_SYSTEM)
        install(FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/include/kcenon/monitoring/adapters/common_monitor_adapter.h
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kcenon/monitoring/adapters
            COMPONENT Development
        )
    endif()

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/monitoring_system-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/monitoring_system-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/monitoring_system
    )
else()
    message(STATUS "Monitoring System: Skipping install/export (local dependency targets cannot be exported)")
endif()

# Summary
message(STATUS "")
message(STATUS "monitoring_system configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build tests: ${MONITORING_BUILD_TESTS}")
message(STATUS "  Build integration tests: ${MONITORING_BUILD_INTEGRATION_TESTS}")
message(STATUS "  Build examples: ${MONITORING_BUILD_EXAMPLES}")
message(STATUS "  Build benchmarks: ${MONITORING_BUILD_BENCHMARKS}")
message(STATUS "  Thread system integration: ${MONITORING_WITH_THREAD_SYSTEM}")
message(STATUS "  Logger system integration: ${MONITORING_WITH_LOGGER_SYSTEM}")
message(STATUS "  Network system integration: ${MONITORING_WITH_NETWORK_SYSTEM}")
message(STATUS "  Coverage reporting: ${MONITORING_ENABLE_COVERAGE}")
message(STATUS "  AddressSanitizer: ${MONITORING_ENABLE_ASAN}")
message(STATUS "  ThreadSanitizer: ${MONITORING_ENABLE_TSAN}")
message(STATUS "  UBSanitizer: ${MONITORING_ENABLE_UBSAN}")
message(STATUS "")
