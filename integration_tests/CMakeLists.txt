cmake_minimum_required(VERSION 3.20)

# Integration Tests for monitoring_system
project(monitoring_integration_tests LANGUAGES CXX)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Google Test
# Note: If not found, parent CMakeLists.txt may provide it via FetchContent
find_package(GTest QUIET)
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
target_link_libraries(monitoring_integration_tests
    PRIVATE
        monitoring_system
        GTest::GTest
        GTest::Main
        Threads::Threads
)

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
add_custom_target(run_integration_tests
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
            DEPENDS run_integration_tests
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
