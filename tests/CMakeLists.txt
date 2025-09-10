# Test directory CMakeLists.txt

# Find GTest
find_package(GTest)

if(NOT GTest_FOUND)
    message(STATUS "GTest not found, using FetchContent")
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
    test_result_types.cpp
    test_di_container.cpp
)

target_link_libraries(monitoring_system_tests
    PRIVATE
        monitoring_system
        GTest::gtest_main
        Threads::Threads
)

# Register tests
include(GoogleTest)
gtest_discover_tests(monitoring_system_tests)