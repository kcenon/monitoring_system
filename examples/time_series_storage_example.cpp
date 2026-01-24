// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
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
 * @file time_series_storage_example.cpp
 * @brief Demonstrates time-series storage for metric history
 *
 * This example shows how to:
 * - Initialize time-series storage backend
 * - Write metrics with timestamps
 * - Perform time-range queries (last N minutes, between timestamps)
 * - Configure retention policies
 * - Implement downsampling for long-term storage
 * - Execute aggregation queries (avg, min, max, percentiles)
 */

#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <random>
#include <iomanip>

#include "kcenon/monitoring/utils/time_series.h"

using namespace kcenon::monitoring;
using namespace std::chrono_literals;

/**
 * @brief Format timestamp for display
 */
std::string format_timestamp(std::chrono::system_clock::time_point tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        tp.time_since_epoch()) % 1000;

    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time_t);
#else
    localtime_r(&time_t, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

/**
 * @brief Demonstrate basic time-series operations
 */
void demonstrate_basic_operations() {
    std::cout << "=== Basic Time-Series Operations ===" << std::endl;

    try {
        // Step 1: Create time-series with configuration
        std::cout << "\n1. Creating time-series storage..." << std::endl;

        time_series_config config;
        config.retention_period = 1h;      // Keep data for 1 hour
        config.resolution = 1s;            // 1-second resolution
        config.max_points = 3600;          // Max 3600 points (1 hour of data)
        config.enable_compression = true;
        config.compression_threshold = 0.1;

        auto ts_result = time_series::create("cpu_usage", config);
        if (ts_result.is_err()) {
            std::cerr << "   Failed to create time-series: "
                      << ts_result.error().message << std::endl;
            return;
        }

        auto ts = std::move(ts_result.value());
        std::cout << "   ✓ Created time-series: " << ts->name() << std::endl;
        std::cout << "     Retention: " << config.retention_period.count() << "s" << std::endl;
        std::cout << "     Max points: " << config.max_points << std::endl;

        // Step 2: Write metrics with timestamps
        std::cout << "\n2. Writing metric data points..." << std::endl;

        auto now = std::chrono::system_clock::now();
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(30.0, 90.0);

        // Add 100 data points over the last 100 seconds
        for (int i = 0; i < 100; ++i) {
            auto timestamp = now - std::chrono::seconds(100 - i);
            double cpu_value = dis(gen);  // Random CPU usage 30-90%

            auto add_result = ts->add_point(cpu_value, timestamp);
            if (add_result.is_err()) {
                std::cerr << "   Failed to add point: "
                          << add_result.error().message << std::endl;
            }
        }

        std::cout << "   ✓ Added 100 data points" << std::endl;
        std::cout << "     Current size: " << ts->size() << " points" << std::endl;
        std::cout << "     Memory footprint: " << ts->memory_footprint() << " bytes" << std::endl;

        // Step 3: Get latest value
        std::cout << "\n3. Retrieving latest value..." << std::endl;

        auto latest_result = ts->get_latest_value();
        if (latest_result.is_ok()) {
            std::cout << "   Latest CPU usage: "
                      << std::fixed << std::setprecision(2)
                      << latest_result.value() << "%" << std::endl;
        }

        // Step 4: Time-range query (last 30 seconds)
        std::cout << "\n4. Querying last 30 seconds..." << std::endl;

        time_series_query query;
        query.start_time = now - 30s;
        query.end_time = now;
        query.step = 5s;  // 5-second aggregation steps

        auto query_result = ts->query(query);
        if (query_result.is_ok()) {
            const auto& result = query_result.value();
            std::cout << "   ✓ Query returned " << result.points.size()
                      << " aggregated points" << std::endl;
            std::cout << "     Total samples: " << result.total_samples << std::endl;
            std::cout << "     Average value: "
                      << std::fixed << std::setprecision(2)
                      << result.get_average() << "%" << std::endl;

            // Show first 3 points
            std::cout << "\n   Sample points:" << std::endl;
            for (size_t i = 0; i < std::min(size_t(3), result.points.size()); ++i) {
                const auto& point = result.points[i];
                std::cout << "     [" << format_timestamp(point.timestamp) << "] "
                          << std::fixed << std::setprecision(2) << point.value << "%"
                          << " (samples: " << point.sample_count << ")" << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

/**
 * @brief Demonstrate aggregation queries
 */
void demonstrate_aggregations() {
    std::cout << "\n=== Aggregation Queries ===" << std::endl;

    try {
        // Create time-series
        time_series_config config;
        config.retention_period = 1h;
        config.max_points = 3600;

        auto ts_result = time_series::create("response_time_ms", config);
        if (ts_result.is_err()) {
            return;
        }

        auto ts = std::move(ts_result.value());

        // Add sample data: simulated response times
        std::cout << "\n1. Populating with response time data..." << std::endl;

        auto now = std::chrono::system_clock::now();
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<> dis(100.0, 20.0);  // Mean 100ms, stddev 20ms

        for (int i = 0; i < 500; ++i) {
            auto timestamp = now - std::chrono::seconds(500 - i);
            double response_time = std::max(10.0, dis(gen));  // Min 10ms

            ts->add_point(response_time, timestamp);
        }

        std::cout << "   ✓ Added 500 response time measurements" << std::endl;

        // Query last 5 minutes with 1-minute aggregation
        std::cout << "\n2. Aggregation query (last 5 minutes)..." << std::endl;

        time_series_query query;
        query.start_time = now - 5min;
        query.end_time = now;
        query.step = 1min;  // 1-minute buckets

        auto query_result = ts->query(query);
        if (query_result.is_ok()) {
            const auto& result = query_result.value();
            auto summary = result.get_summary();

            std::cout << "\n   Aggregation results:" << std::endl;
            std::cout << "     Count: " << summary.count << std::endl;
            std::cout << "     Average: "
                      << std::fixed << std::setprecision(2)
                      << summary.mean() << " ms" << std::endl;
            std::cout << "     Min: "
                      << std::fixed << std::setprecision(2)
                      << summary.min_value << " ms" << std::endl;
            std::cout << "     Max: "
                      << std::fixed << std::setprecision(2)
                      << summary.max_value << " ms" << std::endl;
            std::cout << "     Sum: "
                      << std::fixed << std::setprecision(2)
                      << summary.sum << " ms" << std::endl;
            std::cout << "     Rate of change: "
                      << std::fixed << std::setprecision(2)
                      << result.get_rate() << " ms/s" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

/**
 * @brief Demonstrate retention policy and downsampling
 */
void demonstrate_retention_and_downsampling() {
    std::cout << "\n=== Retention Policy & Downsampling ===" << std::endl;

    try {
        std::cout << "\n1. Creating time-series with short retention..." << std::endl;

        time_series_config config;
        config.retention_period = 60s;     // Only keep last 60 seconds
        config.max_points = 120;           // Limit to 120 points
        config.enable_compression = true;  // Enable downsampling

        auto ts_result = time_series::create("memory_usage_mb", config);
        if (ts_result.is_err()) {
            return;
        }

        auto ts = std::move(ts_result.value());
        std::cout << "   ✓ Created time-series with 60-second retention" << std::endl;

        // Add old data (beyond retention)
        std::cout << "\n2. Adding old data (beyond retention period)..." << std::endl;

        auto now = std::chrono::system_clock::now();

        // Add data from 2 minutes ago (should be cleaned up)
        for (int i = 0; i < 60; ++i) {
            auto old_timestamp = now - 120s + std::chrono::seconds(i);
            ts->add_point(500.0 + i, old_timestamp);
        }

        // Add recent data (within retention)
        for (int i = 0; i < 60; ++i) {
            auto recent_timestamp = now - 60s + std::chrono::seconds(i);
            ts->add_point(800.0 + i, recent_timestamp);
        }

        std::cout << "   Added 120 points total" << std::endl;
        std::cout << "   Current size after cleanup: " << ts->size() << " points" << std::endl;
        std::cout << "   (Old data beyond retention was automatically removed)" << std::endl;

        // Query all data
        std::cout << "\n3. Querying all retained data..." << std::endl;

        time_series_query query;
        query.start_time = now - 60s;
        query.end_time = now;
        query.step = 10s;

        auto query_result = ts->query(query);
        if (query_result.is_ok()) {
            const auto& result = query_result.value();
            std::cout << "   ✓ Retrieved " << result.points.size()
                      << " downsampled points (from " << result.total_samples
                      << " original samples)" << std::endl;
            std::cout << "     Downsampling ratio: "
                      << std::fixed << std::setprecision(1)
                      << (double)result.total_samples / result.points.size()
                      << "x compression" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

/**
 * @brief Demonstrate batch point insertion
 */
void demonstrate_batch_operations() {
    std::cout << "\n=== Batch Point Operations ===" << std::endl;

    try {
        std::cout << "\n1. Creating time-series..." << std::endl;

        auto ts_result = time_series::create("network_throughput_mbps");
        if (ts_result.is_err()) {
            return;
        }

        auto ts = std::move(ts_result.value());

        // Create batch of points
        std::cout << "\n2. Preparing batch of data points..." << std::endl;

        std::vector<time_point_data> batch;
        auto now = std::chrono::system_clock::now();

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(50.0, 200.0);

        for (int i = 0; i < 200; ++i) {
            auto timestamp = now - std::chrono::seconds(200 - i);
            double throughput = dis(gen);
            batch.emplace_back(timestamp, throughput);
        }

        std::cout << "   ✓ Prepared " << batch.size() << " points" << std::endl;

        // Add batch
        std::cout << "\n3. Adding batch..." << std::endl;

        auto add_result = ts->add_points(batch);
        if (add_result.is_ok()) {
            std::cout << "   ✓ Batch insert successful" << std::endl;
            std::cout << "     Total points in series: " << ts->size() << std::endl;
            std::cout << "     Memory footprint: " << ts->memory_footprint() << " bytes" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "Time-Series Storage Example\n" << std::endl;

    // Demonstrate basic operations
    demonstrate_basic_operations();

    std::cout << "\n" << std::string(60, '=') << "\n" << std::endl;

    // Demonstrate aggregation queries
    demonstrate_aggregations();

    std::cout << "\n" << std::string(60, '=') << "\n" << std::endl;

    // Demonstrate retention and downsampling
    demonstrate_retention_and_downsampling();

    std::cout << "\n" << std::string(60, '=') << "\n" << std::endl;

    // Demonstrate batch operations
    demonstrate_batch_operations();

    std::cout << "\n=== Example completed successfully ===" << std::endl;

    return 0;
}
