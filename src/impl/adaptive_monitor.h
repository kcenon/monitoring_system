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
 * @file adaptive_monitor.h
 * @brief Adaptive monitoring implementation that adjusts behavior based on system load
 * @date 2025
 * 
 * Provides adaptive monitoring capabilities that automatically adjust
 * collection intervals, sampling rates, and metric granularity based
 * on current system resource utilization.
 */

#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <functional>
#include <thread>
#include <cmath>
#include <random>

#include <kcenon/monitoring/core/result_types.h>
#include <kcenon/monitoring/core/error_codes.h>
#include <kcenon/monitoring/interfaces/monitoring_interface.h>
#include <kcenon/monitoring/core/performance_monitor.h>

namespace kcenon { namespace monitoring {

/**
 * @brief Adaptation strategy for monitoring behavior
 */
enum class adaptation_strategy {
    conservative,  // Prefer system stability over monitoring detail
    balanced,      // Balance between monitoring and performance
    aggressive     // Prefer monitoring detail over system resources
};

/**
 * @brief System load levels
 */
enum class load_level {
    idle,          // < 20% CPU
    low,           // 20-40% CPU
    moderate,      // 40-60% CPU
    high,          // 60-80% CPU
    critical       // > 80% CPU
};

/**
 * @brief Adaptive configuration parameters
 */
struct adaptive_config {
    // Thresholds for load levels (CPU percentage)
    double idle_threshold{20.0};
    double low_threshold{40.0};
    double moderate_threshold{60.0};
    double high_threshold{80.0};

    // Memory thresholds (percentage)
    double memory_warning_threshold{70.0};
    double memory_critical_threshold{85.0};

    // Collection intervals by load level (milliseconds)
    std::chrono::milliseconds idle_interval{100};
    std::chrono::milliseconds low_interval{250};
    std::chrono::milliseconds moderate_interval{500};
    std::chrono::milliseconds high_interval{1000};
    std::chrono::milliseconds critical_interval{5000};

    // Sampling rates by load level (0.0 to 1.0)
    double idle_sampling_rate{1.0};
    double low_sampling_rate{0.8};
    double moderate_sampling_rate{0.5};
    double high_sampling_rate{0.2};
    double critical_sampling_rate{0.1};

    // Adaptation parameters
    adaptation_strategy strategy{adaptation_strategy::balanced};
    std::chrono::seconds adaptation_interval{10};
    double smoothing_factor{0.7};  // Exponential smoothing for load average

    // Threshold tuning parameters (ARC-005)
    double hysteresis_margin{5.0};  // Percentage margin to prevent oscillation
    std::chrono::milliseconds cooldown_period{1000};  // Minimum time between level changes
    bool enable_hysteresis{true};  // Enable/disable hysteresis
    bool enable_cooldown{true};  // Enable/disable cooldown
    
    /**
     * @brief Get collection interval for load level
     */
    std::chrono::milliseconds get_interval_for_load(load_level level) const {
        switch (level) {
            case load_level::idle: return idle_interval;
            case load_level::low: return low_interval;
            case load_level::moderate: return moderate_interval;
            case load_level::high: return high_interval;
            case load_level::critical: return critical_interval;
        }
        return moderate_interval;
    }
    
    /**
     * @brief Get sampling rate for load level
     */
    double get_sampling_rate_for_load(load_level level) const {
        switch (level) {
            case load_level::idle: return idle_sampling_rate;
            case load_level::low: return low_sampling_rate;
            case load_level::moderate: return moderate_sampling_rate;
            case load_level::high: return high_sampling_rate;
            case load_level::critical: return critical_sampling_rate;
        }
        return moderate_sampling_rate;
    }
};

/**
 * @brief Adaptation statistics
 */
struct adaptation_stats {
    std::uint64_t total_adaptations{0};
    std::uint64_t upscale_count{0};
    std::uint64_t downscale_count{0};
    std::uint64_t samples_dropped{0};
    std::uint64_t samples_collected{0};
    double average_cpu_usage{0.0};
    double average_memory_usage{0.0};
    load_level current_load_level{load_level::moderate};
    std::chrono::milliseconds current_interval;
    double current_sampling_rate{1.0};
    std::chrono::system_clock::time_point last_adaptation;

    // Threshold tuning statistics (ARC-005)
    std::uint64_t hysteresis_prevented_changes{0};  // Changes prevented by hysteresis
    std::uint64_t cooldown_prevented_changes{0};    // Changes prevented by cooldown
    std::chrono::system_clock::time_point last_level_change;  // Time of last level change
};

/**
 * @brief Adaptive collector wrapper
 *
 * @thread_safety Thread-safe. All public methods can be called concurrently.
 *   - config_ protected by config_mutex_
 *   - stats_ protected by stats_mutex_
 *   - Uses std::atomic for enabled_ and current_sampling_rate_
 *   - Avoids deadlock by copying config before acquiring stats_mutex_
 */
class adaptive_collector {
private:
    std::shared_ptr<kcenon::monitoring::metrics_collector> collector_;
    adaptive_config config_;
    mutable std::mutex config_mutex_;  // Protects config_
    adaptation_stats stats_;
    std::atomic<bool> enabled_{true};
    std::atomic<double> current_sampling_rate_{1.0};
    mutable std::mutex stats_mutex_;
    
public:
    adaptive_collector(
        std::shared_ptr<kcenon::monitoring::metrics_collector> collector,
        const adaptive_config& config = {}
    ) : collector_(collector), config_(config) {
        stats_.current_interval = config_.moderate_interval;
        stats_.last_adaptation = std::chrono::system_clock::now();
    }
    
    /**
     * @brief Collect metrics with adaptive sampling
     */
    kcenon::monitoring::result<kcenon::monitoring::metrics_snapshot> collect() {
        if (!should_sample()) {
            stats_.samples_dropped++;
            return kcenon::monitoring::make_error<kcenon::monitoring::metrics_snapshot>(
                kcenon::monitoring::monitoring_error_code::operation_cancelled,
                "Sample dropped due to adaptive sampling"
            );
        }
        
        stats_.samples_collected++;
        return collector_->collect();
    }
    
    /**
     * @brief Adapt collection behavior based on load
     * @thread_safety Thread-safe. Uses mutex for synchronization.
     */
    void adapt(const kcenon::monitoring::system_metrics& sys_metrics) {
        // Copy config under lock to avoid holding both locks
        adaptive_config local_config;
        {
            std::lock_guard<std::mutex> config_lock(config_mutex_);
            local_config = config_;
        }

        std::lock_guard<std::mutex> lock(stats_mutex_);

        bool is_first_adaptation = (stats_.total_adaptations == 0);

        // Initialize averages on first adaptation
        if (is_first_adaptation) {
            stats_.average_cpu_usage = sys_metrics.cpu_usage_percent;
            stats_.average_memory_usage = sys_metrics.memory_usage_percent;
        } else {
            // Update average metrics using exponential smoothing
            stats_.average_cpu_usage =
                local_config.smoothing_factor * sys_metrics.cpu_usage_percent +
                (1.0 - local_config.smoothing_factor) * stats_.average_cpu_usage;

            stats_.average_memory_usage =
                local_config.smoothing_factor * sys_metrics.memory_usage_percent +
                (1.0 - local_config.smoothing_factor) * stats_.average_memory_usage;
        }

        // Determine load level with hysteresis support
        auto new_level = calculate_load_level_with_hysteresis(
            stats_.average_cpu_usage,
            stats_.average_memory_usage,
            stats_.current_load_level,
            local_config
        );

        // Adapt if load level changed
        if (new_level != stats_.current_load_level) {
            auto now = std::chrono::system_clock::now();

            // Check cooldown period (ARC-005)
            // Skip cooldown check for first adaptation to allow initial level setting
            if (local_config.enable_cooldown && !is_first_adaptation) {
                auto time_since_last_change = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - stats_.last_level_change
                );
                if (time_since_last_change < local_config.cooldown_period) {
                    stats_.cooldown_prevented_changes++;
                    return;  // Skip this adaptation due to cooldown
                }
            }

            if (new_level > stats_.current_load_level) {
                stats_.downscale_count++;
            } else {
                stats_.upscale_count++;
            }

            stats_.current_load_level = new_level;
            stats_.current_interval = local_config.get_interval_for_load(new_level);
            current_sampling_rate_ = local_config.get_sampling_rate_for_load(new_level);
            stats_.current_sampling_rate = current_sampling_rate_;
            stats_.total_adaptations++;
            stats_.last_adaptation = now;
            stats_.last_level_change = now;
        }
    }
    
    /**
     * @brief Get current adaptation statistics
     */
    adaptation_stats get_stats() const {
        std::lock_guard lock(stats_mutex_);
        return stats_;
    }
    
    /**
     * @brief Get current collection interval
     */
    std::chrono::milliseconds get_current_interval() const {
        std::lock_guard lock(stats_mutex_);
        return stats_.current_interval;
    }
    
    /**
     * @brief Set adaptive configuration
     * @thread_safety Thread-safe. Uses mutex for synchronization.
     */
    void set_config(const adaptive_config& config) {
        std::lock_guard<std::mutex> lock(config_mutex_);
        config_ = config;
    }

    /**
     * @brief Get adaptive configuration
     * @thread_safety Thread-safe. Uses mutex for synchronization.
     */
    adaptive_config get_config() const {
        std::lock_guard<std::mutex> lock(config_mutex_);
        return config_;
    }
    
    /**
     * @brief Enable or disable adaptive behavior
     */
    void set_enabled(bool enabled) {
        enabled_ = enabled;
    }
    
    /**
     * @brief Check if adaptive behavior is enabled
     */
    bool is_enabled() const {
        return enabled_;
    }
    
private:
    /**
     * @brief Determine if current sample should be collected
     */
    bool should_sample() const {
        if (!enabled_) return true;

        // Use random sampling based on current rate
        static thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<> dis(0.0, 1.0);
        return dis(gen) < current_sampling_rate_.load();
    }

    /**
     * @brief Calculate load level from metrics with provided config
     * @note Thread-safe as it only uses the provided config copy
     */
    static load_level calculate_load_level_with_config(
        double cpu_usage,
        double memory_usage,
        const adaptive_config& cfg
    ) {
        // Consider memory pressure in load calculation
        double effective_load = cpu_usage;

        // Memory pressure should escalate load level
        if (memory_usage > cfg.memory_critical_threshold) {
            // Critical memory -> at least high load
            effective_load = std::max(effective_load, cfg.high_threshold + 1.0);
        } else if (memory_usage > cfg.memory_warning_threshold) {
            // Warning memory -> at least moderate load
            effective_load = std::max(effective_load, cfg.moderate_threshold + 1.0);
        }

        // Apply strategy-specific adjustments BEFORE determining level
        switch (cfg.strategy) {
            case adaptation_strategy::conservative:
                effective_load *= 0.8;  // Be more conservative
                break;
            case adaptation_strategy::aggressive:
                effective_load *= 1.2;  // Be more aggressive
                break;
            case adaptation_strategy::balanced:
            default:
                break;
        }

        // Determine load level
        if (effective_load >= cfg.high_threshold) {
            return load_level::critical;
        } else if (effective_load >= cfg.moderate_threshold) {
            return load_level::high;
        } else if (effective_load >= cfg.low_threshold) {
            return load_level::moderate;
        } else if (effective_load >= cfg.idle_threshold) {
            return load_level::low;
        } else {
            return load_level::idle;
        }
    }

    /**
     * @brief Calculate load level with hysteresis support (ARC-005)
     *
     * Hysteresis prevents oscillation at threshold boundaries by requiring
     * the metric to cross a margin before changing levels.
     *
     * @param cpu_usage Current CPU usage percentage
     * @param memory_usage Current memory usage percentage
     * @param current_level Current load level
     * @param cfg Adaptive configuration
     * @return New load level (may be same as current if within hysteresis margin)
     */
    static load_level calculate_load_level_with_hysteresis(
        double cpu_usage,
        double memory_usage,
        load_level current_level,
        const adaptive_config& cfg
    ) {
        // First calculate what the level would be without hysteresis
        load_level raw_level = calculate_load_level_with_config(cpu_usage, memory_usage, cfg);

        // If hysteresis is disabled, return raw level
        if (!cfg.enable_hysteresis) {
            return raw_level;
        }

        // If no change, return current level
        if (raw_level == current_level) {
            return current_level;
        }

        // Calculate effective load (same logic as calculate_load_level_with_config)
        double effective_load = cpu_usage;
        if (memory_usage > cfg.memory_critical_threshold) {
            effective_load = std::max(effective_load, cfg.high_threshold + 1.0);
        } else if (memory_usage > cfg.memory_warning_threshold) {
            effective_load = std::max(effective_load, cfg.moderate_threshold + 1.0);
        }

        switch (cfg.strategy) {
            case adaptation_strategy::conservative:
                effective_load *= 0.8;
                break;
            case adaptation_strategy::aggressive:
                effective_load *= 1.2;
                break;
            case adaptation_strategy::balanced:
            default:
                break;
        }

        // Get threshold for current level
        double current_threshold = get_threshold_for_level(current_level, cfg);
        double margin = cfg.hysteresis_margin;

        // For upward transitions (higher load), require crossing threshold + margin
        // For downward transitions (lower load), require crossing threshold - margin
        if (raw_level > current_level) {
            // Moving to higher load level - need to exceed threshold by margin
            double next_threshold = get_threshold_for_level(
                static_cast<load_level>(static_cast<int>(current_level) + 1), cfg);
            if (effective_load < next_threshold + margin) {
                return current_level;  // Stay at current level (hysteresis)
            }
        } else {
            // Moving to lower load level - need to drop below threshold by margin
            if (effective_load > current_threshold - margin) {
                return current_level;  // Stay at current level (hysteresis)
            }
        }

        return raw_level;
    }

    /**
     * @brief Get the threshold value for a given load level
     */
    static double get_threshold_for_level(load_level level, const adaptive_config& cfg) {
        switch (level) {
            case load_level::idle: return 0.0;
            case load_level::low: return cfg.idle_threshold;
            case load_level::moderate: return cfg.low_threshold;
            case load_level::high: return cfg.moderate_threshold;
            case load_level::critical: return cfg.high_threshold;
        }
        return cfg.moderate_threshold;
    }
};

/**
 * @brief Adaptive monitoring controller
 */
class adaptive_monitor {
private:
    struct monitor_impl;
    std::unique_ptr<monitor_impl> impl_;
    
public:
    adaptive_monitor();
    ~adaptive_monitor();
    
    // Disable copy
    adaptive_monitor(const adaptive_monitor&) = delete;
    adaptive_monitor& operator=(const adaptive_monitor&) = delete;
    
    // Enable move
    adaptive_monitor(adaptive_monitor&&) noexcept;
    adaptive_monitor& operator=(adaptive_monitor&&) noexcept;
    
    /**
     * @brief Register a collector for adaptive monitoring
     */
    kcenon::monitoring::result<bool> register_collector(
        const std::string& name,
        std::shared_ptr<kcenon::monitoring::metrics_collector> collector,
        const adaptive_config& config = {}
    );
    
    /**
     * @brief Unregister a collector
     */
    kcenon::monitoring::result<bool> unregister_collector(const std::string& name);
    
    /**
     * @brief Start adaptive monitoring
     */
    kcenon::monitoring::result<bool> start();
    
    /**
     * @brief Stop adaptive monitoring
     */
    kcenon::monitoring::result<bool> stop();
    
    /**
     * @brief Check if monitoring is active
     */
    bool is_running() const;
    
    /**
     * @brief Get adaptation statistics for a collector
     */
    kcenon::monitoring::result<adaptation_stats> get_collector_stats(
        const std::string& name
    ) const;
    
    /**
     * @brief Get all collector statistics
     */
    std::unordered_map<std::string, adaptation_stats> get_all_stats() const;
    
    /**
     * @brief Set global adaptation strategy
     */
    void set_global_strategy(adaptation_strategy strategy);
    
    /**
     * @brief Force adaptation cycle
     */
    kcenon::monitoring::result<bool> force_adaptation();
    
    /**
     * @brief Get recommended collectors based on load
     */
    std::vector<std::string> get_active_collectors() const;
    
    /**
     * @brief Set priority for a collector (higher priority = keep active longer)
     */
    kcenon::monitoring::result<bool> set_collector_priority(
        const std::string& name,
        int priority
    );
};

/**
 * @brief Global adaptive monitor instance
 */
adaptive_monitor& global_adaptive_monitor();

/**
 * @brief Adaptive monitoring scope
 */
class adaptive_scope {
private:
    adaptive_monitor* monitor_;
    std::string collector_name_;
    bool registered_{false};
    
public:
    adaptive_scope(
        const std::string& name,
        std::shared_ptr<kcenon::monitoring::metrics_collector> collector,
        const adaptive_config& config = {}
    ) : monitor_(&global_adaptive_monitor()), collector_name_(name) {
        auto result = monitor_->register_collector(name, collector, config);
        registered_ = result.is_ok() && result.value();
    }
    
    ~adaptive_scope() {
        if (registered_ && monitor_) {
            monitor_->unregister_collector(collector_name_);
        }
    }
    
    // Disable copy
    adaptive_scope(const adaptive_scope&) = delete;
    adaptive_scope& operator=(const adaptive_scope&) = delete;
    
    // Enable move
    adaptive_scope(adaptive_scope&& other) noexcept
        : monitor_(other.monitor_)
        , collector_name_(std::move(other.collector_name_))
        , registered_(other.registered_) {
        other.monitor_ = nullptr;
        other.registered_ = false;
    }
    
    adaptive_scope& operator=(adaptive_scope&& other) noexcept {
        if (this != &other) {
            if (registered_ && monitor_) {
                monitor_->unregister_collector(collector_name_);
            }
            monitor_ = other.monitor_;
            collector_name_ = std::move(other.collector_name_);
            registered_ = other.registered_;
            other.monitor_ = nullptr;
            other.registered_ = false;
        }
        return *this;
    }
    
    bool is_registered() const { return registered_; }
};

} } // namespace kcenon::monitoring