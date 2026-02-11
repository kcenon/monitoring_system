# Monitoring System Benchmarks
# Phase 2, ARC-002: Complete Performance Benchmark Suite

cmake_minimum_required(VERSION 3.20)

# Find Google Benchmark
find_package(benchmark QUIET)

if(NOT benchmark_FOUND)
    message(STATUS "Google Benchmark not found. Trying to find via pkg-config...")
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(BENCHMARK benchmark)
    endif()
endif()

if(benchmark_FOUND OR BENCHMARK_FOUND)
    message(STATUS "Google Benchmark found. Building monitoring_system benchmarks...")

    # Benchmark executable
    add_executable(monitoring_benchmarks
        core_metrics_bench.cpp
        metric_collection_bench.cpp
        event_bus_bench.cpp
        collector_overhead_bench.cpp
        adaptive_monitor_bench.cpp
        main_bench.cpp
    )

    target_link_libraries(monitoring_benchmarks
        PRIVATE
            monitoring_system
            benchmark::benchmark
            benchmark::benchmark_main
    )

    target_include_directories(monitoring_benchmarks
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../sources
    )

    # Add common_system include path if available
    if(TARGET monitoring_system_interface)
        target_link_libraries(monitoring_benchmarks PRIVATE monitoring_system_interface)
    endif()

    if(DEFINED COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(monitoring_benchmarks
            PRIVATE
                ${COMMON_SYSTEM_INCLUDE_DIR}
        )
    elseif(TARGET kcenon::common_system)
        target_link_libraries(monitoring_benchmarks PRIVATE kcenon::common_system)
    endif()

    target_compile_features(monitoring_benchmarks PRIVATE cxx_std_20)

    # Set compiler warnings and optimization
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(monitoring_benchmarks PRIVATE
            -Wall -Wextra -Wpedantic
            -O3  # Maximum optimization
            -DNDEBUG  # Disable asserts
        )
    elseif(MSVC)
        target_compile_options(monitoring_benchmarks PRIVATE
            /W4 /O2 /DNDEBUG
        )
    endif()

    # Add custom target to run benchmarks
    add_custom_target(run_monitoring_benchmarks
        COMMAND monitoring_benchmarks --benchmark_format=json --benchmark_out=benchmark_results.json
        COMMAND monitoring_benchmarks --benchmark_format=console
        DEPENDS monitoring_benchmarks
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running monitoring_system performance benchmarks"
    )

    # Installation
    install(TARGETS monitoring_benchmarks
        RUNTIME DESTINATION bin/benchmarks
    )

else()
    message(WARNING "Google Benchmark not found. Skipping monitoring_system benchmarks.")
    message(STATUS "Install with: brew install google-benchmark (macOS) or apt-get install libbenchmark-dev (Ubuntu)")
endif()
