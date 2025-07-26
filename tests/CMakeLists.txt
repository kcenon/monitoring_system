# Enable testing
enable_testing()

# Find Google Test
find_package(GTest CONFIG REQUIRED)

# Placeholder for future test sources
# set(TEST_SOURCES
#     test_monitoring.cpp
#     test_system_metrics.cpp
# )

# Only create test executable if we have test sources
if(TEST_SOURCES)
    add_executable(monitoring_tests ${TEST_SOURCES})
    
    target_link_libraries(monitoring_tests
        PRIVATE
            monitoring
            GTest::gtest
            GTest::gtest_main
    )
    
    # Add test to CTest
    add_test(NAME MonitoringTests COMMAND monitoring_tests)
endif()