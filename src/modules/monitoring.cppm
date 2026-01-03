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
 * @file monitoring.cppm
 * @brief Primary module interface for kcenon.monitoring
 *
 * This is the main entry point for the monitoring_system C++20 module.
 * It exports all public interfaces from the three partitions:
 *
 * - kcenon.monitoring.core: Core types, interfaces, and utilities
 * - kcenon.monitoring.collectors: Metric collectors
 * - kcenon.monitoring.adaptive: Adaptive monitoring
 *
 * Usage:
 * @code
 * import kcenon.monitoring;
 *
 * // Use core types
 * kcenon::monitoring::metric_sample sample("cpu_usage", 75.5);
 *
 * // Use collectors
 * auto& registry = kcenon::monitoring::collector_registry::instance();
 * auto metrics = registry.collect_all();
 *
 * // Use adaptive monitoring
 * auto& controller = kcenon::monitoring::global_adaptive_controller();
 * controller.adapt_all(load_metrics);
 * @endcode
 *
 * Dependencies:
 * - kcenon.common (Tier 0) - Required
 * - kcenon.thread (Tier 1) - Required
 * - kcenon.logger (Tier 2) - Optional
 *
 * @note This module is part of the C++20 module migration effort.
 * Header-based builds are still supported during the transition period.
 *
 * @see https://github.com/kcenon/monitoring_system/issues/310
 * @see https://github.com/kcenon/common_system/issues/256
 */
export module kcenon.monitoring;

// Export all partition modules
export import kcenon.monitoring.core;
export import kcenon.monitoring.collectors;
export import kcenon.monitoring.adaptive;

// ============================================================================
// Module-Level Documentation
// ============================================================================

/**
 * @mainpage kcenon.monitoring Module
 *
 * @section intro Introduction
 *
 * The kcenon.monitoring module provides comprehensive system monitoring
 * capabilities for C++ applications. It is designed as a Tier 3 library
 * in the kcenon ecosystem.
 *
 * @section architecture Architecture
 *
 * The module is organized into three partitions:
 *
 * @subsection core Core Partition (kcenon.monitoring.core)
 *
 * Contains fundamental types and interfaces:
 * - Error codes and result types
 * - Metric types and samples
 * - Event types and bus
 * - Thread-local buffers
 * - C++20 concepts
 *
 * @subsection collectors Collectors Partition (kcenon.monitoring.collectors)
 *
 * Contains metric collection implementations:
 * - System resource collectors (CPU, memory, disk, network)
 * - Hardware collectors (battery, temperature, GPU)
 * - Network state collectors (TCP, socket buffers)
 * - Container and VM collectors
 * - Plugin collector interface
 *
 * @subsection adaptive Adaptive Partition (kcenon.monitoring.adaptive)
 *
 * Contains adaptive monitoring features:
 * - Load-based adaptation
 * - Sampling rate adjustment
 * - Alert rules and notifications
 * - Hysteresis and cooldown
 *
 * @section usage Basic Usage
 *
 * @code
 * import kcenon.monitoring;
 *
 * using namespace kcenon::monitoring;
 *
 * // Create a metric sample
 * metric_sample cpu("cpu.usage.percent", 45.5);
 * cpu.labels["host"] = "server01";
 *
 * // Use the collector registry
 * auto& registry = collector_registry::instance();
 * auto metrics = registry.collect_all();
 *
 * // Configure adaptive monitoring
 * adaptive_config config;
 * config.strategy = adaptation_strategy::balanced;
 *
 * auto& controller = global_adaptive_controller();
 * controller.set_global_strategy(adaptation_strategy::balanced);
 *
 * // Set up alerts
 * alert_rule rule("high_cpu", "cpu.usage.percent",
 *                 comparison_operator::greater_than, 80.0,
 *                 alert_severity::warning);
 * global_alert_manager().add_rule(rule);
 * @endcode
 *
 * @section dependencies Dependencies
 *
 * | Module | Tier | Required | Purpose |
 * |--------|------|----------|---------|
 * | kcenon.common | 0 | Yes | Common types and patterns |
 * | kcenon.thread | 1 | Yes | Threading utilities |
 * | kcenon.logger | 2 | No | Logging (runtime binding) |
 *
 * @section migration Migration from Headers
 *
 * To migrate from header-based usage to modules:
 *
 * @code
 * // Before (header-based)
 * #include <kcenon/monitoring/core/metric_types.h>
 * #include <kcenon/monitoring/collectors/system_resource_collector.h>
 * #include <kcenon/monitoring/adaptive/adaptive_monitor.h>
 *
 * // After (module-based)
 * import kcenon.monitoring;
 * @endcode
 *
 * @section thread_safety Thread Safety
 *
 * All exported classes are thread-safe unless explicitly noted otherwise.
 * The module uses:
 * - std::mutex and std::shared_mutex for synchronization
 * - std::atomic for lock-free counters
 * - Thread-local storage for per-thread buffers
 *
 * @section version Version
 *
 * This module corresponds to monitoring_system version 0.1.0.
 *
 * @see https://github.com/kcenon/monitoring_system
 */

// ============================================================================
// Version Information
// ============================================================================

export namespace kcenon::monitoring {

/**
 * @brief Module version information
 */
struct module_version {
    static constexpr int major = 0;
    static constexpr int minor = 1;
    static constexpr int patch = 0;
    static constexpr const char* string = "0.1.0";
    static constexpr const char* name = "kcenon.monitoring";
};

/**
 * @brief Get the module version string
 * @return Version string (e.g., "0.1.0")
 */
constexpr const char* get_module_version() noexcept {
    return module_version::string;
}

/**
 * @brief Get the module name
 * @return Module name
 */
constexpr const char* get_module_name() noexcept {
    return module_version::name;
}

} // namespace kcenon::monitoring
