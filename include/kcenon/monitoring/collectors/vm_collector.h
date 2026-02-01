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
 * @file vm_collector.h
 * @brief Virtualization metrics collector
 *
 * This file provides virtualization monitoring, detecting if the system
 * is running as a guest in a virtual environment (KVM, Hyper-V, VMware, etc.)
 * and collecting relevant metrics like steal time.
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
 * @enum vm_type
 * @brief Detected virtualization platform
 */
enum class vm_type {
    none = 0,       ///< Bare metal (or undetected)
    kvm = 1,        ///< KVM / QEMU
    hyperv = 2,     ///< Microsoft Hyper-V
    vmware = 3,     ///< VMware
    virtualbox = 4, ///< Oracle VirtualBox
    xen = 5,        ///< Xen
    docker = 6,     ///< Docker Container (if distinguishable)
    other = 7       ///< Other detected virtualization
};

/**
 * @brief Convert vm_type to string representation
 * @param type The VM type to convert
 * @return String name of the VM type
 */
inline std::string vm_type_to_string(vm_type type) {
    switch (type) {
        case vm_type::none: return "NONE";
        case vm_type::kvm: return "KVM";
        case vm_type::hyperv: return "HYPER-V";
        case vm_type::vmware: return "VMWARE";
        case vm_type::virtualbox: return "VIRTUALBOX";
        case vm_type::xen: return "XEN";
        case vm_type::docker: return "DOCKER";
        case vm_type::other: return "OTHER";
        default: return "UNKNOWN";
    }
}

/**
 * @struct vm_metrics
 * @brief Virtualization specific metrics
 */
struct vm_metrics {
    bool is_virtualized{false};      ///< True if running in a VM
    vm_type type{vm_type::none};     ///< Detected hypervisor type
    double guest_cpu_steal_time{0.0}; ///< % CPU time stolen by hypervisor (if available)
    std::string hypervisor_vendor;   ///< Vendor string (e.g., "KVMKVMKVM" or "Microsoft Hv")
    
    // Additional possibilities:
    // - host_physical_cpu_count (if exposed)
    // - ballooned_memory (if relevant)
};

/**
 * @class vm_info_collector
 * @brief Platform-specific virtualization data collector implementation
 */
class vm_info_collector {
   public:
    vm_info_collector();
    ~vm_info_collector();

    // Mac-specific / Platform-specific detection
    vm_metrics collect_metrics();

   private:
    // Caching static info since VM type doesn't change at runtime usually
    bool info_cached_{false};
    vm_metrics cached_metrics_;
    
    void detect_vm_environment();
    double get_steal_time();
};

/**
 * @class vm_collector
 * @brief Virtualization metrics monitoring collector
 *
 * Collects metrics regarding the virtualization environment.
 */
class vm_collector : public collector_plugin {
   public:
    vm_collector();
    ~vm_collector() = default;

    // Non-copyable, non-moveable
    vm_collector(const vm_collector&) = delete;
    vm_collector& operator=(const vm_collector&) = delete;
    vm_collector(vm_collector&&) = delete;
    vm_collector& operator=(vm_collector&&) = delete;

    // collector_plugin interface implementation
    auto name() const -> std::string_view override { return "vm_collector"; }
    auto collect() -> std::vector<metric> override;
    auto interval() const -> std::chrono::milliseconds override { return collection_interval_; }
    auto is_available() const -> bool override;
    /**
     * Check if collector is in a healthy state
     * @return True if collector is operational
     */
    bool is_healthy() const;
    auto get_metric_types() const -> std::vector<std::string> override;

    /**
     * Initialize the collector with configuration
     * @param config Configuration options
     * @return true if initialization successful
     */
    bool initialize(const std::unordered_map<std::string, std::string>& config) override;


    /**
     * Get collector statistics
     * @return Map of statistic name to value
     */
    std::unordered_map<std::string, double> get_statistics() const override;

   private:
    std::unique_ptr<vm_info_collector> collector_;

    // Configuration
    bool enabled_{true};
    std::chrono::milliseconds collection_interval_{std::chrono::seconds(30)};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> collection_count_{0};
    std::atomic<size_t> collection_errors_{0};

    // Helper methods
    metric create_metric(const std::string& name, double value,
                         const std::unordered_map<std::string, std::string>& tags = {},
                         const std::string& unit = "") const;
};

}  // namespace monitoring
}  // namespace kcenon
