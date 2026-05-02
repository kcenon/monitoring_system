# Test directory CMakeLists.txt
#
# Tests mirror src/ feature directories:
#   tests/alert/        - alert manager, triggers, types
#   tests/collectors/   - per-collector unit tests
#   tests/context/      - thread context propagation
#   tests/core/         - core monitor, tracer, adapters, optimization,
#                         interfaces, event bus, performance, integration e2e
#   tests/platform/     - metrics provider strategy
#   tests/plugins/      - collector registry, factory, container plugin
#   tests/utils/        - utils (storage, time series, statistics, stream agg, etc.)
#   tests/integration/  - cross-feature scenarios for src/-less feature areas
#                         (reliability, exporters, storage, health, di, config)
#
# Recursive globbing keeps the file list maintenance-free as tests are added.

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

# Collect test sources from every feature subdirectory.
# CONFIGURE_DEPENDS makes CMake re-run the glob when files are added/removed.
file(GLOB_RECURSE MONITORING_TEST_SOURCES_ALL CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp
)

# Tests with their own add_executable() targets defined later in this file.
list(REMOVE_ITEM MONITORING_TEST_SOURCES_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/core/test_interfaces_compile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/test_container_plugin.cpp
)

# Tests intentionally excluded from monitoring_system_tests:
#   - utils/test_buffering_strategies.cpp: depends on internal headers under src/utils/
#     that are not part of the public include path of the test target.
#   - plugins/test_collector_registry.cpp: superseded by
#     plugins/test_collector_registry_integration.cpp.
#   - context/test_thread_context.cpp: superseded by
#     context/test_thread_context_simple.cpp.
list(REMOVE_ITEM MONITORING_TEST_SOURCES_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/test_buffering_strategies.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/test_collector_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/context/test_thread_context.cpp
)

# Create test executable
add_executable(monitoring_system_tests
    ${MONITORING_TEST_SOURCES_ALL}
)

# Interface compilation test (standalone executable with main())
add_executable(monitoring_interfaces_compile_test
    core/test_interfaces_compile.cpp
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
    core/thread_safety_tests.cpp
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

# Container plugin tests (only when plugin is built)
if(MONITORING_BUILD_CONTAINER_PLUGIN)
    add_executable(monitoring_container_plugin_test
        plugins/test_container_plugin.cpp
    )

    target_link_libraries(monitoring_container_plugin_test
        PRIVATE
            monitoring_container_plugin
            monitoring_system
            GTest::gtest_main
            Threads::Threads
    )

    if(MSVC)
        target_compile_options(monitoring_container_plugin_test PRIVATE /wd4996)
    endif()

    gtest_discover_tests(monitoring_container_plugin_test)
endif()
