// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file builtin_collectors.h
 * @brief Registration of built-in metric collectors with the registry
 *
 * This file provides a single function to register all built-in collectors
 * with the collector_registry. Call register_builtin_collectors() once at
 * application startup to enable runtime plugin management.
 *
 * Usage:
 * @code
 * #include <kcenon/monitoring/factory/builtin_collectors.h>
 *
 * int main() {
 *     kcenon::monitoring::register_builtin_collectors();
 *
 *     auto& registry = kcenon::monitoring::collector_registry::instance();
 *     auto* collector = registry.get_plugin("battery_collector");
 * }
 * @endcode
 */

#include "collector_adapters.h"
#include "../plugins/collector_registry.h"

// Include all collector headers
#include "../collectors/battery_collector.h"
#include "../collectors/interrupt_collector.h"
#include "../collectors/network_metrics_collector.h"
#include "../collectors/platform_metrics_collector.h"
#include "../collectors/process_metrics_collector.h"
#include "../collectors/security_collector.h"
#include "../collectors/smart_collector.h"
#include "../collectors/system_resource_collector.h"
#include "../collectors/uptime_collector.h"
#include "../collectors/vm_collector.h"

namespace kcenon::monitoring {

/**
 * @brief Register all built-in collectors with the collector_registry
 *
 * This function registers the following collectors:
 * - battery_collector (plugin-based)
 * - uptime_collector (plugin-based)
 * - interrupt_collector (plugin-based)
 * - network_metrics_collector (plugin-based)
 * - platform_metrics_collector (plugin-based)
 * - process_metrics_collector (plugin-based)
 * - security_collector (plugin-based)
 * - smart_collector (plugin-based)
 * - vm_collector (plugin-based)
 * - system_resource_collector (standalone, metric_factory only)
 *
 * Plugin-based collectors (using collector_plugin interface) are registered
 * with the collector_registry using factory-based lazy loading, enabling
 * runtime enable/disable and plugin management.
 *
 * All collectors are also registered with metric_factory for backward compatibility.
 *
 * Call this function once at application startup before using the registry.
 *
 * @return true if all collectors were registered successfully
 */
inline bool register_builtin_collectors() {
    bool all_success = true;
    auto& registry = collector_registry::instance();

    // Register plugin-based collectors with collector_registry for runtime plugin management
    // (Only collectors that implement collector_plugin interface)
    registry.register_factory<battery_collector>("battery_collector");
    registry.register_factory<uptime_collector>("uptime_collector");
    registry.register_factory<interrupt_collector>("interrupt_collector");
    registry.register_factory<network_metrics_collector>("network_metrics_collector");
    registry.register_factory<platform_metrics_collector>("platform_metrics_collector");
    registry.register_factory<process_metrics_collector>("process_metrics_collector");
    registry.register_factory<security_collector>("security_collector");
    registry.register_factory<smart_collector>("smart_collector");
    registry.register_factory<vm_collector>("vm_collector");

    // Also register all collectors with metric_factory for backward compatibility
    all_success &= register_plugin_collector<battery_collector>("battery_collector");
    all_success &= register_plugin_collector<uptime_collector>("uptime_collector");
    all_success &= register_plugin_collector<interrupt_collector>("interrupt_collector");
    all_success &= register_plugin_collector<network_metrics_collector>("network_metrics_collector");
    all_success &= register_plugin_collector<platform_metrics_collector>("platform_metrics_collector");
    all_success &= register_plugin_collector<process_metrics_collector>("process_metrics_collector");
    all_success &= register_plugin_collector<security_collector>("security_collector");
    all_success &= register_plugin_collector<smart_collector>("smart_collector");
    all_success &= register_plugin_collector<vm_collector>("vm_collector");
    all_success &= register_standalone_collector<system_resource_collector>("system_resource_collector");

    return all_success;
}

/**
 * @brief Get list of all built-in collector names
 * @return Vector of built-in collector names
 */
inline std::vector<std::string> get_builtin_collector_names() {
    return {"battery_collector",
            "uptime_collector",
            "interrupt_collector",
            "network_metrics_collector",
            "platform_metrics_collector",
            "process_metrics_collector",
            "security_collector",
            "smart_collector",
            "vm_collector",
            "system_resource_collector"};
}

}  // namespace kcenon::monitoring
