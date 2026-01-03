// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
// All rights reserved.
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
 * @file collectors.cppm
 * @brief Collectors partition for kcenon.monitoring module
 *
 * This module partition provides all metric collector implementations
 * for the monitoring system.
 *
 * Contents:
 * - collector_base CRTP class
 * - System resource collectors (CPU, memory, disk, network)
 * - Hardware collectors (battery, temperature, GPU, power)
 * - System information collectors (threads, context switches, interrupts)
 * - Network collectors (TCP state, socket buffer)
 * - Container/VM collectors
 * - Plugin collector interface
 */
module;

// Standard library includes (global module fragment)
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

export module kcenon.monitoring.collectors;

export import kcenon.monitoring.core;

// ============================================================================
// Type Aliases
// ============================================================================

export namespace kcenon::monitoring {

/**
 * @brief Type alias for configuration map
 */
using config_map = std::unordered_map<std::string, std::string>;

/**
 * @brief Type alias for statistics map
 */
using stats_map = std::unordered_map<std::string, double>;

/**
 * @brief Type alias for label map
 */
using label_map = std::unordered_map<std::string, std::string>;

// ============================================================================
// Collector Base
// ============================================================================

/**
 * @class collector_base
 * @brief CRTP base class for metric collectors
 *
 * This template class implements common functionality shared by all collectors:
 * - Configuration parsing (enabled state)
 * - Collection with error handling and statistics
 * - Health monitoring
 * - Statistics tracking (collection count, error count)
 *
 * @tparam Derived The derived collector class (CRTP pattern)
 *
 * Usage:
 * @code
 * class my_collector : public collector_base<my_collector> {
 * public:
 *     static constexpr const char* collector_name = "my_collector";
 *
 *     bool do_initialize(const config_map& config) {
 *         return true;
 *     }
 *
 *     std::vector<metric_sample> do_collect() {
 *         return metrics;
 *     }
 *
 *     bool is_available() const {
 *         return true;
 *     }
 *
 *     std::vector<std::string> do_get_metric_types() const {
 *         return {"metric_type_1"};
 *     }
 *
 *     void do_add_statistics(stats_map& stats) const {
 *         // Add collector-specific statistics
 *     }
 * };
 * @endcode
 */
template<typename Derived>
class collector_base {
public:
    collector_base() = default;
    virtual ~collector_base() = default;

    // Non-copyable, non-moveable
    collector_base(const collector_base&) = delete;
    collector_base& operator=(const collector_base&) = delete;
    collector_base(collector_base&&) = delete;
    collector_base& operator=(collector_base&&) = delete;

    /**
     * @brief Initialize the collector with configuration
     * @param config Configuration options (common: "enabled")
     * @return true if initialization successful
     */
    bool initialize(const config_map& config) {
        if (auto it = config.find("enabled"); it != config.end()) {
            enabled_ = (it->second == "true" || it->second == "1");
        }
        return derived().do_initialize(config);
    }

    /**
     * @brief Collect metrics from the data source
     * @return Collection of metrics
     */
    std::vector<metric_sample> collect() {
        if (!enabled_) {
            return {};
        }

        try {
            auto metrics = derived().do_collect();
            ++collection_count_;
            return metrics;
        } catch (...) {
            ++collection_errors_;
            return {};
        }
    }

    /**
     * @brief Get the name of this collector
     * @return Collector name from Derived::collector_name
     */
    std::string name() const {
        return Derived::collector_name;
    }

    /**
     * @brief Get supported metric types
     * @return Vector of supported metric type names
     */
    std::vector<std::string> get_metric_types() const {
        return derived().do_get_metric_types();
    }

    /**
     * @brief Check if the collector is healthy
     * @return true if collector is operational
     */
    bool is_healthy() const {
        if (!enabled_) {
            return true;  // Disabled collectors are considered healthy
        }
        return derived().is_available();
    }

    /**
     * @brief Get collector statistics
     * @return Map of statistic name to value
     */
    stats_map get_statistics() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_map stats;

        stats["enabled"] = enabled_ ? 1.0 : 0.0;
        stats["collection_count"] = static_cast<double>(collection_count_.load());
        stats["collection_errors"] = static_cast<double>(collection_errors_.load());

        derived().do_add_statistics(stats);
        return stats;
    }

    /**
     * @brief Check if collector is enabled
     */
    bool enabled() const noexcept { return enabled_; }

    /**
     * @brief Enable the collector
     */
    void enable() noexcept { enabled_ = true; }

    /**
     * @brief Disable the collector
     */
    void disable() noexcept { enabled_ = false; }

    /**
     * @brief Get collection count
     */
    std::size_t get_collection_count() const noexcept {
        return collection_count_.load();
    }

    /**
     * @brief Get error count
     */
    std::size_t get_collection_errors() const noexcept {
        return collection_errors_.load();
    }

protected:
    /**
     * @brief Create a metric with common tags
     * @param name Metric name
     * @param value Metric value
     * @param tags Additional tags
     * @return Created metric sample
     */
    metric_sample create_metric(const std::string& name, double value,
                                 const label_map& tags = {}) const {
        metric_sample m;
        m.name = name;
        m.value = value;
        m.timestamp = std::chrono::system_clock::now();
        m.labels = tags;
        m.labels["collector"] = Derived::collector_name;
        return m;
    }

    /**
     * @brief Create a metric with integer value
     */
    metric_sample create_metric(const std::string& name, int64_t value,
                                 const label_map& tags = {}) const {
        metric_sample m;
        m.name = name;
        m.value = value;
        m.timestamp = std::chrono::system_clock::now();
        m.labels = tags;
        m.labels["collector"] = Derived::collector_name;
        return m;
    }

    bool enabled_{true};
    mutable std::mutex stats_mutex_;
    std::atomic<std::size_t> collection_count_{0};
    std::atomic<std::size_t> collection_errors_{0};

private:
    Derived& derived() { return static_cast<Derived&>(*this); }
    const Derived& derived() const { return static_cast<const Derived&>(*this); }
};

// ============================================================================
// Collector Interface
// ============================================================================

/**
 * @class interface_collector
 * @brief Abstract interface for metric collectors
 *
 * This interface provides a non-template base for collectors,
 * enabling polymorphic collector management.
 */
class interface_collector {
public:
    virtual ~interface_collector() = default;

    /**
     * @brief Initialize the collector
     * @param config Configuration map
     * @return true if successful
     */
    virtual bool initialize(const config_map& config) = 0;

    /**
     * @brief Collect metrics
     * @return Vector of metric samples
     */
    virtual std::vector<metric_sample> collect() = 0;

    /**
     * @brief Get collector name
     * @return Collector name
     */
    virtual std::string name() const = 0;

    /**
     * @brief Get supported metric types
     * @return Vector of metric type names
     */
    virtual std::vector<std::string> get_metric_types() const = 0;

    /**
     * @brief Check if collector is enabled
     */
    virtual bool enabled() const = 0;

    /**
     * @brief Enable the collector
     */
    virtual void enable() = 0;

    /**
     * @brief Disable the collector
     */
    virtual void disable() = 0;

    /**
     * @brief Check if collector is healthy
     */
    virtual bool is_healthy() const = 0;

    /**
     * @brief Get collector statistics
     */
    virtual stats_map get_statistics() const = 0;
};

// ============================================================================
// System Resource Metrics
// ============================================================================

/**
 * @struct cpu_metrics
 * @brief CPU usage metrics
 */
struct cpu_metrics {
    double user_percent{0.0};
    double system_percent{0.0};
    double idle_percent{0.0};
    double iowait_percent{0.0};
    double nice_percent{0.0};
    double irq_percent{0.0};
    double softirq_percent{0.0};
    double steal_percent{0.0};
    double guest_percent{0.0};
    uint32_t core_count{0};
    double load_average_1m{0.0};
    double load_average_5m{0.0};
    double load_average_15m{0.0};
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct memory_metrics
 * @brief Memory usage metrics
 */
struct memory_metrics {
    uint64_t total_bytes{0};
    uint64_t available_bytes{0};
    uint64_t used_bytes{0};
    uint64_t free_bytes{0};
    uint64_t cached_bytes{0};
    uint64_t buffers_bytes{0};
    uint64_t swap_total_bytes{0};
    uint64_t swap_used_bytes{0};
    uint64_t swap_free_bytes{0};
    double used_percent{0.0};
    double swap_used_percent{0.0};
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct disk_metrics
 * @brief Disk usage metrics
 */
struct disk_metrics {
    std::string device;
    std::string mount_point;
    std::string filesystem_type;
    uint64_t total_bytes{0};
    uint64_t used_bytes{0};
    uint64_t available_bytes{0};
    double used_percent{0.0};
    uint64_t read_bytes{0};
    uint64_t write_bytes{0};
    uint64_t read_ops{0};
    uint64_t write_ops{0};
    double read_bytes_per_sec{0.0};
    double write_bytes_per_sec{0.0};
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct network_metrics
 * @brief Network interface metrics
 */
struct network_metrics {
    std::string interface_name;
    uint64_t bytes_sent{0};
    uint64_t bytes_received{0};
    uint64_t packets_sent{0};
    uint64_t packets_received{0};
    uint64_t errors_in{0};
    uint64_t errors_out{0};
    uint64_t drops_in{0};
    uint64_t drops_out{0};
    double bytes_sent_per_sec{0.0};
    double bytes_received_per_sec{0.0};
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct process_metrics
 * @brief Process metrics
 */
struct process_metrics {
    uint32_t pid{0};
    std::string name;
    std::string state;
    double cpu_percent{0.0};
    uint64_t memory_bytes{0};
    double memory_percent{0.0};
    uint64_t virtual_memory_bytes{0};
    uint64_t resident_memory_bytes{0};
    uint32_t thread_count{0};
    uint64_t open_files{0};
    std::chrono::system_clock::time_point timestamp;
};

// ============================================================================
// Hardware Metrics
// ============================================================================

/**
 * @struct battery_metrics
 * @brief Battery status metrics
 */
struct battery_metrics {
    double charge_percent{0.0};
    bool is_charging{false};
    bool is_plugged{false};
    std::optional<std::chrono::seconds> time_to_empty;
    std::optional<std::chrono::seconds> time_to_full;
    double voltage{0.0};
    double current{0.0};
    double temperature{0.0};
    std::string health;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct temperature_metrics
 * @brief Temperature sensor metrics
 */
struct temperature_metrics {
    std::string sensor_name;
    std::string label;
    double current_celsius{0.0};
    std::optional<double> high_threshold_celsius;
    std::optional<double> critical_threshold_celsius;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct gpu_metrics
 * @brief GPU metrics
 */
struct gpu_metrics {
    std::string device_name;
    uint32_t device_id{0};
    double utilization_percent{0.0};
    uint64_t memory_total_bytes{0};
    uint64_t memory_used_bytes{0};
    uint64_t memory_free_bytes{0};
    double temperature_celsius{0.0};
    uint32_t fan_speed_percent{0};
    uint32_t power_usage_watts{0};
    uint32_t clock_speed_mhz{0};
    uint32_t memory_clock_mhz{0};
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct power_metrics
 * @brief Power consumption metrics
 */
struct power_metrics {
    double total_watts{0.0};
    double cpu_watts{0.0};
    double memory_watts{0.0};
    double gpu_watts{0.0};
    double other_watts{0.0};
    std::string power_source;  // "AC", "battery", "unknown"
    std::chrono::system_clock::time_point timestamp;
};

// ============================================================================
// System Information Metrics
// ============================================================================

/**
 * @struct thread_metrics
 * @brief Thread metrics
 */
struct thread_metrics {
    uint32_t total_threads{0};
    uint32_t running_threads{0};
    uint32_t sleeping_threads{0};
    uint32_t stopped_threads{0};
    uint32_t zombie_threads{0};
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct context_switch_metrics
 * @brief Context switch metrics
 */
struct context_switch_metrics {
    uint64_t voluntary_switches{0};
    uint64_t involuntary_switches{0};
    double switches_per_sec{0.0};
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct interrupt_metrics
 * @brief Interrupt metrics
 */
struct interrupt_metrics {
    std::string irq_name;
    uint64_t count{0};
    double per_second{0.0};
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct fd_metrics
 * @brief File descriptor metrics
 */
struct fd_metrics {
    uint64_t open_count{0};
    uint64_t max_count{0};
    double usage_percent{0.0};
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct inode_metrics
 * @brief Inode metrics
 */
struct inode_metrics {
    std::string filesystem;
    uint64_t total_inodes{0};
    uint64_t used_inodes{0};
    uint64_t free_inodes{0};
    double used_percent{0.0};
    std::chrono::system_clock::time_point timestamp;
};

// ============================================================================
// Network State Metrics
// ============================================================================

/**
 * @struct tcp_state_metrics
 * @brief TCP connection state metrics
 */
struct tcp_state_metrics {
    uint32_t established{0};
    uint32_t syn_sent{0};
    uint32_t syn_recv{0};
    uint32_t fin_wait1{0};
    uint32_t fin_wait2{0};
    uint32_t time_wait{0};
    uint32_t close{0};
    uint32_t close_wait{0};
    uint32_t last_ack{0};
    uint32_t listen{0};
    uint32_t closing{0};
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct socket_buffer_metrics
 * @brief Socket buffer metrics
 */
struct socket_buffer_metrics {
    uint64_t recv_buffer_bytes{0};
    uint64_t send_buffer_bytes{0};
    uint64_t recv_buffer_max{0};
    uint64_t send_buffer_max{0};
    double recv_buffer_usage_percent{0.0};
    double send_buffer_usage_percent{0.0};
    std::chrono::system_clock::time_point timestamp;
};

// ============================================================================
// Container/VM Metrics
// ============================================================================

/**
 * @struct container_metrics
 * @brief Container resource metrics
 */
struct container_metrics {
    std::string container_id;
    std::string container_name;
    std::string image;
    std::string state;
    double cpu_percent{0.0};
    uint64_t memory_bytes{0};
    uint64_t memory_limit_bytes{0};
    double memory_percent{0.0};
    uint64_t network_rx_bytes{0};
    uint64_t network_tx_bytes{0};
    uint64_t block_read_bytes{0};
    uint64_t block_write_bytes{0};
    uint32_t pids{0};
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct vm_metrics
 * @brief Virtual machine metrics
 */
struct vm_metrics {
    std::string vm_id;
    std::string vm_name;
    std::string hypervisor;
    std::string state;
    uint32_t vcpu_count{0};
    double cpu_percent{0.0};
    uint64_t memory_bytes{0};
    uint64_t memory_allocated_bytes{0};
    uint64_t disk_bytes{0};
    uint64_t network_rx_bytes{0};
    uint64_t network_tx_bytes{0};
    std::chrono::system_clock::time_point timestamp;
};

// ============================================================================
// Uptime and SMART Metrics
// ============================================================================

/**
 * @struct uptime_metrics
 * @brief System uptime metrics
 */
struct uptime_metrics {
    std::chrono::seconds uptime{0};
    std::chrono::seconds idle_time{0};
    std::chrono::system_clock::time_point boot_time;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @struct smart_metrics
 * @brief S.M.A.R.T. disk health metrics
 */
struct smart_metrics {
    std::string device;
    std::string model;
    std::string serial;
    bool smart_enabled{false};
    bool healthy{true};
    uint32_t temperature_celsius{0};
    uint64_t power_on_hours{0};
    uint64_t power_cycle_count{0};
    uint64_t reallocated_sectors{0};
    uint64_t pending_sectors{0};
    uint64_t uncorrectable_errors{0};
    std::chrono::system_clock::time_point timestamp;
};

// ============================================================================
// Security Metrics
// ============================================================================

/**
 * @struct security_metrics
 * @brief Security-related metrics
 */
struct security_metrics {
    uint32_t failed_login_attempts{0};
    uint32_t active_sessions{0};
    uint32_t sudo_commands{0};
    uint32_t firewall_blocked{0};
    bool selinux_enforcing{false};
    bool apparmor_enabled{false};
    std::chrono::system_clock::time_point timestamp;
};

// ============================================================================
// Plugin Collector Interface
// ============================================================================

/**
 * @class plugin_collector
 * @brief Interface for plugin-based metric collectors
 *
 * This interface allows external plugins to provide custom metric collection.
 */
class plugin_collector : public interface_collector {
public:
    using collect_function = std::function<std::vector<metric_sample>()>;
    using health_function = std::function<bool()>;

    plugin_collector(std::string name,
                     collect_function collect_fn,
                     std::vector<std::string> metric_types)
        : name_(std::move(name))
        , collect_fn_(std::move(collect_fn))
        , metric_types_(std::move(metric_types))
        , enabled_(true) {}

    bool initialize(const config_map& config) override {
        if (auto it = config.find("enabled"); it != config.end()) {
            enabled_ = (it->second == "true" || it->second == "1");
        }
        return true;
    }

    std::vector<metric_sample> collect() override {
        if (!enabled_ || !collect_fn_) {
            return {};
        }
        try {
            auto metrics = collect_fn_();
            ++collection_count_;
            return metrics;
        } catch (...) {
            ++collection_errors_;
            return {};
        }
    }

    std::string name() const override { return name_; }

    std::vector<std::string> get_metric_types() const override {
        return metric_types_;
    }

    bool enabled() const override { return enabled_; }
    void enable() override { enabled_ = true; }
    void disable() override { enabled_ = false; }

    bool is_healthy() const override {
        if (health_fn_) {
            return health_fn_();
        }
        return enabled_;
    }

    stats_map get_statistics() const override {
        return {
            {"enabled", enabled_ ? 1.0 : 0.0},
            {"collection_count", static_cast<double>(collection_count_.load())},
            {"collection_errors", static_cast<double>(collection_errors_.load())}
        };
    }

    /**
     * @brief Set custom health check function
     */
    void set_health_check(health_function fn) {
        health_fn_ = std::move(fn);
    }

private:
    std::string name_;
    collect_function collect_fn_;
    health_function health_fn_;
    std::vector<std::string> metric_types_;
    bool enabled_;
    std::atomic<std::size_t> collection_count_{0};
    std::atomic<std::size_t> collection_errors_{0};
};

// ============================================================================
// Collector Registry
// ============================================================================

/**
 * @class collector_registry
 * @brief Registry for managing metric collectors
 */
class collector_registry {
public:
    using collector_ptr = std::shared_ptr<interface_collector>;

    /**
     * @brief Get the singleton instance
     */
    static collector_registry& instance() {
        static collector_registry registry;
        return registry;
    }

    /**
     * @brief Register a collector
     * @param collector The collector to register
     * @return true if registered successfully
     */
    bool register_collector(collector_ptr collector) {
        if (!collector) {
            return false;
        }
        std::lock_guard<std::shared_mutex> lock(mutex_);
        auto name = collector->name();
        if (collectors_.find(name) != collectors_.end()) {
            return false;  // Already registered
        }
        collectors_[name] = std::move(collector);
        return true;
    }

    /**
     * @brief Unregister a collector
     * @param name The collector name
     * @return true if unregistered successfully
     */
    bool unregister_collector(const std::string& name) {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        return collectors_.erase(name) > 0;
    }

    /**
     * @brief Get a collector by name
     * @param name The collector name
     * @return Collector pointer or nullptr
     */
    collector_ptr get_collector(const std::string& name) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = collectors_.find(name);
        if (it != collectors_.end()) {
            return it->second;
        }
        return nullptr;
    }

    /**
     * @brief Get all registered collectors
     * @return Vector of collector pointers
     */
    std::vector<collector_ptr> get_all_collectors() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<collector_ptr> result;
        result.reserve(collectors_.size());
        for (const auto& [name, collector] : collectors_) {
            result.push_back(collector);
        }
        return result;
    }

    /**
     * @brief Collect metrics from all enabled collectors
     * @return Vector of all collected metric samples
     */
    std::vector<metric_sample> collect_all() {
        std::vector<metric_sample> all_metrics;
        std::shared_lock<std::shared_mutex> lock(mutex_);
        for (const auto& [name, collector] : collectors_) {
            if (collector->enabled()) {
                auto metrics = collector->collect();
                all_metrics.insert(all_metrics.end(),
                                   std::make_move_iterator(metrics.begin()),
                                   std::make_move_iterator(metrics.end()));
            }
        }
        return all_metrics;
    }

    /**
     * @brief Get names of all registered collectors
     */
    std::vector<std::string> get_collector_names() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(collectors_.size());
        for (const auto& [name, _] : collectors_) {
            names.push_back(name);
        }
        return names;
    }

    /**
     * @brief Clear all registered collectors
     */
    void clear() {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        collectors_.clear();
    }

private:
    collector_registry() = default;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, collector_ptr> collectors_;
};

} // namespace kcenon::monitoring
