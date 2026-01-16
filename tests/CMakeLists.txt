# Test directory CMakeLists.txt

# Find GTest
find_package(GTest)

if(NOT GTest_FOUND)
    message(STATUS "GTest not found, using FetchContent")

    # Configure googletest options before fetching (fix Windows/MSVC pthread.h issue)
    set(gtest_force_shared_crt ON CACHE BOOL "Always use shared runtime library" FORCE)
    set(gtest_disable_pthreads ON CACHE BOOL "Disable pthreads for Windows" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "Disable GMock" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "Disable GTest installation" FORCE)

    message(STATUS "GTest not found - fetching from GitHub")
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
    message(STATUS "GTest fetched successfully")
endif()

# Create test executable
add_executable(monitoring_system_tests
    # Core working tests (verified compiling and passing)
    test_result_types.cpp
    test_di_container.cpp
    test_monitorable_interface.cpp
    test_thread_context_simple.cpp
    test_lock_free_collector.cpp
    test_distributed_tracing.cpp
    test_performance_monitoring.cpp
    test_event_bus.cpp
    test_trace_exporters.cpp
    test_adapter_functionality.cpp
    test_cross_system_integration.cpp

    test_adaptive_monitoring.cpp
    test_timer_metrics.cpp
    test_service_registration.cpp

    # Container metrics monitoring (Issue #228)
    test_container_collector.cpp

    # SMART disk health monitoring (Issue #227)
    test_smart_collector.cpp

    # Hardware temperature monitoring (Issue #215)
    test_temperature_collector.cpp

    # File descriptor usage monitoring (Issue #220)
    test_fd_collector.cpp

    # Inode usage monitoring (Issue #224)
    test_inode_collector.cpp

    # TCP connection state monitoring (Issue #225)
    test_tcp_state_collector.cpp

    # Interrupt statistics monitoring (Issue #223)
    test_interrupt_collector.cpp

    # Power consumption monitoring (Issue #216)
    test_power_collector.cpp

    # GPU metrics monitoring (Issue #221)
    test_gpu_collector.cpp

    # Socket buffer usage monitoring (Issue #226)
    test_socket_buffer_collector.cpp

    # Context switch monitoring (Issue #222)
    test_system_resource_collector.cpp
    test_context_switch_collector.cpp

    # Security event monitoring (Issue #230)
    test_security_collector.cpp

    # Virtualization metrics monitoring (Issue #229)
    test_vm_collector.cpp

    # System uptime monitoring (Issue #217)
    test_uptime_collector.cpp

    # Battery status monitoring (Issue #218)
    test_battery_collector.cpp

    # Load average history tracking (Issue #219)
    test_time_series_buffer.cpp

    # Platform metrics provider (Issue #295)
    test_metrics_provider.cpp

    # Metric exporters and OpenTelemetry adapter tests (Issue #320 - Phase 1)
    test_metric_exporters.cpp
    test_opentelemetry_adapter.cpp

    # Storage backends test (Issue #326 - Phase 2, #343)
    test_storage_backends.cpp

    # Fault tolerance tests (Issue #329 - ARC-001 Phase 1)
    test_fault_tolerance.cpp

    # Error boundaries and graceful degradation tests (Issue #338)
    test_error_boundaries.cpp

    # Metric storage tests (Issue #339 - ring_buffer.h, metric_storage.h implemented)
    test_metric_storage.cpp

    # Resource management tests (Issue #341 - resource_manager.h implemented)
    test_resource_management.cpp

    # Health monitoring tests (Issue #330 - health_check, composite_health_check,
    # health_dependency_graph, health_check_builder, global_health_monitor() implemented)
    test_health_monitoring.cpp

    # Data consistency tests (Issue #342 - data_consistency.h implemented)
    test_data_consistency.cpp

    # Stress performance tests (Issue #345 - headers fixed)
    test_stress_performance.cpp

    # Optimization tests (Issue #340 - optimization/ folder implemented)
    test_optimization.cpp

    # =========================================================================
    # Phase 3 P2: Stream aggregation
    test_stream_aggregation.cpp

    # Integration E2E tests (Issue #346)
    test_integration_e2e.cpp

    # Metric factory tests (Issue #366)
    test_metric_factory.cpp

    # Statistics utilities tests (Issue #381)
    test_statistics_utils.cpp
)

# Interface compilation test (standalone executable with main())
add_executable(monitoring_interfaces_compile_test
    test_interfaces_compile.cpp
)

target_link_libraries(monitoring_interfaces_compile_test
    PRIVATE
        monitoring_system
        Threads::Threads
)

# Add to CTest
add_test(NAME monitoring_interfaces_compile_test
         COMMAND monitoring_interfaces_compile_test)

# Thread safety tests (Phase 1 - Task 1.3)
add_executable(monitoring_thread_safety_test
    thread_safety_tests.cpp
)

target_link_libraries(monitoring_system_tests
    PRIVATE
        monitoring_system
        GTest::gtest_main
        Threads::Threads
)

# Suppress C4996 (deprecated) warning for tests using common_system's ILogger interface
# The ILogger default implementation calls deprecated legacy API internally
if(MSVC)
    target_compile_options(monitoring_system_tests PRIVATE /wd4996)
    target_compile_options(monitoring_thread_safety_test PRIVATE /wd4996)
endif()

target_link_libraries(monitoring_thread_safety_test
    PRIVATE
        monitoring_system
        GTest::gtest_main
        Threads::Threads
)

# Register tests
include(GoogleTest)
gtest_discover_tests(monitoring_system_tests)
gtest_discover_tests(monitoring_thread_safety_test)
