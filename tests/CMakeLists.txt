# Test directory CMakeLists.txt

# Find GTest
find_package(GTest)

if(NOT GTest_FOUND)
    message(STATUS "GTest not found, using FetchContent")

    # Configure googletest options before fetching (fix Windows/MSVC pthread.h issue)
    set(gtest_force_shared_crt ON CACHE BOOL "Always use shared runtime library" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "Disable GMock" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "Disable GTest installation" FORCE)

    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    FetchContent_MakeAvailable(googletest)
endif()

# Create test executable
add_executable(monitoring_system_tests
    # Core working tests (verified compiling and passing)
    test_result_types.cpp
    test_di_container.cpp
    test_monitorable_interface.cpp
    test_thread_context_simple.cpp
    test_lock_free_collector.cpp

    # =========================================================================
    # Tests requiring API alignment - MON-002 (deferred)
    # =========================================================================
    # These tests were disabled because they reference old API patterns.
    # Re-enabling requires updating each test file to use current APIs.
    # See docs/kanban/MON-002-test-activation.md for details.
    #
    # Tracing tests:
    #   test_distributed_tracing.cpp - needs trace_span, trace_context API updates
    #
    # Monitoring tests:
    #   test_performance_monitoring.cpp - needs adaptive_level API
    #   test_adaptive_monitoring.cpp - needs adaptive_level API
    #   test_health_monitoring.cpp - needs health_check types
    #
    # Storage/Metrics tests:
    #   test_metric_storage.cpp
    #   test_stream_aggregation.cpp
    #   test_buffering_strategies.cpp
    #   test_metric_exporters.cpp
    #   test_storage_backends.cpp
    #
    # Reliability tests:
    #   test_fault_tolerance.cpp
    #   test_error_boundaries.cpp
    #   test_optimization.cpp
    #
    # Other tests:
    #   test_resource_management.cpp
    #   test_data_consistency.cpp
    #   test_opentelemetry_adapter.cpp
    #   test_trace_exporters.cpp
    #   test_integration_e2e.cpp
    #   test_stress_performance.cpp
    #   test_event_bus.cpp - Result<T> bool conversion issue
    #   test_interfaces_compile.cpp - metric_value API change
)

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