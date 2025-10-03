# Examples directory CMakeLists.txt

# Include directories for examples
include_directories(
    ${CMAKE_SOURCE_DIR}/sources
    ${CMAKE_SOURCE_DIR}/include
)

# Basic monitoring example
add_executable(basic_monitoring_example
    basic_monitoring_example.cpp
)
target_link_libraries(basic_monitoring_example
    PRIVATE monitoring_system
    Threads::Threads
)

# Distributed tracing example
add_executable(distributed_tracing_example
    distributed_tracing_example.cpp
)
target_link_libraries(distributed_tracing_example
    PRIVATE monitoring_system
    Threads::Threads
)

# Health monitoring and reliability example
add_executable(health_reliability_example
    health_reliability_example.cpp
)
target_link_libraries(health_reliability_example
    PRIVATE monitoring_system
    Threads::Threads
)

# Result pattern example (existing)
add_executable(result_pattern_example
    result_pattern_example.cpp
)
target_link_libraries(result_pattern_example
    PRIVATE monitoring_system
    Threads::Threads
)

# Phase 4 DI Pattern examples - available when common_system integration is enabled
if(BUILD_WITH_COMMON_SYSTEM OR MONITORING_USING_COMMON_INTERFACES)
    # Logger DI Integration example
    add_executable(logger_di_integration_example
        logger_di_integration_example.cpp
    )
    target_link_libraries(logger_di_integration_example
        PRIVATE monitoring_system
        Threads::Threads
    )

    # Monitor Factory Pattern example (TODO: Fix type mismatches)
    # add_executable(monitor_factory_pattern_example
    #     monitor_factory_pattern_example.cpp
    # )
    # target_link_libraries(monitor_factory_pattern_example
    #     PRIVATE monitoring_system
    #     Threads::Threads
    # )

    message(STATUS "Phase 4 DI pattern examples enabled for monitoring_system")
endif()