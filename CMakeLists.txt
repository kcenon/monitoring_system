cmake_minimum_required(VERSION 3.20)
project(monitoring_system VERSION 0.1.0.0 LANGUAGES CXX)

# C++ standard requirements
set(CMAKE_CXX_STANDARD 20)
message(STATUS "Monitoring System: Using C++20 mode")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

##################################################
# C++20 Module Support (Experimental)
##################################################
# C++20 modules require CMake 3.28+ and a compatible compiler.
# This is an opt-in feature for the transition period.
#
# To enable modules:
#   cmake -DMONITORING_ENABLE_MODULES=ON ..
#
# Requirements:
#   - CMake 3.28 or later
#   - GCC 14+, Clang 16+, or MSVC 19.34+
#
# See: https://github.com/kcenon/monitoring_system/issues/310
##################################################
option(MONITORING_ENABLE_MODULES "Enable C++20 module support (requires CMake 3.28+)" OFF)

if(MONITORING_ENABLE_MODULES)
    if(CMAKE_VERSION VERSION_LESS "3.28")
        message(FATAL_ERROR
            "C++20 modules require CMake 3.28 or later.\n"
            "Current CMake version: ${CMAKE_VERSION}\n"
            "Either upgrade CMake or disable modules with -DMONITORING_ENABLE_MODULES=OFF")
    endif()

    # Enable module scanning
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)
    message(STATUS "Monitoring System: C++20 modules ENABLED")
else()
    message(STATUS "Monitoring System: C++20 modules DISABLED (header-based build)")
endif()

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

##################################################
# Dependency Configuration (Option A Structure)
#
# Tier 3: monitoring_system
#   Required: thread_system <- common_system
#   Optional: logger_system (runtime-bound via ILogger interface)
#   Optional: network_system (HTTP metrics export)
#
# Note: logger_system is now optional because monitoring_system uses
# common_system's ILogger interface for runtime binding.
# See Issue #213 for migration details.
##################################################

# Required dependencies (Tier 2 and below)
option(MONITORING_WITH_THREAD_SYSTEM "Enable thread_system integration (REQUIRED)" ON)
option(MONITORING_WITH_LOGGER_SYSTEM "Enable logger_system integration (OPTIONAL - runtime binding via ILogger)" OFF)

# Optional integration (Tier 4 - kept optional to prevent circular dependency)
option(MONITORING_WITH_NETWORK_SYSTEM "Enable network_system integration for HTTP transport (OPTIONAL)" OFF)

# Optional gRPC support for OTLP trace export
option(MONITORING_WITH_GRPC "Enable gRPC support for OTLP trace export (OPTIONAL)" OFF)

# Note: Required dependency validation is performed during detection phase.
# If dependencies are not found, they will be automatically disabled with a warning.

# Optional integration notice (network_system)
if(MONITORING_WITH_NETWORK_SYSTEM)
    message(STATUS "MONITORING_WITH_NETWORK_SYSTEM is enabled - HTTP metrics export available")
    message(STATUS "Note: This creates an optional cross-tier dependency (Tier 3 -> Tier 4)")
endif()

option(MONITORING_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(MONITORING_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(MONITORING_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(MONITORING_ENABLE_COVERAGE "Enable coverage reporting" OFF)

# Optional hardware monitoring plugin (battery, power, temperature, GPU)
# Disabled by default for server environments
option(MONITORING_BUILD_HARDWARE_PLUGIN "Build hardware monitoring plugin" OFF)

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
    # Priority: Workspace-relative paths first (for CI), then sibling, then environment variable
    set(_MONITORING_COMMON_PATHS
        "${CMAKE_CURRENT_SOURCE_DIR}/common_system/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common_system/include"
        "$ENV{COMMON_SYSTEM_ROOT}/include"
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

##################################################
# Required Dependencies Detection
##################################################

# thread_system (REQUIRED - Tier 1)
message(STATUS "=== Finding thread_system (REQUIRED) ===")
set(MONITORING_THREAD_TARGET "")
if(MONITORING_WITH_THREAD_SYSTEM)
    foreach(_candidate ThreadSystem utilities thread_system::thread_system)
        if(TARGET ${_candidate})
            set(MONITORING_THREAD_TARGET ${_candidate})
            message(STATUS "Found thread_system target: ${_candidate}")
            break()
        endif()
    endforeach()

    if(NOT MONITORING_THREAD_TARGET)
        find_package(thread_system CONFIG QUIET)
        if(thread_system_FOUND)
            # Verify the target actually exists (config may have errors)
            if(TARGET thread_system::thread_system)
                set(MONITORING_THREAD_TARGET thread_system::thread_system)
            elseif(TARGET ThreadSystem)
                set(MONITORING_THREAD_TARGET ThreadSystem)
            else()
                message(STATUS "thread_system package found but target missing, trying subdirectory fallback")
            endif()
        endif()
    endif()

    # Subdirectory/sibling fallback (preferred for CI where targets may be missing)
    # Priority: Workspace-relative paths first (for CI), then sibling, then environment variable
    if(NOT MONITORING_THREAD_TARGET)
        set(_THREAD_PATHS
            "${CMAKE_CURRENT_SOURCE_DIR}/thread_system"
            "${CMAKE_CURRENT_SOURCE_DIR}/../thread_system"
            "$ENV{THREAD_SYSTEM_ROOT}"
        )

        foreach(_path ${_THREAD_PATHS})
            if(EXISTS "${_path}/CMakeLists.txt")
                message(STATUS "Found thread_system at: ${_path}")
                set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
                set(BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
                add_subdirectory("${_path}" "${CMAKE_BINARY_DIR}/thread_system_build")
                if(TARGET utilities)
                    set(MONITORING_THREAD_TARGET utilities)
                elseif(TARGET ThreadSystem)
                    set(MONITORING_THREAD_TARGET ThreadSystem)
                endif()
                break()
            endif()
        endforeach()
    endif()

    if(MONITORING_THREAD_TARGET)
        message(STATUS "thread_system integration: ENABLED (${MONITORING_THREAD_TARGET})")
        add_compile_definitions(MONITORING_HAS_THREAD_SYSTEM)
    else()
        message(WARNING
            "thread_system is REQUIRED but not found.\n"
            "Please ensure thread_system is available:\n"
            "  1. As sibling directory: ../thread_system/\n"
            "  2. As installed package\n"
            "  3. Via THREAD_SYSTEM_ROOT environment variable\n"
            "Disabling thread_system integration for standalone build.")
        set(MONITORING_WITH_THREAD_SYSTEM OFF)
    endif()
endif()

# logger_system (OPTIONAL - Tier 2, runtime-bound via ILogger interface)
message(STATUS "=== Finding logger_system (OPTIONAL) ===")
set(MONITORING_LOGGER_TARGET "")
if(MONITORING_WITH_LOGGER_SYSTEM)
    foreach(_candidate LoggerSystem logger logger_system::logger_system)
        if(TARGET ${_candidate})
            set(MONITORING_LOGGER_TARGET ${_candidate})
            message(STATUS "Found logger_system target: ${_candidate}")
            break()
        endif()
    endforeach()

    # Subdirectory/sibling fallback (preferred for CI where targets file may be missing)
    # Priority: Workspace-relative paths first (for CI), then sibling, then environment variable
    if(NOT MONITORING_LOGGER_TARGET)
        set(_LOGGER_PATHS
            "${CMAKE_CURRENT_SOURCE_DIR}/logger_system"
            "${CMAKE_CURRENT_SOURCE_DIR}/../logger_system"
            "$ENV{LOGGER_SYSTEM_ROOT}"
        )

        foreach(_path ${_LOGGER_PATHS})
            if(EXISTS "${_path}/CMakeLists.txt")
                message(STATUS "Found logger_system at: ${_path}")
                set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
                set(BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
                add_subdirectory("${_path}" "${CMAKE_BINARY_DIR}/logger_system_build")
                if(TARGET LoggerSystem)
                    set(MONITORING_LOGGER_TARGET LoggerSystem)
                elseif(TARGET logger)
                    set(MONITORING_LOGGER_TARGET logger)
                endif()
                break()
            endif()
        endforeach()
    endif()

    if(MONITORING_LOGGER_TARGET)
        message(STATUS "logger_system integration: ENABLED (${MONITORING_LOGGER_TARGET})")
        add_compile_definitions(MONITORING_HAS_LOGGER_SYSTEM)
    else()
        message(STATUS
            "logger_system not found - using runtime binding via ILogger interface.\n"
            "Note: logger_system can still be provided at runtime via dependency injection.")
        set(MONITORING_WITH_LOGGER_SYSTEM OFF)
    endif()
endif()

# Always define logging capability via common_system ILogger interface
message(STATUS "Logging: Using common_system ILogger interface (runtime binding)")

##################################################
# Optional Dependencies Detection
##################################################

# network_system (OPTIONAL - Tier 4, for HTTP transport)
# Note: Kept optional to prevent circular dependency with network_system
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
    # Priority: Workspace-relative paths first (for CI), then sibling, then environment variable
    if(NOT MONITORING_NETWORK_TARGET)
        set(_MONITORING_NETWORK_PATHS
            "${CMAKE_CURRENT_SOURCE_DIR}/network_system/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/../network_system/include"
            "$ENV{NETWORK_SYSTEM_ROOT}/include"
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

# gRPC (OPTIONAL - for OTLP gRPC transport)
message(STATUS "=== Finding gRPC (OPTIONAL) ===")
set(MONITORING_GRPC_TARGET "")
set(MONITORING_PROTOBUF_TARGET "")
if(MONITORING_WITH_GRPC)
    # Find gRPC package
    find_package(gRPC CONFIG QUIET)
    if(gRPC_FOUND)
        message(STATUS "Found gRPC package")
        set(MONITORING_GRPC_TARGET gRPC::grpc++)
    endif()

    # Find Protobuf package
    find_package(Protobuf CONFIG QUIET)
    if(Protobuf_FOUND)
        message(STATUS "Found Protobuf package")
        set(MONITORING_PROTOBUF_TARGET protobuf::libprotobuf)
    endif()

    if(MONITORING_GRPC_TARGET AND MONITORING_PROTOBUF_TARGET)
        message(STATUS "gRPC integration: ENABLED")
        add_compile_definitions(MONITORING_HAS_GRPC)
    else()
        message(STATUS "gRPC or Protobuf not found: OTLP gRPC transport will use stub implementation")
        message(STATUS "  To enable gRPC support, install via vcpkg: vcpkg install monitoring-system[grpc]")
        set(MONITORING_WITH_GRPC OFF)
    endif()
else()
    message(STATUS "gRPC integration: DISABLED (use -DMONITORING_WITH_GRPC=ON to enable)")
endif()

##################################################
# Transport Interface Detection
##################################################

# Check for common_system transport interfaces (IHttpClient, IUdpClient)
# These interfaces enable dependency injection for network communication
message(STATUS "=== Checking for transport interfaces ===")
set(MONITORING_HAS_TRANSPORT_INTERFACES OFF)

if(MONITORING_WITH_COMMON_SYSTEM)
    # Check multiple possible locations for transport.h
    set(_TRANSPORT_INTERFACE_PATHS
        "${COMMON_SYSTEM_INCLUDE_DIR}/kcenon/common/interfaces/transport.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common_system/include/kcenon/common/interfaces/transport.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/common_system/include/kcenon/common/interfaces/transport.h"
    )

    foreach(_path ${_TRANSPORT_INTERFACE_PATHS})
        if(EXISTS "${_path}")
            message(STATUS "Found transport interfaces at: ${_path}")
            set(MONITORING_HAS_TRANSPORT_INTERFACES ON)
            add_compile_definitions(MONITORING_HAS_COMMON_TRANSPORT_INTERFACES)
            break()
        endif()
    endforeach()

    if(MONITORING_HAS_TRANSPORT_INTERFACES)
        message(STATUS "Transport interfaces: ENABLED (IHttpClient, IUdpClient available)")
    else()
        message(STATUS "Transport interfaces: NOT FOUND (using local transport implementations)")
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

# Find common_system target with multiple possible names
# Priority: namespaced alias first, then direct target name
# This handles both find_package() and FetchContent scenarios
set(_COMMON_SYSTEM_TARGET "")
foreach(_candidate kcenon::common_system kcenon::common common_system common)
    if(TARGET ${_candidate})
        set(_COMMON_SYSTEM_TARGET ${_candidate})
        message(STATUS "Found common_system target: ${_candidate}")
        break()
    endif()
endforeach()

# Add common_system dependency
if(_COMMON_SYSTEM_TARGET)
    target_link_libraries(monitoring_system_interface INTERFACE ${_COMMON_SYSTEM_TARGET})
elseif(TARGET common_system AND NOT TARGET kcenon::common_system)
    # Create alias if the base target exists but namespaced alias doesn't
    # This can happen when common_system is added via FetchContent
    add_library(kcenon::common_system ALIAS common_system)
    target_link_libraries(monitoring_system_interface INTERFACE kcenon::common_system)
    message(STATUS "Created kcenon::common_system alias for common_system target")
elseif(DEFINED COMMON_SYSTEM_INCLUDE_DIR)
    # Fallback to header-only integration using include directories
    target_include_directories(monitoring_system_interface
        INTERFACE
            $<BUILD_INTERFACE:${COMMON_SYSTEM_INCLUDE_DIR}>
    )
    message(STATUS "Using common_system headers from: ${COMMON_SYSTEM_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "common_system is required but no suitable target or include directory was found.")
endif()

target_compile_definitions(monitoring_system_interface
    INTERFACE
        KCENON_HAS_COMMON_SYSTEM=1
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
        -Wdeprecated-declarations
        -Werror=return-type
        -Werror=uninitialized
        -Werror=unused-result
    )
    
    # Add SIMD support based on architecture
    include(CheckCXXCompilerFlag)

    # Detect architecture - only enable AVX2 on x86/x64
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i[3-6]86")
        check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
        if(COMPILER_SUPPORTS_AVX2)
            target_compile_options(monitoring_system_interface INTERFACE -mavx2)
            target_compile_definitions(monitoring_system_interface INTERFACE SIMD_AVX2_AVAILABLE)
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        # ARM64 uses NEON, which is enabled by default on AArch64
        target_compile_definitions(monitoring_system_interface INTERFACE SIMD_NEON_AVAILABLE)
    endif()
elseif(MSVC)
    target_compile_options(monitoring_system_interface INTERFACE
        /W4
        /WX
        /permissive-
        /Zc:__cplusplus
        /w14996  # Enable deprecated declarations warning
    )

    # Enable AVX2 on MSVC for x64 architecture only
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
        target_compile_options(monitoring_system_interface INTERFACE /arch:AVX2)
        target_compile_definitions(monitoring_system_interface INTERFACE SIMD_AVX2_AVAILABLE)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64")
        target_compile_definitions(monitoring_system_interface INTERFACE SIMD_NEON_AVAILABLE)
    endif()
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
            src/impl/container_collector.cpp
            src/platform/linux_metrics.cpp
            src/platform/windows_metrics.cpp
            src/platform/cgroup_metrics.cpp
            src/platform/docker_metrics.cpp
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
        src/impl/container_collector.cpp
        src/platform/linux_metrics.cpp
        src/platform/windows_metrics.cpp
        src/platform/cgroup_metrics.cpp
        src/platform/docker_metrics.cpp
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

##################################################
# C++20 Module Sources
##################################################
# Add module sources when MONITORING_ENABLE_MODULES is ON
# This creates a separate library for module-based usage
#
# Module structure:
#   kcenon.monitoring          - Primary module interface
#   kcenon.monitoring.core     - Core types and interfaces
#   kcenon.monitoring.collectors - Metric collectors
#   kcenon.monitoring.adaptive - Adaptive monitoring
##################################################
if(MONITORING_ENABLE_MODULES)
    # Define module source files
    set(MONITORING_MODULE_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/monitoring.cppm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/core.cppm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/collectors.cppm
        ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/adaptive.cppm
    )

    # Create module library
    add_library(monitoring_system_modules)

    # Add module sources using FILE_SET
    target_sources(monitoring_system_modules
        PUBLIC FILE_SET CXX_MODULES
        FILES ${MONITORING_MODULE_SOURCES}
    )

    target_include_directories(monitoring_system_modules
        PUBLIC
            $<BUILD_INTERFACE:${MONITORING_INCLUDE_DIR}>
            $<BUILD_INTERFACE:${MONITORING_SOURCE_DIR}>
            $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(monitoring_system_modules
        PUBLIC
            monitoring_system_interface
            Threads::Threads
    )

    # Link to same dependencies as main library
    if(MONITORING_WITH_THREAD_SYSTEM AND MONITORING_THREAD_TARGET)
        target_link_libraries(monitoring_system_modules PUBLIC ${MONITORING_THREAD_TARGET})
    endif()

    if(MONITORING_WITH_LOGGER_SYSTEM AND MONITORING_LOGGER_TARGET)
        target_link_libraries(monitoring_system_modules PUBLIC ${MONITORING_LOGGER_TARGET})
    endif()

    # Create alias for namespaced usage
    add_library(kcenon::monitoring_modules ALIAS monitoring_system_modules)

    message(STATUS "Monitoring System: Module library created (monitoring_system_modules)")
    message(STATUS "  Module files: ${MONITORING_MODULE_SOURCES}")
endif()

# Platform-specific libraries for temperature monitoring
if(APPLE)
    target_link_libraries(monitoring_system PUBLIC
        "-framework IOKit"
        "-framework CoreFoundation"
    )
endif()

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
        message(STATUS "MONITORING_WITH_LOGGER_SYSTEM is ON but no logger_system target found. "
                       "Falling back to runtime binding via ILogger interface.")
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

if(MONITORING_WITH_GRPC)
    if(MONITORING_GRPC_TARGET AND TARGET ${MONITORING_GRPC_TARGET})
        target_link_libraries(monitoring_system PUBLIC ${MONITORING_GRPC_TARGET})
        message(STATUS "Monitoring System: Linked to ${MONITORING_GRPC_TARGET} target")
    endif()
    if(MONITORING_PROTOBUF_TARGET AND TARGET ${MONITORING_PROTOBUF_TARGET})
        target_link_libraries(monitoring_system PUBLIC ${MONITORING_PROTOBUF_TARGET})
        message(STATUS "Monitoring System: Linked to ${MONITORING_PROTOBUF_TARGET} target")
    endif()
endif()

##################################################
# Hardware Monitoring Plugin (Optional)
##################################################
# Build hardware monitoring plugin as a separate library.
# This keeps hardware-specific collectors (battery, power, temperature, GPU)
# out of the core library for server environments where they are unnecessary.
##################################################
if(MONITORING_BUILD_HARDWARE_PLUGIN)
    message(STATUS "=== Building Hardware Monitoring Plugin ===")

    add_library(monitoring_hardware_plugin STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/hardware/hardware_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/battery_collector.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/power_collector.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/temperature_collector.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/gpu_collector.cpp
    )

    target_include_directories(monitoring_hardware_plugin
        PUBLIC
            $<BUILD_INTERFACE:${MONITORING_INCLUDE_DIR}>
            $<BUILD_INTERFACE:${MONITORING_SOURCE_DIR}>
            $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(monitoring_hardware_plugin
        PUBLIC
            monitoring_system
            Threads::Threads
    )

    # Platform-specific libraries for hardware monitoring
    if(APPLE)
        target_link_libraries(monitoring_hardware_plugin PUBLIC
            "-framework IOKit"
            "-framework CoreFoundation"
        )
    endif()

    # Create alias for namespaced usage
    add_library(kcenon::monitoring_hardware_plugin ALIAS monitoring_hardware_plugin)

    message(STATUS "Hardware plugin: ENABLED")
    message(STATUS "  Collectors: battery, power, temperature, gpu")
else()
    message(STATUS "Hardware plugin: DISABLED (use -DMONITORING_BUILD_HARDWARE_PLUGIN=ON to enable)")
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

##################################################
# Build Summary
##################################################

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
message(STATUS "")
message(STATUS "Sanitizers:")
message(STATUS "  Coverage reporting: ${MONITORING_ENABLE_COVERAGE}")
message(STATUS "  AddressSanitizer: ${MONITORING_ENABLE_ASAN}")
message(STATUS "  ThreadSanitizer: ${MONITORING_ENABLE_TSAN}")
message(STATUS "  UBSanitizer: ${MONITORING_ENABLE_UBSAN}")
message(STATUS "========================================")
