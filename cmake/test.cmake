# test.cmake
# Test wiring for monitoring_system.
#
# Handles both the unit-test subdirectory (tests/) and the
# integration-test subdirectory (integration_tests/), each guarded by its
# own option. enable_testing() is called once when either group is enabled.

if(MONITORING_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Integration Tests
if(MONITORING_BUILD_INTEGRATION_TESTS)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/integration_tests/CMakeLists.txt)
        enable_testing()
        add_subdirectory(integration_tests)
    else()
        message(STATUS "Integration tests requested, but CMakeLists.txt not found in integration_tests directory. Skipping.")
    endif()
endif()
