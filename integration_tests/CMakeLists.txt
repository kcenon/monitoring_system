cmake_minimum_required(VERSION 3.20)

# Integration Tests for monitoring_system
project(monitoring_integration_tests LANGUAGES CXX)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find or fetch Google Test
find_package(GTest QUIET)

if(NOT GTest_FOUND)
    message(STATUS "GTest not found - fetching from GitHub")

    # Configure googletest options before fetching (fix Windows/MSVC pthread.h issue)
    set(gtest_force_shared_crt ON CACHE BOOL "Always use shared runtime library" FORCE)
    set(gtest_disable_pthreads ON CACHE BOOL "Disable pthreads for Windows" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "Disable GMock" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "Disable GTest installation" FORCE)

    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googletest)
    message(STATUS "GTest fetched successfully")
endif()

include(GoogleTest)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/framework
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/sources
)

# Collect all test source files
file(GLOB SCENARIO_TESTS scenarios/*.cpp)
file(GLOB PERFORMANCE_TESTS performance/*.cpp)
file(GLOB FAILURE_TESTS failures/*.cpp)

# Create integration test executable
add_executable(monitoring_integration_tests
    ${SCENARIO_TESTS}
    ${PERFORMANCE_TESTS}
    ${FAILURE_TESTS}
)

# Link against monitoring_system and Google Test
if(TARGET GTest::GTest)
    # Using system-installed GTest
    target_link_libraries(monitoring_integration_tests
        PRIVATE
            monitoring_system
            GTest::GTest
            GTest::Main
            Threads::Threads
    )
else()
    # Using FetchContent GTest
    target_link_libraries(monitoring_integration_tests
        PRIVATE
            monitoring_system
            gtest
            gtest_main
            Threads::Threads
    )
endif()

# Add compile options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(monitoring_integration_tests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror=return-type
    )
elseif(MSVC)
    target_compile_options(monitoring_integration_tests PRIVATE
        /W4
        /WX
    )
endif()

# Enable testing
enable_testing()

# Discover tests
gtest_discover_tests(monitoring_integration_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    PROPERTIES
        TIMEOUT 300
)

# Add custom target to run integration tests
add_custom_target(monitoring_run_integration_tests
    COMMAND monitoring_integration_tests
    DEPENDS monitoring_integration_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running monitoring system integration tests"
)

# Coverage target (if coverage is enabled)
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(monitoring_integration_tests PRIVATE
            --coverage
            -fprofile-arcs
            -ftest-coverage
        )
        target_link_options(monitoring_integration_tests PRIVATE
            --coverage
        )

        # Add coverage target
        add_custom_target(integration_coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
            COMMAND lcov --capture --directory . --output-file coverage/integration.info --ignore-errors mismatch --ignore-errors inconsistent
            COMMAND lcov --remove coverage/integration.info '/usr/*' '*/tests/*' '*/examples/*' --output-file coverage/integration_filtered.info --ignore-errors unused
            COMMAND genhtml coverage/integration_filtered.info --output-directory coverage/integration_html --ignore-errors source
            DEPENDS monitoring_run_integration_tests
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating integration test coverage report"
        )
    endif()
endif()

# Summary
message(STATUS "Integration Tests Configuration:")
message(STATUS "  Scenario tests: ${SCENARIO_TESTS}")
message(STATUS "  Performance tests: ${PERFORMANCE_TESTS}")
message(STATUS "  Failure tests: ${FAILURE_TESTS}")
