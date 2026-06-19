# sources.cmake
# Collect source and header files for monitoring_system targets.
#
# Uses CONFIGURE_DEPENDS so CMake re-runs the glob check at build time and
# newly added/removed files are picked up without manually re-running cmake.
#
# Out-variables consumed by targets.cmake:
#   MONITORING_INCLUDE_DIR        Top-level include directory
#   MONITORING_SOURCE_DIR         Top-level src directory
#   MONITORING_HEADERS            All public headers under include/kcenon/monitoring/
#   MONITORING_SOURCES            All .cpp files under src/
#   MONITORING_MODULE_SOURCES     C++20 module .cppm files under src/modules/
#                                 (only set when MONITORING_ENABLE_MODULES is ON)
#   MONITORING_HARDWARE_PLUGIN_SOURCES   Hardware plugin .cpp files
#                                        (only set when MONITORING_BUILD_HARDWARE_PLUGIN is ON)
#   MONITORING_CONTAINER_PLUGIN_SOURCES  Container plugin .cpp files
#                                        (only set when MONITORING_BUILD_CONTAINER_PLUGIN is ON)
#   MONITORING_ADAPTER_HEADERS    Adapter headers under include/kcenon/monitoring/adapters/
#                                 (used by install.cmake for the Development component tag)

# Source layout (canonical, post src/impl consolidation)
set(MONITORING_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(MONITORING_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Collect source files
# CONFIGURE_DEPENDS makes CMake re-run the glob check at build time, so newly
# added/removed files are picked up without manually re-running cmake.
file(GLOB_RECURSE MONITORING_HEADERS CONFIGURE_DEPENDS
    ${MONITORING_INCLUDE_DIR}/kcenon/monitoring/*.h
)

file(GLOB_RECURSE MONITORING_SOURCES CONFIGURE_DEPENDS
    ${MONITORING_SOURCE_DIR}/*.cpp
)

# Module sources (C++20 modules)
if(MONITORING_ENABLE_MODULES)
    # CONFIGURE_DEPENDS picks up new .cppm files on reconfigure without manual list edits.
    file(GLOB_RECURSE MONITORING_MODULE_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/*.cppm
    )
endif()

# Hardware plugin sources (plugin wrapper only).
# Collector implementations (battery, power, temperature, gpu) are already
# compiled into monitoring_system (the main library) via the MONITORING_SOURCES
# GLOB; they are linked transitively through the PUBLIC dependency declared
# in targets.cmake, so the plugin library only needs to compile the plugin
# wrapper itself.
if(MONITORING_BUILD_HARDWARE_PLUGIN)
    file(GLOB_RECURSE MONITORING_HARDWARE_PLUGIN_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/hardware/*.cpp
    )
endif()

# Container plugin sources (plugin wrapper). Underlying collector
# implementations are compiled into monitoring_system via MONITORING_SOURCES
# and linked transitively through the PUBLIC dependency in targets.cmake.
if(MONITORING_BUILD_CONTAINER_PLUGIN)
    file(GLOB_RECURSE MONITORING_CONTAINER_PLUGIN_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/container/*.cpp
    )
endif()

# Adapter headers (used by install.cmake for the Development component tag).
file(GLOB MONITORING_ADAPTER_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/kcenon/monitoring/adapters/*.h
)
