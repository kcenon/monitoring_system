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
 * @file builtin_collectors.h
 * @brief Registration of built-in metric collectors with the factory
 *
 * This file provides a single function to register all built-in collectors
 * with the metric_factory. Call register_builtin_collectors() once at
 * application startup to enable factory-based collector creation.
 *
 * Usage:
 * @code
 * #include <kcenon/monitoring/factory/builtin_collectors.h>
 *
 * int main() {
 *     kcenon::monitoring::register_builtin_collectors();
 *
 *     auto& factory = kcenon::monitoring::metric_factory::instance();
 *     auto collector = factory.create("system_resource_collector", {});
 * }
 * @endcode
 */

#include "collector_adapters.h"

// Include all collector headers
#include "../collectors/battery_collector.h"
#include "../collectors/interrupt_collector.h"
#include "../collectors/network_metrics_collector.h"
#include "../collectors/platform_metrics_collector.h"
#include "../collectors/process_metrics_collector.h"
#include "../collectors/system_resource_collector.h"
#include "../collectors/uptime_collector.h"
#include "../collectors/vm_collector.h"

namespace kcenon::monitoring {

/**
 * @brief Register all built-in collectors with the metric_factory
 *
 * This function registers the following collectors:
 * - system_resource_collector (plugin-based)
 * - vm_collector (standalone)
 * - uptime_collector (CRTP-based)
 * - battery_collector (CRTP-based)
 * - process_metrics_collector (CRTP-based, consolidated from fd, inode, context_switch)
 * - network_metrics_collector (CRTP-based, consolidated from tcp_state and socket_buffer)
 * - platform_metrics_collector (CRTP-based, unified platform metrics using Strategy pattern)
 * - interrupt_collector (CRTP-based)
 *
 * Call this function once at application startup before using the factory.
 *
 * @return true if all collectors were registered successfully
 */
inline bool register_builtin_collectors() {
    bool all_success = true;

    // Plugin-based collectors
    all_success &= register_plugin_collector<system_resource_collector>("system_resource_collector");

    // Standalone collectors
    all_success &= register_standalone_collector<vm_collector>("vm_collector");

    // CRTP-based collectors
    all_success &= register_crtp_collector<uptime_collector>("uptime_collector");
    all_success &= register_crtp_collector<battery_collector>("battery_collector");
    all_success &= register_crtp_collector<process_metrics_collector>("process_metrics_collector");
    all_success &= register_crtp_collector<network_metrics_collector>("network_metrics_collector");
    all_success &= register_crtp_collector<platform_metrics_collector>("platform_metrics_collector");
    all_success &= register_crtp_collector<interrupt_collector>("interrupt_collector");

    return all_success;
}

/**
 * @brief Get list of all built-in collector names
 * @return Vector of built-in collector names
 */
inline std::vector<std::string> get_builtin_collector_names() {
    return {"system_resource_collector",
            "vm_collector",
            "uptime_collector",
            "battery_collector",
            "process_metrics_collector",
            "network_metrics_collector",
            "platform_metrics_collector",
            "interrupt_collector"};
}

}  // namespace kcenon::monitoring
