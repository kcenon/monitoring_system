// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file custom_metric_types_example.cpp
 * @brief Example demonstrating custom metric types: histogram, summary, and timer
 *
 * This example shows how to use the advanced metric types added in ARC-007:
 * - histogram_data: Distribution of values with configurable buckets
 * - summary_data: Min/max/mean statistics
 * - timer_data: Duration measurements with percentile calculations (p50, p90, p95, p99)
 * - timer_scope: RAII-style automatic duration recording
 */

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <random>
#include <vector>

#include "kcenon/monitoring/utils/metric_types.h"

using namespace kcenon::monitoring;
using namespace std::chrono_literals;

// Simulate an API endpoint with variable latency
double simulate_api_call(std::mt19937& rng) {
    // Most calls are fast (10-50ms), some are slow (100-500ms)
    std::uniform_real_distribution<> fast_dist(10.0, 50.0);
    std::uniform_real_distribution<> slow_dist(100.0, 500.0);
    std::uniform_int_distribution<> is_slow(1, 10);

    double latency;
    if (is_slow(rng) == 1) {
        // 10% of calls are slow
        latency = slow_dist(rng);
    } else {
        latency = fast_dist(rng);
    }

    // Simulate the actual delay
    std::this_thread::sleep_for(std::chrono::microseconds(static_cast<int>(latency * 100)));
    return latency;
}

void demonstrate_histogram() {
    std::cout << "\n=== Histogram Metric Example ===" << std::endl;
    std::cout << "Tracking request size distribution\n" << std::endl;

    histogram_data request_sizes;

    // Initialize with custom buckets for request sizes (in KB)
    request_sizes.buckets = {
        {1.0, 0},    // <= 1KB
        {10.0, 0},   // <= 10KB
        {100.0, 0},  // <= 100KB
        {1000.0, 0}, // <= 1MB
        {10000.0, 0} // <= 10MB
    };

    // Simulate various request sizes
    std::mt19937 rng(42);
    std::exponential_distribution<> size_dist(0.1); // Most requests are small

    std::cout << "Recording 1000 request sizes..." << std::endl;
    for (int i = 0; i < 1000; ++i) {
        double size_kb = size_dist(rng) * 10; // Scale to reasonable KB range
        request_sizes.add_sample(size_kb);
    }

    // Display results
    std::cout << "\nHistogram Results:" << std::endl;
    std::cout << "  Total requests: " << request_sizes.total_count << std::endl;
    std::cout << "  Total size: " << std::fixed << std::setprecision(2)
              << request_sizes.sum << " KB" << std::endl;
    std::cout << "  Mean size: " << request_sizes.mean() << " KB" << std::endl;
    std::cout << "\nBucket Distribution:" << std::endl;

    uint64_t prev_count = 0;
    for (const auto& bucket : request_sizes.buckets) {
        uint64_t bucket_count = bucket.count - prev_count;
        double percentage = (bucket_count * 100.0) / request_sizes.total_count;
        std::cout << "  <= " << std::setw(7) << bucket.upper_bound << " KB: "
                  << std::setw(5) << bucket_count << " requests ("
                  << std::setw(5) << std::fixed << std::setprecision(1)
                  << percentage << "%)" << std::endl;
        prev_count = bucket.count;
    }
}

void demonstrate_summary() {
    std::cout << "\n=== Summary Metric Example ===" << std::endl;
    std::cout << "Tracking CPU usage over time\n" << std::endl;

    summary_data cpu_usage;

    // Simulate CPU usage readings
    std::mt19937 rng(123);
    std::normal_distribution<> usage_dist(45.0, 15.0); // Mean 45%, stddev 15%

    std::cout << "Recording 100 CPU usage samples..." << std::endl;
    for (int i = 0; i < 100; ++i) {
        double usage = std::clamp(usage_dist(rng), 0.0, 100.0);
        cpu_usage.add_sample(usage);
    }

    // Display results
    std::cout << "\nSummary Results:" << std::endl;
    std::cout << "  Sample count: " << cpu_usage.count << std::endl;
    std::cout << "  Min CPU: " << std::fixed << std::setprecision(2)
              << cpu_usage.min_value << "%" << std::endl;
    std::cout << "  Max CPU: " << cpu_usage.max_value << "%" << std::endl;
    std::cout << "  Mean CPU: " << cpu_usage.mean() << "%" << std::endl;
    std::cout << "  Total sum: " << cpu_usage.sum << std::endl;

    // Demonstrate reset functionality
    std::cout << "\nResetting summary..." << std::endl;
    cpu_usage.reset();
    std::cout << "  After reset - count: " << cpu_usage.count << std::endl;
}

void demonstrate_timer() {
    std::cout << "\n=== Timer Metric Example ===" << std::endl;
    std::cout << "Measuring API response times with percentiles\n" << std::endl;

    // Create timer with custom reservoir size
    timer_data api_latency(512);

    std::mt19937 rng(456);

    std::cout << "Simulating 500 API calls..." << std::endl;
    for (int i = 0; i < 500; ++i) {
        double latency = simulate_api_call(rng);
        api_latency.record(latency);
    }

    // Get and display snapshot
    auto snapshot = api_latency.get_snapshot();

    std::cout << "\nTimer Results:" << std::endl;
    std::cout << "  Total calls: " << snapshot.count << std::endl;
    std::cout << "  Min latency: " << std::fixed << std::setprecision(2)
              << snapshot.min << " ms" << std::endl;
    std::cout << "  Max latency: " << snapshot.max << " ms" << std::endl;
    std::cout << "  Mean latency: " << snapshot.mean << " ms" << std::endl;
    std::cout << "  Std deviation: " << snapshot.stddev << " ms" << std::endl;

    std::cout << "\nPercentiles:" << std::endl;
    std::cout << "  p50 (median): " << snapshot.p50 << " ms" << std::endl;
    std::cout << "  p90: " << snapshot.p90 << " ms" << std::endl;
    std::cout << "  p95: " << snapshot.p95 << " ms" << std::endl;
    std::cout << "  p99: " << snapshot.p99 << " ms" << std::endl;
    std::cout << "  p99.9: " << snapshot.p999 << " ms" << std::endl;
}

void demonstrate_timer_scope() {
    std::cout << "\n=== Timer Scope (RAII) Example ===" << std::endl;
    std::cout << "Automatic duration recording with RAII pattern\n" << std::endl;

    timer_data operation_timer;

    // Simulate different operations with automatic timing
    std::cout << "Running 10 operations with automatic timing..." << std::endl;

    for (int i = 0; i < 10; ++i) {
        // timer_scope automatically records duration when it goes out of scope
        timer_scope scope(operation_timer);

        // Simulate varying work
        std::this_thread::sleep_for(std::chrono::milliseconds(20 + (i * 5)));
    }

    // Display results
    std::cout << "\nTimer Scope Results:" << std::endl;
    std::cout << "  Operations recorded: " << operation_timer.count() << std::endl;
    std::cout << "  Mean duration: " << std::fixed << std::setprecision(2)
              << operation_timer.mean() << " ms" << std::endl;
    std::cout << "  Min duration: " << operation_timer.min() << " ms" << std::endl;
    std::cout << "  Max duration: " << operation_timer.max() << " ms" << std::endl;
    std::cout << "  p95 duration: " << operation_timer.p95() << " ms" << std::endl;
}

void demonstrate_metric_metadata() {
    std::cout << "\n=== Metric Metadata Example ===" << std::endl;
    std::cout << "Creating and using metric metadata\n" << std::endl;

    // Create metadata for different metric types
    auto counter_meta = create_metric_metadata("http_requests_total", metric_type::counter, 2);
    auto gauge_meta = create_metric_metadata("memory_usage_bytes", metric_type::gauge, 1);
    auto histogram_meta = create_metric_metadata("request_duration_seconds", metric_type::histogram, 3);

    std::cout << "Metric Metadata Examples:" << std::endl;
    std::cout << "  Counter: http_requests_total" << std::endl;
    std::cout << "    - Hash: " << counter_meta.name_hash << std::endl;
    std::cout << "    - Type: " << metric_type_to_string(counter_meta.type) << std::endl;
    std::cout << "    - Tags: " << static_cast<int>(counter_meta.tag_count) << std::endl;

    std::cout << "\n  Gauge: memory_usage_bytes" << std::endl;
    std::cout << "    - Hash: " << gauge_meta.name_hash << std::endl;
    std::cout << "    - Type: " << metric_type_to_string(gauge_meta.type) << std::endl;
    std::cout << "    - Tags: " << static_cast<int>(gauge_meta.tag_count) << std::endl;

    std::cout << "\n  Histogram: request_duration_seconds" << std::endl;
    std::cout << "    - Hash: " << histogram_meta.name_hash << std::endl;
    std::cout << "    - Type: " << metric_type_to_string(histogram_meta.type) << std::endl;
    std::cout << "    - Tags: " << static_cast<int>(histogram_meta.tag_count) << std::endl;

    // Create compact metric values
    compact_metric_value counter_value(counter_meta, int64_t(12345));
    compact_metric_value gauge_value(gauge_meta, 1024.5);

    std::cout << "\nCompact Metric Values:" << std::endl;
    std::cout << "  Counter value: " << counter_value.as_int64() << std::endl;
    std::cout << "  Gauge value: " << gauge_value.as_double() << std::endl;
    std::cout << "  Memory footprint (counter): " << counter_value.memory_footprint() << " bytes" << std::endl;
    std::cout << "  Memory footprint (gauge): " << gauge_value.memory_footprint() << " bytes" << std::endl;
}

void demonstrate_metric_batch() {
    std::cout << "\n=== Metric Batch Example ===" << std::endl;
    std::cout << "Batching metrics for efficient processing\n" << std::endl;

    metric_batch batch(1);
    batch.reserve(100);

    // Add various metrics to the batch
    auto counter_meta = create_metric_metadata("requests", metric_type::counter);
    auto gauge_meta = create_metric_metadata("connections", metric_type::gauge);

    std::cout << "Adding 100 metrics to batch..." << std::endl;
    for (int i = 0; i < 50; ++i) {
        batch.add_metric(compact_metric_value(counter_meta, int64_t(i * 10)));
        batch.add_metric(compact_metric_value(gauge_meta, double(100 + i)));
    }

    std::cout << "\nBatch Statistics:" << std::endl;
    std::cout << "  Batch ID: " << batch.batch_id << std::endl;
    std::cout << "  Metrics count: " << batch.size() << std::endl;
    std::cout << "  Memory footprint: " << batch.memory_footprint() << " bytes" << std::endl;
    std::cout << "  Is empty: " << (batch.empty() ? "yes" : "no") << std::endl;

    // Clear and verify
    batch.clear();
    std::cout << "\nAfter clear:" << std::endl;
    std::cout << "  Metrics count: " << batch.size() << std::endl;
    std::cout << "  Is empty: " << (batch.empty() ? "yes" : "no") << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Custom Metric Types Example" << std::endl;
    std::cout << "  monitoring_system v2.0" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        // Demonstrate each metric type
        demonstrate_histogram();
        demonstrate_summary();
        demonstrate_timer();
        demonstrate_timer_scope();
        demonstrate_metric_metadata();
        demonstrate_metric_batch();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  Example completed successfully!" << std::endl;
        std::cout << "========================================" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
