# dependencies.cmake
# Dependency discovery for monitoring_system.
#
# Discovery order (must match historical evaluation order):
#   1. Threads (REQUIRED)
#   2. common_system (Tier 0, mandatory when MONITORING_WITH_COMMON_SYSTEM is ON)
#   3. thread_system (Tier 1, REQUIRED)
#   4. logger_system (Tier 2, OPTIONAL)
#   5. network_system (Tier 4, OPTIONAL)
#   6. gRPC + Protobuf (OPTIONAL, for OTLP gRPC transport)
#   7. Transport interfaces (IHttpClient, IUdpClient) probe in common_system
#
# Each dependency follows the pattern:
#   - try existing target candidates
#   - find_package(... CONFIG QUIET)
#   - sibling/workspace add_subdirectory fallback (CI relies on this)
#   - header-only fallback for header-bearing components
#
# Out-variables consumed by targets.cmake:
#   COMMON_SYSTEM_INCLUDE_DIR             (when header-only fallback used)
#   MONITORING_THREAD_TARGET              (target name or empty)
#   MONITORING_LOGGER_TARGET              (target name or empty)
#   MONITORING_NETWORK_TARGET             (target name or empty)
#   NETWORK_SYSTEM_INCLUDE_DIR            (when header-only fallback used)
#   MONITORING_GRPC_TARGET                (target name or empty)
#   MONITORING_PROTOBUF_TARGET            (target name or empty)
#   MONITORING_HAS_TRANSPORT_INTERFACES   (BOOL)

# Find Threads (always required)
find_package(Threads REQUIRED)

##################################################
# common_system (Tier 0 - mandatory when ON)
##################################################
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
    foreach(_candidate thread_system utilities thread_system::thread_system thread_system::ThreadSystem ThreadSystem::ThreadSystem)
        if(TARGET ${_candidate})
            set(MONITORING_THREAD_TARGET ${_candidate})
            message(STATUS "Found thread_system target: ${_candidate}")
            break()
        endif()
    endforeach()

    if(NOT MONITORING_THREAD_TARGET)
        find_package(thread_system CONFIG QUIET)
        if(thread_system_FOUND)
            # Check all possible target names from thread_system package
            foreach(_ts_candidate thread_system::thread_system thread_system::ThreadSystem ThreadSystem::ThreadSystem)
                if(TARGET ${_ts_candidate})
                    set(MONITORING_THREAD_TARGET ${_ts_candidate})
                    break()
                endif()
            endforeach()
            if(NOT MONITORING_THREAD_TARGET)
                message(STATUS "thread_system package found but target missing, trying subdirectory fallback")
            endif()
        endif()
    endif()

    # Also try the PascalCase ThreadSystem config (legacy/vcpkg compat)
    if(NOT MONITORING_THREAD_TARGET)
        find_package(ThreadSystem CONFIG QUIET)
        if(ThreadSystem_FOUND AND TARGET ThreadSystem::ThreadSystem)
            set(MONITORING_THREAD_TARGET ThreadSystem::ThreadSystem)
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

                # Suppress deprecation warnings from thread_system (backward compatibility layer)
                if(MSVC)
                    # Get all targets defined in the thread_system directory
                    get_property(_thread_targets DIRECTORY "${_path}" PROPERTY BUILDSYSTEM_TARGETS)
                    foreach(_target ${_thread_targets})
                        if(TARGET ${_target})
                            target_compile_options(${_target} PRIVATE /wd4996)
                            message(STATUS "Suppressed C4996 warnings for thread_system target: ${_target}")
                        endif()
                    endforeach()
                endif()

                if(TARGET utilities)
                    set(MONITORING_THREAD_TARGET utilities)
                elseif(TARGET thread_system)
                    set(MONITORING_THREAD_TARGET thread_system)
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
    foreach(_candidate logger_system logger logger_system::logger_system)
        if(TARGET ${_candidate})
            set(MONITORING_LOGGER_TARGET ${_candidate})
            message(STATUS "Found logger_system target: ${_candidate}")
            break()
        endif()
    endforeach()

    if(NOT MONITORING_LOGGER_TARGET)
        find_package(logger_system CONFIG QUIET)
        if(logger_system_FOUND)
            if(TARGET logger_system::logger_system)
                set(MONITORING_LOGGER_TARGET logger_system::logger_system)
            endif()
        endif()
    endif()

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
                if(TARGET logger_system)
                    set(MONITORING_LOGGER_TARGET logger_system)
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
    foreach(_candidate network_system network_system::network_system)
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
