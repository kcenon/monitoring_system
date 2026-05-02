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

# Collect test sources via glob, then exclude tests that have their own
# executable target or are intentionally not part of the main test binary.
# CONFIGURE_DEPENDS makes CMake re-run the glob when files are added/removed.
file(GLOB MONITORING_TEST_SOURCES_ALL CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp
)

# Tests with their own add_executable() targets defined later in this file.
list(REMOVE_ITEM MONITORING_TEST_SOURCES_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/test_interfaces_compile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_container_plugin.cpp
)

# Tests intentionally excluded from monitoring_system_tests:
#   - test_buffering_strategies.cpp: depends on internal headers under src/utils/
#     that are not part of the public include path of the test target.
#   - test_collector_registry.cpp: superseded by test_collector_registry_integration.cpp.
#   - test_thread_context.cpp: superseded by test_thread_context_simple.cpp.
list(REMOVE_ITEM MONITORING_TEST_SOURCES_ALL
    ${CMAKE_CURRENT_SOURCE_DIR}/test_buffering_strategies.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_collector_registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_thread_context.cpp
)

# Create test executable
add_executable(monitoring_system_tests
    ${MONITORING_TEST_SOURCES_ALL}
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

# Container plugin tests (only when plugin is built)
if(MONITORING_BUILD_CONTAINER_PLUGIN)
    add_executable(monitoring_container_plugin_test
        test_container_plugin.cpp
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
