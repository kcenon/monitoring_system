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
 * @file smart_collector.h
 * @brief S.M.A.R.T. disk health monitoring collector
 *
 * This file provides disk health monitoring using S.M.A.R.T.
 * (Self-Monitoring, Analysis and Reporting Technology) data:
 * - Cross-platform support via smartctl (smartmontools)
 * - Health status and predictive failure warnings
 * - Key SMART attributes collection
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

namespace kcenon {
namespace monitoring {

/**
 * SMART disk metrics structure containing per-disk health data
 */
struct smart_disk_metrics {
    // Disk identification
    std::string device_path;       ///< Device path (e.g., /dev/sda, /dev/disk0)
    std::string model_name;        ///< Disk model name
    std::string serial_number;     ///< Disk serial number
    std::string firmware_version;  ///< Firmware version

    // Health status
    bool smart_supported{false};  ///< Whether SMART is supported
    bool smart_enabled{false};    ///< Whether SMART is enabled
    bool health_ok{true};         ///< Overall health status (PASSED = true)

    // SMART attributes
    double temperature_celsius{0.0};   ///< Current temperature in Celsius
    uint64_t reallocated_sectors{0};   ///< Reallocated sector count
    uint64_t power_on_hours{0};        ///< Total power-on hours
    uint64_t power_cycle_count{0};     ///< Number of power cycles
    uint64_t pending_sectors{0};       ///< Sectors pending reallocation
    uint64_t uncorrectable_errors{0};  ///< Uncorrectable error count
    uint64_t read_error_rate{0};       ///< Read error rate (raw value)
    uint64_t write_error_rate{0};      ///< Write error rate (raw value)

    // Timestamp
    std::chrono::system_clock::time_point timestamp;
};

/**
 * Disk information structure for enumeration
 */
struct disk_info {
    std::string device_path;      ///< Device path
    std::string device_type;      ///< Device type (e.g., ata, nvme, scsi)
    bool smart_available{false};  ///< Whether SMART data might be available
};

/**
 * Platform-specific SMART data collector implementation
 */
class smart_info_collector {
   public:
    smart_info_collector();
    ~smart_info_collector();

    // Non-copyable, non-moveable due to internal state
    smart_info_collector(const smart_info_collector&) = delete;
    smart_info_collector& operator=(const smart_info_collector&) = delete;
    smart_info_collector(smart_info_collector&&) = delete;
    smart_info_collector& operator=(smart_info_collector&&) = delete;

    /**
     * Check if smartctl is available on the system
     * @return True if smartctl can be executed
     */
    bool is_smartctl_available() const;

    /**
     * Enumerate all disks that may have SMART data
     * @return Vector of disk info
     */
    std::vector<disk_info> enumerate_disks();

    /**
     * Collect SMART metrics for a specific disk
     * @param info Disk info
     * @return SMART disk metrics
     */
    smart_disk_metrics collect_smart_metrics(const disk_info& info);

   private:
    mutable std::mutex mutex_;
    mutable bool smartctl_checked_{false};
    mutable bool smartctl_available_{false};

    // Helper methods
    std::string execute_command(const std::string& command) const;
    smart_disk_metrics parse_smartctl_json(const std::string& json_output,
                                           const disk_info& info) const;
};

/**
 * SMART disk health collector implementing a standalone plugin interface
 *
 * Collects S.M.A.R.T. disk health data using smartctl (smartmontools).
 * Gracefully degrades when smartctl is not available or disks don't support SMART.
 */
class smart_collector {
   public:
    smart_collector();
    ~smart_collector() = default;

    // Non-copyable, non-moveable due to internal state
    smart_collector(const smart_collector&) = delete;
    smart_collector& operator=(const smart_collector&) = delete;
    smart_collector(smart_collector&&) = delete;
    smart_collector& operator=(smart_collector&&) = delete;

    /**
     * Initialize the collector with configuration
     * @param config Configuration options
     * @return true if initialization successful
     */
    bool initialize(const std::unordered_map<std::string, std::string>& config);

    /**
     * Collect SMART metrics from all disks
     * @return Vector of collected metrics
     */
    std::vector<metric> collect();

    /**
     * Get the name of this collector
     * @return Collector name
     */
    std::string get_name() const { return "smart_collector"; }

    /**
     * Get supported metric types
     * @return Vector of supported metric type names
     */
    std::vector<std::string> get_metric_types() const;

    /**
     * Check if the collector is healthy
     * @return true if collector is operational
     */
    bool is_healthy() const;

    /**
     * Get collector statistics
     * @return Map of statistic name to value
     */
    std::unordered_map<std::string, double> get_statistics() const;

    /**
     * Get last collected SMART metrics
     * @return Vector of SMART disk metrics
     */
    std::vector<smart_disk_metrics> get_last_metrics() const;

    /**
     * Check if SMART monitoring is available
     * @return True if smartctl is available
     */
    bool is_smart_available() const;

   private:
    std::unique_ptr<smart_info_collector> collector_;

    // Configuration
    bool enabled_{true};
    bool collect_temperature_{true};
    bool collect_error_rates_{true};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> collection_count_{0};
    std::atomic<size_t> collection_errors_{0};
    std::atomic<size_t> disks_found_{0};
    std::vector<smart_disk_metrics> last_metrics_;

    // Helper methods
    metric create_metric(const std::string& name, double value, const smart_disk_metrics& disk,
                         const std::string& unit = "") const;
    void add_disk_metrics(std::vector<metric>& metrics, const smart_disk_metrics& disk);
};

}  // namespace monitoring
}  // namespace kcenon
