/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#pragma once

#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <functional>
#include <random>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <memory>
#include <unordered_map>

#include <kcenon/monitoring/utils/metric_types.h>
#include <kcenon/monitoring/core/result_types.h>

namespace integration_tests {

namespace fs = std::filesystem;

/**
 * @class ScopedTimer
 * @brief RAII timer for measuring execution time
 */
class ScopedTimer {
public:
    using clock_type = std::chrono::high_resolution_clock;
    using duration_type = std::chrono::nanoseconds;

    explicit ScopedTimer(std::function<void(duration_type)> callback = nullptr)
        : start_(clock_type::now()), callback_(std::move(callback)) {}

    ~ScopedTimer() {
        auto duration = std::chrono::duration_cast<duration_type>(
            clock_type::now() - start_);
        if (callback_) {
            callback_(duration);
        }
    }

    duration_type elapsed() const {
        return std::chrono::duration_cast<duration_type>(
            clock_type::now() - start_);
    }

private:
    clock_type::time_point start_;
    std::function<void(duration_type)> callback_;
};

/**
 * @class PerformanceMetrics
 * @brief Collects and calculates performance statistics
 */
class PerformanceMetrics {
public:
    void add_sample(std::chrono::nanoseconds duration) {
        samples_.push_back(duration.count());
    }

    void add_sample(int64_t nanoseconds) {
        samples_.push_back(nanoseconds);
    }

    double mean() const {
        if (samples_.empty()) return 0.0;
        int64_t sum = 0;
        for (auto s : samples_) {
            sum += s;
        }
        return static_cast<double>(sum) / samples_.size();
    }

    int64_t min() const {
        if (samples_.empty()) return 0;
        return *std::min_element(samples_.begin(), samples_.end());
    }

    int64_t max() const {
        if (samples_.empty()) return 0;
        return *std::max_element(samples_.begin(), samples_.end());
    }

    int64_t p50() const {
        return percentile(50);
    }

    int64_t p95() const {
        return percentile(95);
    }

    int64_t p99() const {
        return percentile(99);
    }

    size_t count() const {
        return samples_.size();
    }

    void clear() {
        samples_.clear();
    }

private:
    int64_t percentile(int p) const {
        if (samples_.empty()) return 0;
        auto sorted = samples_;
        std::sort(sorted.begin(), sorted.end());
        size_t index = (sorted.size() * p) / 100;
        if (index >= sorted.size()) index = sorted.size() - 1;
        return sorted[index];
    }

    std::vector<int64_t> samples_;
};

/**
 * @class WorkSimulator
 * @brief Simulates CPU work for testing
 */
class WorkSimulator {
public:
    /**
     * @brief Simulate CPU work for specified duration
     */
    static void simulate_work(std::chrono::microseconds duration) {
        auto start = std::chrono::high_resolution_clock::now();
        volatile int64_t sum = 0;
        while (std::chrono::high_resolution_clock::now() - start < duration) {
            sum += 1;
        }
    }

    /**
     * @brief Simulate variable CPU work with random duration
     */
    static void simulate_variable_work(
        std::chrono::microseconds min_duration,
        std::chrono::microseconds max_duration) {
        static thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<> dis(
            static_cast<int>(min_duration.count()),
            static_cast<int>(max_duration.count()));

        simulate_work(std::chrono::microseconds(dis(gen)));
    }
};

/**
 * @class BarrierSync
 * @brief Simple barrier synchronization for tests
 */
class BarrierSync {
public:
    explicit BarrierSync(size_t count)
        : threshold_(count), count_(count), generation_(0) {}

    void arrive_and_wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        auto gen = generation_;
        if (--count_ == 0) {
            generation_++;
            count_ = threshold_;
            cv_.notify_all();
        } else {
            cv_.wait(lock, [this, gen] { return gen != generation_; });
        }
    }

private:
    const size_t threshold_;
    size_t count_;
    size_t generation_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

/**
 * @class RateLimiter
 * @brief Controls the rate of operations
 */
class RateLimiter {
public:
    explicit RateLimiter(size_t ops_per_second)
        : interval_(std::chrono::seconds(1) / ops_per_second),
          last_op_(std::chrono::steady_clock::now()) {}

    void acquire() {
        std::unique_lock<std::mutex> lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - last_op_;

        if (elapsed < interval_) {
            std::this_thread::sleep_for(interval_ - elapsed);
        }

        last_op_ = std::chrono::steady_clock::now();
    }

private:
    const std::chrono::nanoseconds interval_;
    std::chrono::steady_clock::time_point last_op_;
    std::mutex mutex_;
};

/**
 * @class TempMetricStorage
 * @brief RAII wrapper for temporary metric storage
 */
class TempMetricStorage {
public:
    explicit TempMetricStorage(const std::string& prefix = "test_metrics")
        : path_(fs::temp_directory_path() / (prefix + "_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".dat")) {}

    ~TempMetricStorage() {
        if (fs::exists(path_)) {
            std::error_code ec;
            fs::remove(path_, ec);
        }
    }

    const std::string& path() const {
        return path_string_;
    }

    const fs::path& get_path() const {
        return path_;
    }

    std::string read() const {
        std::ifstream file(path_);
        if (!file.is_open()) {
            return "";
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    size_t size() const {
        if (!fs::exists(path_)) {
            return 0;
        }
        return fs::file_size(path_);
    }

private:
    fs::path path_;
    std::string path_string_{path_.string()};
};

/**
 * @class MockMetricExporter
 * @brief Mock exporter for testing metric export
 */
class MockMetricExporter {
public:
    MockMetricExporter() = default;

    void export_metrics(const std::vector<monitoring_system::compact_metric_value>& metrics) {
        export_count_.fetch_add(1);
        last_export_size_ = metrics.size();
        total_exported_.fetch_add(metrics.size());
    }

    void set_healthy(bool healthy) {
        healthy_ = healthy;
    }

    bool is_healthy() const {
        return healthy_;
    }

    size_t get_export_count() const {
        return export_count_.load();
    }

    size_t get_last_export_size() const {
        return last_export_size_;
    }

    size_t get_total_exported() const {
        return total_exported_.load();
    }

    void reset() {
        export_count_.store(0);
        last_export_size_ = 0;
        total_exported_.store(0);
    }

private:
    std::atomic<size_t> export_count_{0};
    size_t last_export_size_{0};
    std::atomic<size_t> total_exported_{0};
    bool healthy_{true};
};

/**
 * @brief Helper to wait for atomic counter to reach expected value
 */
template<typename T>
bool WaitForAtomicValue(const std::atomic<T>& counter,
                       T expected,
                       std::chrono::milliseconds timeout) {
    auto start = std::chrono::steady_clock::now();
    while (counter.load() < expected) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

/**
 * @brief Helper to measure throughput (operations per second)
 */
inline double CalculateThroughput(size_t operations,
                                 std::chrono::nanoseconds duration) {
    if (duration.count() == 0) return 0.0;
    return (static_cast<double>(operations) * 1'000'000'000.0) / duration.count();
}

/**
 * @brief Helper to format duration for display
 */
inline std::string FormatDuration(std::chrono::nanoseconds duration) {
    auto ns = duration.count();
    if (ns < 1'000) {
        return std::to_string(ns) + " ns";
    } else if (ns < 1'000'000) {
        return std::to_string(ns / 1'000) + " us";
    } else if (ns < 1'000'000'000) {
        return std::to_string(ns / 1'000'000) + " ms";
    } else {
        return std::to_string(ns / 1'000'000'000) + " s";
    }
}

/**
 * @brief Generate random string for testing
 */
inline std::string GenerateRandomString(size_t length) {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[dis(gen)];
    }
    return result;
}

/**
 * @brief Create test metric with random value
 */
inline monitoring_system::compact_metric_value CreateTestMetric(
    const std::string& name,
    monitoring_system::metric_type type = monitoring_system::metric_type::gauge) {

    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<> dis(0.0, 100.0);

    auto metadata = monitoring_system::create_metric_metadata(name, type);
    return monitoring_system::compact_metric_value(metadata, dis(gen));
}

/**
 * @brief Verify metric value is within expected range
 */
inline bool CheckMetricValue(const monitoring_system::compact_metric_value& metric,
                            double expected,
                            double tolerance = 0.01) {
    double actual = metric.as_double();
    return std::abs(actual - expected) <= tolerance;
}

/**
 * @brief Count metrics by type
 */
inline size_t CountMetricsByType(
    const std::vector<monitoring_system::compact_metric_value>& metrics,
    monitoring_system::metric_type type) {

    return std::count_if(metrics.begin(), metrics.end(),
        [type](const auto& metric) {
            return metric.metadata.type == type;
        });
}

/**
 * @brief Calculate total memory footprint of metrics
 */
inline size_t CalculateMetricsMemory(
    const std::vector<monitoring_system::compact_metric_value>& metrics) {

    size_t total = 0;
    for (const auto& metric : metrics) {
        total += metric.memory_footprint();
    }
    return total;
}

/**
 * @brief Generate metric batch for testing
 */
inline monitoring_system::metric_batch GenerateMetricBatch(size_t count) {
    monitoring_system::metric_batch batch;
    batch.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        auto metric = CreateTestMetric("test_metric_" + std::to_string(i));
        batch.add_metric(std::move(metric));
    }

    return batch;
}

} // namespace integration_tests
