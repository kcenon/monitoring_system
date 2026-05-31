# Examples directory CMakeLists.txt

# Include directories for examples
include_directories(
    ${CMAKE_SOURCE_DIR}/sources
    ${CMAKE_SOURCE_DIR}/include
)

# Collect example sources via glob; each example is a single-file executable.
# CONFIGURE_DEPENDS picks up newly-added examples on reconfigure.
file(GLOB MONITORING_EXAMPLE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# Examples that are not currently built. Reasons vary (depend on internal
# headers, proof-of-concept code, type mismatches awaiting fix). Keeping the
# files in-tree but skipping their target registration mirrors the prior
# behaviour after this glob conversion.
list(REMOVE_ITEM MONITORING_EXAMPLE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/bidirectional_di_example.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/event_bus_example.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/facade_adapter_poc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/monitor_factory_pattern_example.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/storage_example.cpp
)

# Examples gated on common_system integration. Removed from the unconditional
# list; re-added below inside the if() block.
set(MONITORING_COMMON_SYSTEM_EXAMPLES
    ${CMAKE_CURRENT_SOURCE_DIR}/logger_di_integration_example.cpp
)
list(REMOVE_ITEM MONITORING_EXAMPLE_SOURCES ${MONITORING_COMMON_SYSTEM_EXAMPLES})

foreach(_example_src IN LISTS MONITORING_EXAMPLE_SOURCES)
    get_filename_component(_example_name "${_example_src}" NAME_WE)
    add_executable(${_example_name} ${_example_src})
    target_link_libraries(${_example_name}
        PRIVATE monitoring_system
        Threads::Threads
    )
endforeach()

# Phase 4 DI Pattern examples - available when common_system integration is enabled
if(MONITORING_WITH_COMMON_SYSTEM OR MONITORING_USING_COMMON_INTERFACES)
    foreach(_example_src IN LISTS MONITORING_COMMON_SYSTEM_EXAMPLES)
        get_filename_component(_example_name "${_example_src}" NAME_WE)
        add_executable(${_example_name} ${_example_src})
        target_link_libraries(${_example_name}
            PRIVATE monitoring_system
            Threads::Threads
        )
    endforeach()

    message(STATUS "Phase 4 DI pattern examples enabled for monitoring_system")
endif()