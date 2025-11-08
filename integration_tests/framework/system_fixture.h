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

#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <functional>
#include <fstream>
#include <filesystem>
#include <vector>
#include <sstream>
#include <unordered_map>

#include <kcenon/monitoring/core/performance_monitor.h>
#include <kcenon/monitoring/core/error_codes.h>
#include <kcenon/monitoring/utils/metric_types.h>
#include <kcenon/monitoring/interfaces/metric_collector_interface.h>

namespace integration_tests {

namespace fs = std::filesystem;

/**
 * @class MonitoringSystemFixture
 * @brief Base fixture for integration tests providing common setup and teardown
 *
 * This fixture provides:
 * - Performance monitor creation and management
 * - Metric collection and verification
 * - Temporary storage management for monitoring data
 * - Common test utilities
 * - Cleanup and verification
 */
class MonitoringSystemFixture : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test data
        temp_dir_ = fs::temp_directory_path() / ("monitoring_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(temp_dir_);

        // Reset counters
        metric_count_.store(0);
        error_count_.store(0);

        // Create performance monitor
        monitor_ = std::make_unique<kcenon::monitoring::performance_monitor>("test_monitor");
    }

    void TearDown() override {
        // Stop monitor if still running
        if (monitor_) {
            monitor_->cleanup();
        }

        monitor_.reset();

        // Clean up temporary files
        if (fs::exists(temp_dir_)) {
            std::error_code ec;
            fs::remove_all(temp_dir_, ec);
        }

        // Clean up tracked temp files
        for (const auto& file : temp_files_) {
            if (fs::exists(file)) {
                std::error_code ec;
                fs::remove(file, ec);
            }
        }
    }

    /**
     * @brief Create a performance monitor with specified configuration
     */
    void CreateMonitor(const std::string& name = "test_monitor") {
        monitor_ = std::make_unique<kcenon::monitoring::performance_monitor>(name);
    }

    /**
     * @brief Initialize and start monitoring
     */
    bool StartMonitoring() {
        if (!monitor_) {
            return false;
        }

        auto result = monitor_->initialize();
        return result.is_ok();
    }

    /**
     * @brief Get a temporary file path
     */
    std::string GetTempFilePath(const std::string& name) {
        auto path = temp_dir_ / name;
        temp_files_.push_back(path);
        return path.string();
    }

    /**
     * @brief Wait for a condition to become true with timeout
     */
    template<typename Predicate>
    bool WaitForCondition(Predicate pred,
                         std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
        auto start = std::chrono::steady_clock::now();
        while (!pred()) {
            if (std::chrono::steady_clock::now() - start > timeout) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    }

    /**
     * @brief Wait for metric collection to complete
     */
    void WaitForCollection(std::chrono::milliseconds timeout = std::chrono::seconds(2)) {
        std::this_thread::sleep_for(timeout);
    }

    /**
     * @brief Create a metric with specified parameters
     */
    kcenon::monitoring::compact_metric_value CreateMetric(
        const std::string& name,
        kcenon::monitoring::metric_type type,
        double value) {

        auto metadata = kcenon::monitoring::create_metric_metadata(name, type);
        return kcenon::monitoring::compact_metric_value(metadata, value);
    }

    /**
     * @brief Get metric value by name from collected metrics
     */
    std::optional<double> GetMetricValue(const std::string& name) {
        if (!monitor_) {
            return std::nullopt;
        }

        auto result = monitor_->collect();
        if (result.is_err()) {
            return std::nullopt;
        }

        auto snapshot = result.value();
        return snapshot.get_metric(name);
    }

    /**
     * @brief Count metrics in snapshot
     */
    size_t CountMetrics() {
        if (!monitor_) {
            return 0;
        }

        auto result = monitor_->collect();
        if (result.is_err()) {
            return 0;
        }

        return result.value().metrics.size();
    }

    /**
     * @brief Record a performance sample
     */
    bool RecordSample(const std::string& operation, std::chrono::nanoseconds duration) {
        if (!monitor_) {
            return false;
        }

        auto result = monitor_->get_profiler().record_sample(operation, duration);
        return result.is_ok();
    }

    /**
     * @brief Get performance metrics for an operation
     */
    std::optional<kcenon::monitoring::performance_metrics> GetPerformanceMetrics(
        const std::string& operation) {

        if (!monitor_) {
            return std::nullopt;
        }

        auto result = monitor_->get_profiler().get_metrics(operation);
        if (result.is_err()) {
            return std::nullopt;
        }

        return result.value();
    }

    /**
     * @brief Wait for file to exist
     */
    bool WaitForFile(const std::string& filepath,
                    std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
        return WaitForCondition([&filepath]() {
            return fs::exists(filepath);
        }, timeout);
    }

    /**
     * @brief Read file contents
     */
    std::string ReadFile(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    /**
     * @brief Check if file contains specific text
     */
    bool FileContains(const std::string& filepath, const std::string& text) {
        auto contents = ReadFile(filepath);
        return contents.find(text) != std::string::npos;
    }

    // Protected member variables
    std::unique_ptr<kcenon::monitoring::performance_monitor> monitor_;
    fs::path temp_dir_;
    std::vector<fs::path> temp_files_;

    std::atomic<size_t> metric_count_{0};
    std::atomic<size_t> error_count_{0};
};

/**
 * @class MultiMonitorFixture
 * @brief Fixture for tests requiring multiple monitor instances
 */
class MultiMonitorFixture : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = fs::temp_directory_path() / ("monitoring_multi_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(temp_dir_);
    }

    void TearDown() override {
        // Stop all monitors
        for (auto& monitor : monitors_) {
            if (monitor) {
                monitor->cleanup();
            }
        }
        monitors_.clear();

        // Clean up temporary directory
        if (fs::exists(temp_dir_)) {
            std::error_code ec;
            fs::remove_all(temp_dir_, ec);
        }
    }

    /**
     * @brief Create multiple monitors
     */
    void CreateMultipleMonitors(size_t count) {
        for (size_t i = 0; i < count; ++i) {
            auto monitor = std::make_unique<kcenon::monitoring::performance_monitor>(
                "monitor_" + std::to_string(i)
            );

            monitor->initialize();
            monitors_.push_back(std::move(monitor));
        }
    }

    /**
     * @brief Wait for condition with timeout
     */
    template<typename Predicate>
    bool WaitForCondition(Predicate pred,
                         std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
        auto start = std::chrono::steady_clock::now();
        while (!pred()) {
            if (std::chrono::steady_clock::now() - start > timeout) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return true;
    }

    std::vector<std::unique_ptr<kcenon::monitoring::performance_monitor>> monitors_;
    fs::path temp_dir_;
};

} // namespace integration_tests
