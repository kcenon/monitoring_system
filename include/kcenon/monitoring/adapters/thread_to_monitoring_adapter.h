#pragma once

// Define guard so compatibility shim does not try to include a non‑existent path
#ifndef KCENON_MONITORING_ADAPTERS_THREAD_TO_MONITORING_ADAPTER_H
#define KCENON_MONITORING_ADAPTERS_THREAD_TO_MONITORING_ADAPTER_H

/*
 * BSD 3-Clause License
 * Copyright (c) 2025, monitoring_system contributors
 */

// Module description:
// Thread system adapter for monitoring_system. Provides a thin integration
// layer that can (optionally) pull metrics from thread_system and/or publish
// them to the monitoring event bus. When thread_system is not available, the
// adapter degrades gracefully and returns empty metric sets.

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "../core/event_bus.h"
#include "../core/event_types.h"
#include "../core/result_types.h"
#include "../interfaces/metric_types_adapter.h"

// Optional thread_system integration
// Note: thread_system v3.0+ uses common_system interfaces instead of
// thread-specific monitorable_interface.h. Detection now uses umbrella headers.
#if !defined(MONITORING_THREAD_SYSTEM_AVAILABLE)
#  if defined(MONITORING_HAS_THREAD_SYSTEM)
#    define MONITORING_THREAD_SYSTEM_AVAILABLE 1
#  elif __has_include(<kcenon/thread/thread_pool.h>)
#    define MONITORING_THREAD_SYSTEM_AVAILABLE 1
#  else
#    define MONITORING_THREAD_SYSTEM_AVAILABLE 0
#  endif
#endif

// thread_system v3.0+ uses common_system interfaces
#if MONITORING_THREAD_SYSTEM_AVAILABLE
#  include <kcenon/thread/interfaces/service_container.h>
#  if __has_include(<kcenon/common/interfaces/monitoring_interface.h>)
#    include <kcenon/common/interfaces/monitoring_interface.h>
#    define MONITORING_HAS_COMMON_INTERFACES 1
#  endif
#endif

namespace kcenon { namespace monitoring {

class thread_to_monitoring_adapter {
public:
    struct collection_config {
        std::chrono::milliseconds interval{std::chrono::milliseconds{1000}};
        bool publish_events{true};
    };

    explicit thread_to_monitoring_adapter(std::shared_ptr<event_bus> bus)
        : bus_(std::move(bus)) {}

    // Returns true when thread_system headers are available and a monitorable
    // provider can be discovered at runtime (best‑effort).
    bool is_thread_system_available() const {
#if MONITORING_THREAD_SYSTEM_AVAILABLE
        // thread_system v3.0+ uses common_system interfaces
#  if defined(MONITORING_HAS_COMMON_INTERFACES)
        // Dynamic discovery via service_container if present
        try {
            auto& container = kcenon::thread::service_container::global();
            auto monitorable = container.resolve<kcenon::common::interfaces::IMonitorable>();
            return static_cast<bool>(monitorable);
        } catch (...) {
            return true; // Headers present; treat as available even if not registered
        }
#  else
        return true; // thread_system available but no common interfaces
#  endif
#else
        return false;
#endif
    }

    // One‑shot collection. When thread_system is not available, returns empty.
    common::Result<std::vector<metric>> collect_metrics() {
        std::vector<metric> out;

#if MONITORING_THREAD_SYSTEM_AVAILABLE && defined(MONITORING_HAS_COMMON_INTERFACES)
        try {
            auto& container = kcenon::thread::service_container::global();
            auto monitorable = container.resolve<kcenon::common::interfaces::IMonitorable>();
            if (monitorable) {
                // Convert minimal subset of thread_system metrics into adapter metrics
                // thread_system v3.0 uses common::interfaces::IMonitorable
                auto monitoring_result = monitorable->get_monitoring_data();
                if (!monitoring_result.is_err()) {
                    const auto& snap = monitoring_result.value();
                    // Map metrics from common_system snapshot
                    for (const auto& m : snap.metrics) {
                        metric adapted{m.name, m.value, {}};
                        out.emplace_back(std::move(adapted));
                    }
                }
            }
        } catch (...) {
            // Fall back to empty on any failure
        }
#endif

        return common::ok(std::move(out));
    }

    // Supported metric names. Empty in fallback mode to satisfy tests.
    std::vector<std::string> get_metric_types() const {
#if MONITORING_THREAD_SYSTEM_AVAILABLE
        return {
            "thread.pool.jobs_pending",
            "thread.pool.jobs_completed",
            "thread.pool.worker_threads"
        };
#else
        return {};
#endif
    }

    // Start periodic collection (best‑effort). When publish_events is true,
    // collected metrics are emitted via metric_collection_event.
    common::VoidResult start_collection(const collection_config& cfg) {
        if (running_.exchange(true)) {
            return common::ok(); // already running
        }

        if (!bus_) {
            running_ = false;
            return common::VoidResult::err(static_cast<int>(monitoring_error_code::operation_failed), "event_bus not set");
        }

        worker_ = std::thread([this, cfg]() {
            while (running_.load()) {
                auto res = collect_metrics();
                if (res.is_ok() && cfg.publish_events && !res.value().empty()) {
                    bus_->publish_event(metric_collection_event("thread_system_adapter", res.value()));
                }
                std::this_thread::sleep_for(cfg.interval);
            }
        });

        return common::ok();
    }

    common::VoidResult stop_collection() {
        if (!running_.exchange(false)) {
            return common::ok();
        }
        if (worker_.joinable()) {
            worker_.join();
        }
        return common::ok();
    }

    ~thread_to_monitoring_adapter() { (void)stop_collection(); }

private:
    std::shared_ptr<event_bus> bus_;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

} } // namespace kcenon::monitoring

#endif // KCENON_MONITORING_ADAPTERS_THREAD_TO_MONITORING_ADAPTER_H
