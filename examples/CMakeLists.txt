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

# Custom metric types example (histogram, summary, timer)
add_executable(custom_metric_types_example
    custom_metric_types_example.cpp
)
target_link_libraries(custom_metric_types_example
    PRIVATE monitoring_system
    Threads::Threads
)

# Alert system examples
add_executable(alert_pipeline_example
    alert_pipeline_example.cpp
)
target_link_libraries(alert_pipeline_example
    PRIVATE monitoring_system
    Threads::Threads
)

add_executable(alert_triggers_example
    alert_triggers_example.cpp
)
target_link_libraries(alert_triggers_example
    PRIVATE monitoring_system
    Threads::Threads
)

add_executable(alert_notifiers_example
    alert_notifiers_example.cpp
)
target_link_libraries(alert_notifiers_example
    PRIVATE monitoring_system
    Threads::Threads
)

# Phase 2: Unified Collector Examples
add_executable(system_collectors_example
    system_collectors_example.cpp
)
target_link_libraries(system_collectors_example
    PRIVATE monitoring_system
    Threads::Threads
)

add_executable(platform_metrics_example
    platform_metrics_example.cpp
)
target_link_libraries(platform_metrics_example
    PRIVATE monitoring_system
    Threads::Threads
)

add_executable(collector_factory_example
    collector_factory_example.cpp
)
target_link_libraries(collector_factory_example
    PRIVATE monitoring_system
    Threads::Threads
)

# Phase 3: Advanced Integration Examples
add_executable(otlp_export_example
    otlp_export_example.cpp
)
target_link_libraries(otlp_export_example
    PRIVATE monitoring_system
    Threads::Threads
)

add_executable(time_series_storage_example
    time_series_storage_example.cpp
)
target_link_libraries(time_series_storage_example
    PRIVATE monitoring_system
    Threads::Threads
)

add_executable(multi_service_tracing_example
    multi_service_tracing_example.cpp
)
target_link_libraries(multi_service_tracing_example
    PRIVATE monitoring_system
    Threads::Threads
)

# Phase 4: Production Patterns Examples
add_executable(production_monitoring_example
    production_monitoring_example.cpp
)
target_link_libraries(production_monitoring_example
    PRIVATE monitoring_system
    Threads::Threads
)

add_executable(graceful_degradation_example
    graceful_degradation_example.cpp
)
target_link_libraries(graceful_degradation_example
    PRIVATE monitoring_system
    Threads::Threads
)

# Phase 4 DI Pattern examples - available when common_system integration is enabled
if(MONITORING_WITH_COMMON_SYSTEM OR MONITORING_USING_COMMON_INTERFACES)
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