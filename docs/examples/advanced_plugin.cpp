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
 * @file advanced_plugin.cpp
 * @brief Advanced collector plugin example with best practices
 *
 * This example demonstrates:
 * - Configuration handling
 * - Error handling and recovery
 * - Statistics tracking
 * - Platform-specific code
 * - Performance optimization
 * - Thread safety
 * - Dynamic loading support
 *
 * NOTE: This is a documentation example. For a complete working example
 * of a dynamically loadable plugin, see examples/plugin_example/
 *
 * To build as a shared library, integrate this into the monitoring_system
 * build system using CMake, similar to examples/plugin_example/CMakeLists.txt
 */

#include "kcenon/monitoring/plugins/collector_plugin.h"
#include "kcenon/monitoring/plugins/plugin_api.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <random>

namespace kcenon::monitoring {

/**
 * @class advanced_collector
 * @brief Advanced collector plugin with comprehensive features
 *
 * This plugin demonstrates best practices for production-ready
 * collector implementations.
 */
class advanced_collector : public collector_plugin {
public:
    advanced_collector()
        : generator_(std::random_device{}())
    {
    }

    ~advanced_collector() override {
        // Ensure cleanup on destruction
        if (initialized_) {
            shutdown();
        }
    }

    auto name() const -> std::string_view override {
        return "advanced_collector";
    }

    auto initialize(const config_map& config) -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);

        try {
            // Parse configuration
            if (config.contains("sample_interval")) {
                sample_interval_ = std::chrono::seconds(
                    std::stoi(config.at("sample_interval"))
                );
            }

            if (config.contains("metric_prefix")) {
                metric_prefix_ = config.at("metric_prefix");
            }

            if (config.contains("enable_debug")) {
                enable_debug_ = (config.at("enable_debug") == "true");
            }

            // Perform one-time initialization
            initialize_cache();

            initialized_ = true;
            return true;

        } catch (const std::exception& e) {
            std::cerr << "Initialization failed: " << e.what() << "\n";
            return false;
        }
    }

    void shutdown() override {
        std::lock_guard<std::mutex> lock(mutex_);

        // Clear cached data
        cached_hostname_.clear();

        // Reset state
        initialized_ = false;
        collection_count_ = 0;
        error_count_ = 0;
    }

    auto collect() -> std::vector<metric> override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto start_time = std::chrono::steady_clock::now();
        std::vector<metric> metrics;

        if (!initialized_) {
            std::cerr << "Plugin not initialized\n";
            return metrics;
        }

        try {
            // Increment collection counter
            ++collection_count_;

            // Collect CPU metric
            metrics.push_back(collect_cpu_metric());

            // Collect memory metric
            metrics.push_back(collect_memory_metric());

            // Collect disk metric (platform-specific)
            if (auto disk_metric = collect_disk_metric()) {
                metrics.push_back(*disk_metric);
            }

            // Collect statistics metric
            metrics.push_back(collect_statistics_metric());

        } catch (const std::exception& e) {
            ++error_count_;

            if (enable_debug_) {
                std::cerr << "Collection error: " << e.what() << "\n";
            }

            // Emit error metric
            metric error_metric;
            error_metric.name = metric_prefix_ + ".collection_errors";
            error_metric.value = 1.0;
            error_metric.timestamp = std::chrono::system_clock::now();
            metrics.push_back(error_metric);
        }

        // Update statistics
        auto end_time = std::chrono::steady_clock::now();
        last_collection_duration_ = std::chrono::duration_cast<
            std::chrono::microseconds>(end_time - start_time);

        return metrics;
    }

    auto interval() const -> std::chrono::milliseconds override {
        return sample_interval_;
    }

    auto is_available() const -> bool override {
        // Check platform compatibility
#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
        return true;
#else
        return false;
#endif
    }

    auto get_metadata() const -> plugin_metadata override {
        return plugin_metadata{
            .name = "advanced_collector",
            .description = "Advanced collector with best practices",
            .category = plugin_category::custom,
            .version = "2.0.0",
            .dependencies = {},
            .requires_platform_support = false
        };
    }

    auto get_statistics() const -> stats_map override {
        std::lock_guard<std::mutex> lock(mutex_);

        stats_map stats;
        stats["collection_count"] = std::to_string(collection_count_.load());
        stats["error_count"] = std::to_string(error_count_.load());
        stats["last_duration_us"] = std::to_string(
            last_collection_duration_.count()
        );
        stats["initialized"] = initialized_ ? "true" : "false";

        return stats;
    }

    auto get_metric_types() const -> std::vector<std::string> override {
        return {
            metric_prefix_ + ".cpu_usage",
            metric_prefix_ + ".memory_usage",
            metric_prefix_ + ".disk_usage",
            metric_prefix_ + ".collection_count"
        };
    }

private:
    /**
     * @brief Initialize cached data
     */
    void initialize_cache() {
        // Cache hostname (expensive to retrieve repeatedly)
        char hostname[256];
#if defined(__linux__) || defined(__APPLE__)
        if (gethostname(hostname, sizeof(hostname)) == 0) {
            cached_hostname_ = hostname;
        }
#elif defined(_WIN32)
        DWORD size = sizeof(hostname);
        if (GetComputerNameA(hostname, &size)) {
            cached_hostname_ = hostname;
        }
#endif
    }

    /**
     * @brief Collect CPU usage metric
     */
    auto collect_cpu_metric() -> metric {
        metric m;
        m.name = metric_prefix_ + ".cpu_usage";
        m.value = generate_random_value(0.0, 100.0);
        m.timestamp = std::chrono::system_clock::now();
        m.tags["unit"] = "percent";
        m.tags["collector"] = std::string(name());
        m.tags["hostname"] = cached_hostname_;

        return m;
    }

    /**
     * @brief Collect memory usage metric
     */
    auto collect_memory_metric() -> metric {
        metric m;
        m.name = metric_prefix_ + ".memory_usage";
        m.value = generate_random_value(0.0, 16384.0);
        m.timestamp = std::chrono::system_clock::now();
        m.tags["unit"] = "MB";
        m.tags["collector"] = std::string(name());
        m.tags["hostname"] = cached_hostname_;

        return m;
    }

    /**
     * @brief Collect disk usage metric (platform-specific)
     */
    auto collect_disk_metric() -> std::optional<metric> {
#if defined(__linux__) || defined(__APPLE__)
        try {
            // Use std::filesystem for cross-platform disk stats
            auto space = std::filesystem::space("/");
            double usage_percent = 100.0 *
                (1.0 - static_cast<double>(space.available) /
                       static_cast<double>(space.capacity));

            metric m;
            m.name = metric_prefix_ + ".disk_usage";
            m.value = usage_percent;
            m.timestamp = std::chrono::system_clock::now();
            m.tags["unit"] = "percent";
            m.tags["collector"] = std::string(name());
            m.tags["path"] = "/";

            return m;

        } catch (const std::exception&) {
            return std::nullopt;
        }
#else
        return std::nullopt;
#endif
    }

    /**
     * @brief Collect plugin statistics metric
     */
    auto collect_statistics_metric() -> metric {
        metric m;
        m.name = metric_prefix_ + ".collection_count";
        m.value = static_cast<double>(collection_count_.load());
        m.timestamp = std::chrono::system_clock::now();
        m.tags["unit"] = "count";
        m.tags["collector"] = std::string(name());

        return m;
    }

    /**
     * @brief Generate random value for demonstration
     */
    auto generate_random_value(double min, double max) -> double {
        std::uniform_real_distribution<double> dist(min, max);
        return dist(generator_);
    }

    // Configuration
    std::string metric_prefix_{"advanced"};
    std::chrono::milliseconds sample_interval_{std::chrono::seconds(5)};
    bool enable_debug_{false};

    // State
    bool initialized_{false};

    // Statistics (thread-safe)
    std::atomic<uint64_t> collection_count_{0};
    std::atomic<uint64_t> error_count_{0};
    std::chrono::microseconds last_collection_duration_{0};

    // Cached data
    std::string cached_hostname_;

    // Thread safety
    mutable std::mutex mutex_;

    // Random number generation
    std::mt19937 generator_;
};

} // namespace kcenon::monitoring

// Export plugin for dynamic loading
IMPLEMENT_PLUGIN(
    kcenon::monitoring::advanced_collector,
    "advanced_collector",
    "2.0.0",
    "Advanced collector plugin with best practices",
    "kcenon",
    "custom"
)
