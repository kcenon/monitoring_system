# Examples directory CMakeLists.txt

# Basic example showing Result pattern usage
add_executable(result_pattern_example
    result_pattern_example.cpp
)

target_link_libraries(result_pattern_example
    PRIVATE monitoring_system
)