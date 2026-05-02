# targets.cmake
# Library targets for monitoring_system.
#
# Defines and configures:
#   - monitoring_system          (STATIC, the core library)
#   - monitoring_system_modules  (CXX_MODULES library, when MONITORING_ENABLE_MODULES is ON)
#   - monitoring_hardware_plugin (STATIC, when MONITORING_BUILD_HARDWARE_PLUGIN is ON)
#   - monitoring_container_plugin(STATIC, when MONITORING_BUILD_CONTAINER_PLUGIN is ON)
#
# Depends on variables set by dependencies.cmake:
#   MONITORING_THREAD_TARGET, MONITORING_LOGGER_TARGET, MONITORING_NETWORK_TARGET,
#   NETWORK_SYSTEM_INCLUDE_DIR, MONITORING_GRPC_TARGET, MONITORING_PROTOBUF_TARGET
#
# Depends on variables set by sources.cmake:
#   MONITORING_INCLUDE_DIR, MONITORING_SOURCE_DIR, MONITORING_HEADERS, MONITORING_SOURCES,
#   MONITORING_MODULE_SOURCES, MONITORING_HARDWARE_PLUGIN_SOURCES, MONITORING_CONTAINER_PLUGIN_SOURCES
#
# Out-variable consumed by install.cmake:
#   MONITORING_CAN_INSTALL                BOOL true when all PUBLIC deps are IMPORTED
#   _MONITORING_NON_IMPORTED_DEPS         List of dep names that aren't IMPORTED (diagnostic)

##################################################
# Core library: monitoring_system
##################################################

add_library(monitoring_system STATIC
    ${MONITORING_SOURCES}
    ${MONITORING_HEADERS}
)

target_include_directories(monitoring_system
    PUBLIC
        $<BUILD_INTERFACE:${MONITORING_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        $<BUILD_INTERFACE:${MONITORING_SOURCE_DIR}>
)

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
            $<INSTALL_INTERFACE:include>
        PRIVATE
            $<BUILD_INTERFACE:${MONITORING_SOURCE_DIR}>
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

##################################################
# Link optional dependencies (with IMPORTED-target tracking)
##################################################
# MONITORING_CAN_INSTALL is gated to TRUE while all PUBLIC deps are IMPORTED
# (find_package style). It flips to FALSE if any PUBLIC dep is non-IMPORTED
# (add_subdirectory style), since install(EXPORT) cannot record those.
set(MONITORING_CAN_INSTALL TRUE)

if(MONITORING_WITH_THREAD_SYSTEM)
    if(MONITORING_THREAD_TARGET AND TARGET ${MONITORING_THREAD_TARGET})
        target_link_libraries(monitoring_system PUBLIC ${MONITORING_THREAD_TARGET})
        message(STATUS "Monitoring System: Linked to ${MONITORING_THREAD_TARGET} target")
        get_target_property(_thread_imported ${MONITORING_THREAD_TARGET} IMPORTED)
        if(NOT _thread_imported)
            set(MONITORING_CAN_INSTALL FALSE)
            message(WARNING "monitoring_system: Installation disabled because '${MONITORING_THREAD_TARGET}' is not an IMPORTED target. "
                            "This typically happens when dependencies are added via add_subdirectory() instead of find_package(). "
                            "vcpkg builds require IMPORTED targets for proper packaging.")
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
            message(WARNING "monitoring_system: Installation disabled because '${MONITORING_LOGGER_TARGET}' is not an IMPORTED target. "
                            "This typically happens when dependencies are added via add_subdirectory() instead of find_package(). "
                            "vcpkg builds require IMPORTED targets for proper packaging.")
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
            message(WARNING "monitoring_system: Installation disabled because '${MONITORING_NETWORK_TARGET}' is not an IMPORTED target. "
                            "This typically happens when dependencies are added via add_subdirectory() instead of find_package(). "
                            "vcpkg builds require IMPORTED targets for proper packaging.")
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

# Collect non-IMPORTED dependency names for diagnostic messages
set(_MONITORING_NON_IMPORTED_DEPS "")
if(MONITORING_WITH_THREAD_SYSTEM AND MONITORING_THREAD_TARGET AND TARGET ${MONITORING_THREAD_TARGET})
    get_target_property(_imp ${MONITORING_THREAD_TARGET} IMPORTED)
    if(NOT _imp)
        list(APPEND _MONITORING_NON_IMPORTED_DEPS "${MONITORING_THREAD_TARGET}")
    endif()
endif()
if(MONITORING_WITH_LOGGER_SYSTEM AND MONITORING_LOGGER_TARGET AND TARGET ${MONITORING_LOGGER_TARGET})
    get_target_property(_imp ${MONITORING_LOGGER_TARGET} IMPORTED)
    if(NOT _imp)
        list(APPEND _MONITORING_NON_IMPORTED_DEPS "${MONITORING_LOGGER_TARGET}")
    endif()
endif()
if(MONITORING_WITH_NETWORK_SYSTEM AND MONITORING_NETWORK_TARGET AND TARGET ${MONITORING_NETWORK_TARGET})
    get_target_property(_imp ${MONITORING_NETWORK_TARGET} IMPORTED)
    if(NOT _imp)
        list(APPEND _MONITORING_NON_IMPORTED_DEPS "${MONITORING_NETWORK_TARGET}")
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
        ${MONITORING_HARDWARE_PLUGIN_SOURCES}
    )

    target_include_directories(monitoring_hardware_plugin
        PUBLIC
            $<BUILD_INTERFACE:${MONITORING_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            $<BUILD_INTERFACE:${MONITORING_SOURCE_DIR}>
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

##################################################
# Container Monitoring Plugin (Optional)
##################################################
# Build container monitoring plugin as a separate library.
# This keeps container-specific collectors (Docker, Kubernetes, cgroups)
# out of the core library for non-containerized environments.
##################################################
if(MONITORING_BUILD_CONTAINER_PLUGIN)
    message(STATUS "=== Building Container Monitoring Plugin ===")

    add_library(monitoring_container_plugin STATIC
        ${MONITORING_CONTAINER_PLUGIN_SOURCES}
    )

    target_include_directories(monitoring_container_plugin
        PUBLIC
            $<BUILD_INTERFACE:${MONITORING_INCLUDE_DIR}>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            $<BUILD_INTERFACE:${MONITORING_SOURCE_DIR}>
    )

    target_link_libraries(monitoring_container_plugin
        PUBLIC
            monitoring_system
            Threads::Threads
    )

    # Create alias for namespaced usage
    add_library(kcenon::monitoring_container_plugin ALIAS monitoring_container_plugin)

    message(STATUS "Container plugin: ENABLED")
    message(STATUS "  Collectors: docker, cgroup, kubernetes (stub)")
else()
    message(STATUS "Container plugin: DISABLED (use -DMONITORING_BUILD_CONTAINER_PLUGIN=ON to enable)")
endif()
