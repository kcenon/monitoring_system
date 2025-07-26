# Basic monitoring sample
add_executable(basic_monitoring basic_monitoring.cpp)
target_link_libraries(basic_monitoring PRIVATE monitoring)
set_target_properties(basic_monitoring PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)