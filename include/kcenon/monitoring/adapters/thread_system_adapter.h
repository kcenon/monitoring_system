#pragma once

// Define guard so compatibility shim does not try to include a non‑existent path
#ifndef KCENON_MONITORING_ADAPTERS_THREAD_SYSTEM_ADAPTER_H
#define KCENON_MONITORING_ADAPTERS_THREAD_SYSTEM_ADAPTER_H

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
#ifdef USE_THREAD_SYSTEM
#  include <kcenon/thread/interfaces/monitorable_interface.h>
#  include <kcenon/thread/interfaces/service_container.h>
#endif

namespace monitoring_system {

class thread_system_adapter {
public:
    struct collection_config {
        std::chrono::milliseconds interval{std::chrono::milliseconds{1000}};
        bool publish_events{true};
    };

    explicit thread_system_adapter(std::shared_ptr<event_bus> bus)
        : bus_(std::move(bus)) {}

    // Returns true when thread_system headers are available and a monitorable
    // provider can be discovered at runtime (best‑effort).
    bool is_thread_system_available() const {
#ifdef USE_THREAD_SYSTEM
        // Dynamic discovery via service_container if present
        try {
            auto& container = kcenon::thread::service_container::global();
            auto monitorable = container.resolve<kcenon::thread::monitorable_interface>();
            return static_cast<bool>(monitorable);
        } catch (...) {
            return true; // Headers present; treat as available even if not registered
        }
#else
        return false;
#endif
    }

    // One‑shot collection. When thread_system is not available, returns empty.
    result<std::vector<metric>> collect_metrics() {
        std::vector<metric> out;

#ifdef USE_THREAD_SYSTEM
        try {
            auto& container = kcenon::thread::service_container::global();
            auto monitorable = container.resolve<kcenon::thread::monitorable_interface>();
            if (monitorable) {
                // Convert minimal subset of thread_system metrics into adapter metrics
                auto snap = monitorable->get_metrics();
                // Map a few well‑known fields when present; best‑effort conversion.
                // Note: The interface in thread_system exposes a metrics_snapshot with
                // system/thread_pool/worker fields. We convert selected numeric values.
                metric m_jobs_pending{"thread.pool.jobs_pending", 0.0, {}};
                metric m_jobs_completed{"thread.pool.jobs_completed", 0.0, {}};
                metric m_workers{"thread.pool.worker_threads", 0.0, {}};

                // Values default to 0.0 if missing — safe for aggregation
                m_jobs_pending.value = static_cast<int64_t>(snap.thread_pool.jobs_pending);
                m_jobs_completed.value = static_cast<int64_t>(snap.thread_pool.jobs_completed);
                m_workers.value = static_cast<int64_t>(snap.thread_pool.worker_threads);

                out.emplace_back(std::move(m_jobs_pending));
                out.emplace_back(std::move(m_jobs_completed));
                out.emplace_back(std::move(m_workers));
            }
        } catch (...) {
            // Fall back to empty on any failure
        }
#endif

        return make_success(std::move(out));
    }

    // Supported metric names. Empty in fallback mode to satisfy tests.
    std::vector<std::string> get_metric_types() const {
#ifdef USE_THREAD_SYSTEM
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
    result_void start_collection(const collection_config& cfg) {
        if (running_.exchange(true)) {
            return result_void::success(); // already running
        }

        if (!bus_) {
            running_ = false;
            return result_void(monitoring_error_code::operation_failed, "event_bus not set");
        }

        worker_ = std::thread([this, cfg]() {
            while (running_.load()) {
                auto res = collect_metrics();
                if (res && cfg.publish_events && !res.value().empty()) {
                    bus_->publish_event(metric_collection_event("thread_system_adapter", res.value()));
                }
                std::this_thread::sleep_for(cfg.interval);
            }
        });

        return result_void::success();
    }

    result_void stop_collection() {
        if (!running_.exchange(false)) {
            return result_void::success();
        }
        if (worker_.joinable()) {
            worker_.join();
        }
        return result_void::success();
    }

    ~thread_system_adapter() { (void)stop_collection(); }

private:
    std::shared_ptr<event_bus> bus_;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

} // namespace monitoring_system

#endif // KCENON_MONITORING_ADAPTERS_THREAD_SYSTEM_ADAPTER_H

