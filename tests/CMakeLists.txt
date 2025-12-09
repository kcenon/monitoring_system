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

    # =========================================================================
    # MON-002: Deferred tests requiring API updates or missing headers (13 remaining)
    # =========================================================================
    # The following tests require significant updates due to:
    # - Result<T> API changes (need .is_ok() instead of bool conversion)
    # - Missing header files (graceful_degradation.h, resource_manager.h, etc.)
    # - Type mismatches (monitoring_data not in current API)
    #
    # test_metric_exporters.cpp       # monitoring_data type missing
    # test_opentelemetry_adapter.cpp  # monitoring_data type missing, Result API
    # test_health_monitoring.cpp      # Result<T> API updates needed
    # test_fault_tolerance.cpp        # Result<T> API updates needed
    # test_error_boundaries.cpp       # Missing: graceful_degradation.h
    # test_optimization.cpp           # Missing: optimization/ folder
    # test_resource_management.cpp    # Missing: resource_manager.h
    # test_data_consistency.cpp       # Missing: data_consistency.h
    # test_metric_storage.cpp         # Missing: ring_buffer.h, metric_storage.h
    # test_stream_aggregation.cpp     # Missing: stream_aggregator.h, aggregation_processor.h
    # test_buffering_strategies.cpp   # Missing: buffering_strategy.h, buffer_manager.h
    # test_stress_performance.cpp     # Missing: multiple headers
    # test_storage_backends.cpp       # API mismatch with storage_backends.h
    # test_integration_e2e.cpp        # Multiple missing headers
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