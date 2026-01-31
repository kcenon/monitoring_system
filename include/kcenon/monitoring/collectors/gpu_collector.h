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
 * @file gpu_collector.h
 * @brief GPU metrics monitoring collector
 *
 * This file provides GPU metrics monitoring using platform-specific
 * APIs to gather GPU utilization, memory, temperature, and power data:
 * - Linux: sysfs (/sys/class/drm/) for NVIDIA, AMD, and Intel GPUs
 * - macOS: IOKit for GPU enumeration, SMC for temperature
 * - Windows: Stub implementation (future: DirectX/WMI)
 */

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../interfaces/metric_types_adapter.h"
#include "../plugins/collector_plugin.h"

namespace kcenon {
namespace monitoring {

/**
 * @enum gpu_vendor
 * @brief GPU vendor identification
 */
enum class gpu_vendor {
    unknown,  ///< Unknown vendor
    nvidia,   ///< NVIDIA Corporation
    amd,      ///< Advanced Micro Devices
    intel,    ///< Intel Corporation
    apple,    ///< Apple (Apple Silicon GPU)
    other     ///< Other vendor
};

/**
 * @brief Convert gpu_vendor to string representation
 */
inline std::string gpu_vendor_to_string(gpu_vendor vendor) {
    switch (vendor) {
        case gpu_vendor::nvidia:
            return "nvidia";
        case gpu_vendor::amd:
            return "amd";
        case gpu_vendor::intel:
            return "intel";
        case gpu_vendor::apple:
            return "apple";
        case gpu_vendor::other:
            return "other";
        default:
            return "unknown";
    }
}

/**
 * @enum gpu_type
 * @brief GPU type classification
 */
enum class gpu_type {
    unknown,     ///< Unknown GPU type
    discrete,    ///< Discrete GPU (dedicated graphics card)
    integrated,  ///< Integrated GPU (part of CPU/SoC)
    virtual_gpu  ///< Virtual GPU (cloud/VM)
};

/**
 * @brief Convert gpu_type to string representation
 */
inline std::string gpu_type_to_string(gpu_type type) {
    switch (type) {
        case gpu_type::discrete:
            return "discrete";
        case gpu_type::integrated:
            return "integrated";
        case gpu_type::virtual_gpu:
            return "virtual";
        default:
            return "unknown";
    }
}

/**
 * @struct gpu_device_info
 * @brief Information about a GPU device
 */
struct gpu_device_info {
    std::string id;              ///< Unique device identifier (e.g., "gpu0")
    std::string name;            ///< Human-readable device name
    std::string device_path;     ///< Platform-specific path (e.g., /sys/class/drm/card0)
    std::string driver_version;  ///< Driver version string
    gpu_vendor vendor{gpu_vendor::unknown};  ///< GPU vendor
    gpu_type type{gpu_type::unknown};        ///< GPU type (discrete/integrated)
    uint32_t device_index{0};                ///< Device index for multi-GPU systems
};

/**
 * @struct gpu_reading
 * @brief A single GPU metrics reading
 */
struct gpu_reading {
    gpu_device_info device;  ///< GPU device information

    // Utilization metrics
    double utilization_percent{0.0};  ///< GPU compute utilization (0-100)

    // Memory metrics
    uint64_t memory_used_bytes{0};   ///< VRAM currently used
    uint64_t memory_total_bytes{0};  ///< Total VRAM capacity

    // Thermal metrics
    double temperature_celsius{0.0};  ///< GPU temperature

    // Power metrics
    double power_watts{0.0};        ///< Current power consumption
    double power_limit_watts{0.0};  ///< Power limit/TDP

    // Clock metrics
    double clock_mhz{0.0};         ///< Current GPU clock speed
    double memory_clock_mhz{0.0};  ///< Current memory clock speed

    // Fan metrics
    double fan_speed_percent{0.0};  ///< Fan speed (0-100)

    // Availability flags
    bool utilization_available{false};   ///< Whether utilization metrics available
    bool memory_available{false};        ///< Whether memory metrics available
    bool temperature_available{false};   ///< Whether temperature metrics available
    bool power_available{false};         ///< Whether power metrics available
    bool clock_available{false};         ///< Whether clock metrics available
    bool fan_available{false};           ///< Whether fan metrics available

    std::chrono::system_clock::time_point timestamp;  ///< Reading timestamp
};

// Forward declaration
namespace platform {
class metrics_provider;
}  // namespace platform

/**
 * @class gpu_info_collector
 * @brief GPU data collector using platform abstraction layer
 *
 * This class provides GPU data collection using the unified
 * metrics_provider interface, eliminating platform-specific code.
 */
class gpu_info_collector {
   public:
    gpu_info_collector();
    ~gpu_info_collector();

    // Non-copyable, non-moveable due to internal state
    gpu_info_collector(const gpu_info_collector&) = delete;
    gpu_info_collector& operator=(const gpu_info_collector&) = delete;
    gpu_info_collector(gpu_info_collector&&) = delete;
    gpu_info_collector& operator=(gpu_info_collector&&) = delete;

    /**
     * Check if GPU monitoring is available on this system
     * @return True if GPUs can be enumerated and read
     */
    bool is_gpu_available() const;

    /**
     * Enumerate all available GPUs
     * @return Vector of GPU device information
     */
    std::vector<gpu_device_info> enumerate_gpus();

    /**
     * Read metrics from all available GPUs
     * @return Vector of GPU readings
     */
    std::vector<gpu_reading> read_all_gpu_metrics();

   private:
    std::unique_ptr<platform::metrics_provider> provider_;
};

/**
 * @class gpu_collector
 * @brief GPU metrics monitoring collector implementing collector_plugin interface
 *
 * Collects GPU metrics data from available GPUs with cross-platform
 * support. Gracefully degrades when GPUs are not available or when
 * vendor-specific libraries are not installed.
 */
class gpu_collector : public collector_plugin {
   public:
    gpu_collector();
    ~gpu_collector() override = default;

    // Non-copyable, non-moveable due to internal state
    gpu_collector(const gpu_collector&) = delete;
    gpu_collector& operator=(const gpu_collector&) = delete;
    gpu_collector(gpu_collector&&) = delete;
    gpu_collector& operator=(gpu_collector&&) = delete;

    // collector_plugin implementation
    auto name() const -> std::string_view override { return "gpu"; }
    auto collect() -> std::vector<metric> override;
    auto interval() const -> std::chrono::milliseconds override { return std::chrono::seconds(5); }
    auto is_available() const -> bool override;
    auto get_metric_types() const -> std::vector<std::string> override;

    auto get_metadata() const -> plugin_metadata override {
        return plugin_metadata{
            .name = name(),
            .description = "GPU metrics (utilization, memory, temperature, power)",
            .category = plugin_category::hardware,
            .version = "1.0.0",
            .dependencies = {},
            .requires_platform_support = true
        };
    }

    auto initialize(const config_map& config) -> bool override;
    void shutdown() override {}
    auto get_statistics() const -> stats_map override;

    // Legacy compatibility (deprecated)
    bool is_healthy() const;

    /**
     * Get last collected GPU readings
     * @return Vector of GPU readings
     */
    std::vector<gpu_reading> get_last_readings() const;

    /**
     * Check if GPU monitoring is available
     * @return True if GPUs are accessible
     */
    bool is_gpu_available() const;

   private:
    std::unique_ptr<gpu_info_collector> collector_;

    // Configuration
    bool enabled_{true};
    bool collect_utilization_{true};
    bool collect_memory_{true};
    bool collect_temperature_{true};
    bool collect_power_{true};
    bool collect_clock_{true};
    bool collect_fan_{true};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> collection_count_{0};
    std::atomic<size_t> collection_errors_{0};
    std::atomic<size_t> gpus_found_{0};
    std::vector<gpu_reading> last_readings_;

    // Helper methods
    metric create_metric(const std::string& name, double value, const gpu_reading& reading,
                         const std::string& unit = "") const;
    void add_gpu_metrics(std::vector<metric>& metrics, const gpu_reading& reading);
};

}  // namespace monitoring
}  // namespace kcenon
