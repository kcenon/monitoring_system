/**
 * @file main_bench.cpp
 * @brief Main entry point for monitoring_system benchmarks
 * @details Initializes Google Benchmark and runs all registered benchmarks
 *
 * Usage:
 *   ./monitoring_benchmarks                                 # Run all
 *   ./monitoring_benchmarks --benchmark_filter=Metric       # Specific category
 *   ./monitoring_benchmarks --benchmark_format=json         # JSON output
 *   ./monitoring_benchmarks --benchmark_out=results.json    # Save results
 *
 * Phase 0, Task 0.2: Baseline Performance Benchmarking
 */

#include <benchmark/benchmark.h>
#include <iostream>

int main(int argc, char** argv) {
    std::cout << "========================================\n";
    std::cout << "monitoring_system Performance Benchmarks\n";
    std::cout << "Phase 0: Baseline Measurement\n";
    std::cout << "========================================\n\n";

    benchmark::Initialize(&argc, argv);

    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }

    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();

    std::cout << "\n========================================\n";
    std::cout << "Benchmarks Complete\n";
    std::cout << "========================================\n";

    return 0;
}
