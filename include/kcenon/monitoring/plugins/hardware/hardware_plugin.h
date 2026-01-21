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

#pragma once

/**
 * @file hardware_plugin.h
 * @brief Hardware monitoring plugin for battery, power, temperature, and GPU metrics
 *
 * This plugin extracts hardware-specific collectors from the core library,
 * making them optional for server environments where hardware monitoring
 * is typically unnecessary.
 *
 * Usage:
 * @code
 * #include <kcenon/monitoring/plugins/hardware/hardware_plugin.h>
 *
 * // Create plugin with default configuration
 * auto plugin = hardware_plugin::create();
 *
 * // Or with custom configuration
 * hardware_plugin_config config;
 * config.enable_battery = true;
 * config.enable_temperature = true;
 * config.enable_gpu = false;
 * auto plugin = hardware_plugin::create(config);
 *
 * // Register with plugin_metric_collector
 * collector.register_plugin(std::move(plugin));
 * @endcode
 */

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../collectors/plugin_metric_collector.h"

namespace kcenon {
namespace monitoring {

// Forward declarations for internal collectors
class battery_collector;
class power_collector;
class temperature_collector;
class gpu_collector;

namespace plugins {

/**
 * @struct hardware_plugin_config
 * @brief Configuration options for the hardware monitoring plugin
 */
struct hardware_plugin_config {
    /// Enable battery monitoring (default: true)
    bool enable_battery = true;

    /// Enable power consumption monitoring (default: true)
    bool enable_power = true;

    /// Enable temperature monitoring (default: true)
    bool enable_temperature = true;

    /// Enable GPU monitoring (default: false, requires GPU hardware)
    bool enable_gpu = false;

    /// Battery-specific options
    bool battery_collect_health = true;
    bool battery_collect_thermal = true;

    /// Power-specific options
    bool power_collect_battery = true;
    bool power_collect_rapl = true;

    /// Temperature-specific options
    bool temperature_collect_thresholds = true;
    bool temperature_collect_warnings = true;

    /// GPU-specific options
    bool gpu_collect_utilization = true;
    bool gpu_collect_memory = true;
    bool gpu_collect_temperature = true;
    bool gpu_collect_power = true;
    bool gpu_collect_clock = true;
    bool gpu_collect_fan = true;
};

/**
 * @class hardware_plugin
 * @brief Hardware monitoring plugin aggregating battery, power, temperature, and GPU collectors
 *
 * This plugin provides hardware-specific metrics collection for desktop/laptop environments.
 * For server deployments, this plugin should not be loaded to reduce binary size and
 * avoid unnecessary collection overhead.
 *
 * Metrics provided:
 * - Battery: level, charging status, health, capacity, cycles
 * - Power: consumption (watts), voltage, current, RAPL domains
 * - Temperature: CPU, GPU, motherboard, ambient temperatures
 * - GPU: utilization, VRAM usage, temperature, power, clocks, fan speed
 */
class hardware_plugin : public metric_collector_plugin {
   public:
    /**
     * Create a hardware plugin instance with configuration
     * @param config Plugin configuration options
     * @return Unique pointer to hardware_plugin instance
     */
    static std::unique_ptr<hardware_plugin> create(const hardware_plugin_config& config = {});

    ~hardware_plugin() override;

    // Disable copy
    hardware_plugin(const hardware_plugin&) = delete;
    hardware_plugin& operator=(const hardware_plugin&) = delete;

    // metric_collector_plugin interface
    bool initialize(const std::unordered_map<std::string, std::string>& config) override;
    std::vector<metric> collect() override;
    std::string get_name() const override;
    std::vector<std::string> get_metric_types() const override;
    bool is_healthy() const override;
    std::unordered_map<std::string, double> get_statistics() const override;

    /**
     * Check if battery monitoring is available
     * @return True if battery hardware detected
     */
    bool is_battery_available() const;

    /**
     * Check if power monitoring is available
     * @return True if power sensors detected
     */
    bool is_power_available() const;

    /**
     * Check if temperature monitoring is available
     * @return True if thermal sensors detected
     */
    bool is_temperature_available() const;

    /**
     * Check if GPU monitoring is available
     * @return True if GPU hardware detected
     */
    bool is_gpu_available() const;

    /**
     * Get the current configuration
     * @return Copy of current configuration
     */
    hardware_plugin_config get_config() const;

   private:
    explicit hardware_plugin(const hardware_plugin_config& config);

    // Internal collectors
    std::unique_ptr<battery_collector> battery_collector_;
    std::unique_ptr<power_collector> power_collector_;
    std::unique_ptr<temperature_collector> temperature_collector_;
    std::unique_ptr<gpu_collector> gpu_collector_;

    // Configuration
    hardware_plugin_config config_;
    bool initialized_{false};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> total_collections_{0};
    std::atomic<size_t> collection_errors_{0};

    // Helper methods
    void initialize_collectors();
};

}  // namespace plugins
}  // namespace monitoring
}  // namespace kcenon
