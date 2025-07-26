# Basic monitoring sample
add_executable(basic_monitoring basic_monitoring.cpp)
target_link_libraries(basic_monitoring PRIVATE monitoring)
set_target_properties(basic_monitoring PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Multi-process monitoring sample
add_executable(multi_process_example multi_process_example.cpp)
target_link_libraries(multi_process_example PRIVATE monitoring)
set_target_properties(multi_process_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Thread pool analysis sample
add_executable(thread_pool_analysis_example thread_pool_analysis_example.cpp)
target_link_libraries(thread_pool_analysis_example PRIVATE monitoring)
set_target_properties(thread_pool_analysis_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)