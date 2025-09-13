#pragma once

/**
 * @file thread_system_adapter.h
 * @brief Adapter for thread_system integration with monitoring
 *
 * This adapter provides optional integration with thread_system,
 * collecting metrics only when thread_system is available.
 */

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include "../interfaces/metric_collector_interface.h"
#include "../interfaces/event_bus_interface.h"
#include "../core/event_types.h"
#include "../core/result_types.h"

namespace monitoring_system {

/**
 * @struct thread_adapter_config
 * @brief Configuration for thread system adapter
 */
struct thread_adapter_config {
    std::chrono::milliseconds collection_interval{1000};
    bool enable_detailed_metrics = true;
    bool auto_register_pools = true;
};

/**
 * @class thread_system_adapter
 * @brief Adapter for collecting metrics from thread_system
 *
 * This adapter:
 * - Detects thread_system availability at runtime
 * - Collects thread pool metrics when available
 * - Publishes metrics via event bus
 * - Operates as no-op when thread_system is not present
 */
class thread_system_adapter : public interface_metric_collector {
public:
    using adapter_config = thread_adapter_config;

    explicit thread_system_adapter(std::shared_ptr<interface_event_bus> event_bus,
                                  const adapter_config& config = adapter_config())
        : event_bus_(event_bus), config_(config), is_collecting_(false),
          is_thread_system_available_(false), total_collections_(0),
          failed_collections_(0) {

        // Check if thread_system is available
        check_thread_system_availability();

        // Register for configuration changes
        if (event_bus_) {
            event_bus_->subscribe_event<configuration_change_event>(
                [this](const configuration_change_event& event) {
                    handle_configuration_change(event);
                },
                event_priority::normal
            );
        }
    }

    ~thread_system_adapter() {
        stop_collection();
    }

    // Metric collector interface implementation
    result<std::vector<metric>> collect_metrics() override {
        if (!is_thread_system_available_) {
            return std::vector<metric>{};
        }

        std::vector<metric> metrics;

        try {
            // Simulate thread pool metrics collection
            // In real implementation, this would interface with actual thread_system
            auto stats = collect_thread_pool_stats();

            for (const auto& [pool_name, pool_stats] : stats) {
                // CPU usage metric
                metrics.push_back(metric{
                    "thread_pool.cpu_usage",
                    metric_value{pool_stats.cpu_usage_percent},
                    {{"pool", pool_name}},
                    metric_type::gauge
                });

                // Active threads metric
                metrics.push_back(metric{
                    "thread_pool.active_threads",
                    metric_value{static_cast<int64_t>(pool_stats.active_threads)},
                    {{"pool", pool_name}},
                    metric_type::gauge
                });

                // Queued tasks metric
                metrics.push_back(metric{
                    "thread_pool.queued_tasks",
                    metric_value{static_cast<int64_t>(pool_stats.queued_tasks)},
                    {{"pool", pool_name}},
                    metric_type::gauge
                });

                // Completed tasks metric
                metrics.push_back(metric{
                    "thread_pool.completed_tasks",
                    metric_value{static_cast<int64_t>(pool_stats.completed_tasks)},
                    {{"pool", pool_name}},
                    metric_type::counter
                });

                // Publish event
                if (event_bus_) {
                    event_bus_->publish_event(
                        thread_pool_metric_event(pool_name, pool_stats)
                    );
                }
            }

            total_collections_.fetch_add(1);
        } catch (const std::exception& e) {
            failed_collections_.fetch_add(1);
            return result<std::vector<metric>>(
                monitoring_error_code::collection_failed,
                std::string("Failed to collect thread pool metrics: ") + e.what()
            );
        }

        return metrics;
    }

    result_void start_collection(const collection_config& config) override {
        std::lock_guard<std::mutex> lock(collection_mutex_);

        if (is_collecting_) {
            return result_void::error(monitoring_error_code::already_started,
                                    "Collection already in progress");
        }

        if (!is_thread_system_available_) {
            return result_void::error(monitoring_error_code::dependency_missing,
                                    "Thread system is not available");
        }

        collection_config_ = config;
        is_collecting_ = true;
        stop_requested_ = false;

        // Start collection thread
        collection_thread_ = std::thread([this] {
            collection_worker();
        });

        return result_void::success();
    }

    result_void stop_collection() override {
        std::lock_guard<std::mutex> lock(collection_mutex_);

        if (!is_collecting_) {
            return result_void::success();
        }

        stop_requested_ = true;
        is_collecting_ = false;

        if (collection_thread_.joinable()) {
            collection_thread_.join();
        }

        return result_void::success();
    }

    bool is_collecting() const override {
        std::lock_guard<std::mutex> lock(collection_mutex_);
        return is_collecting_;
    }

    std::vector<std::string> get_metric_types() const override {
        if (!is_thread_system_available_) {
            return {};
        }

        return {
            "thread_pool.cpu_usage",
            "thread_pool.active_threads",
            "thread_pool.idle_threads",
            "thread_pool.queued_tasks",
            "thread_pool.completed_tasks",
            "thread_pool.avg_task_duration"
        };
    }

    collection_config get_config() const override {
        std::lock_guard<std::mutex> lock(collection_mutex_);
        return collection_config_;
    }

    result_void update_config(const collection_config& config) override {
        std::lock_guard<std::mutex> lock(collection_mutex_);
        collection_config_ = config;
        return result_void::success();
    }

    result<std::vector<metric>> force_collect() override {
        return collect_metrics();
    }

    metric_stats get_stats() const override {
        metric_stats stats;
        stats.total_collected = total_collections_.load();
        stats.total_errors = failed_collections_.load();
        stats.total_dropped = 0;
        stats.avg_collection_time = std::chrono::milliseconds(50); // Simulated
        stats.last_collection = last_collection_time_;
        return stats;
    }

    void reset_stats() override {
        total_collections_ = 0;
        failed_collections_ = 0;
    }

    // Observable interface implementation
    result_void register_observer(std::shared_ptr<interface_monitoring_observer> observer) override {
        std::lock_guard<std::mutex> lock(observers_mutex_);
        observers_.push_back(observer);
        return result_void::success();
    }

    result_void unregister_observer(std::shared_ptr<interface_monitoring_observer> observer) override {
        std::lock_guard<std::mutex> lock(observers_mutex_);
        observers_.erase(
            std::remove(observers_.begin(), observers_.end(), observer),
            observers_.end()
        );
        return result_void::success();
    }

    void notify_metric(const metric_event& event) override {
        std::lock_guard<std::mutex> lock(observers_mutex_);
        for (auto& observer : observers_) {
            observer->on_metric_collected(event);
        }
    }

    void notify_event(const system_event& event) override {
        std::lock_guard<std::mutex> lock(observers_mutex_);
        for (auto& observer : observers_) {
            observer->on_event_occurred(event);
        }
    }

    void notify_state_change(const state_change_event& event) override {
        std::lock_guard<std::mutex> lock(observers_mutex_);
        for (auto& observer : observers_) {
            observer->on_system_state_changed(event);
        }
    }

    // Thread system specific methods
    bool is_thread_system_available() const {
        return is_thread_system_available_;
    }

    void register_thread_pool(const std::string& pool_name) {
        std::lock_guard<std::mutex> lock(pools_mutex_);
        registered_pools_.insert(pool_name);
    }

    void unregister_thread_pool(const std::string& pool_name) {
        std::lock_guard<std::mutex> lock(pools_mutex_);
        registered_pools_.erase(pool_name);
    }

private:
    void check_thread_system_availability() {
        // In real implementation, this would check if thread_system library is linked
        // For now, we simulate availability
        is_thread_system_available_ = false; // Set to false by default

        // Could use dlsym or weak symbols to check for thread_system functions
        // Example: is_thread_system_available_ = (dlsym(RTLD_DEFAULT, "thread_pool_create") != nullptr);
    }

    void collection_worker() {
        while (!stop_requested_) {
            auto start = std::chrono::steady_clock::now();

            // Collect metrics
            auto result = collect_metrics();

            if (result) {
                // Publish collected metrics
                if (event_bus_ && !result.value().empty()) {
                    event_bus_->publish_event(
                        metric_collection_event("thread_system_adapter", result.value())
                    );
                }

                // Notify observers
                for (const auto& m : result.value()) {
                    notify_metric(metric_event("thread_system", m));
                }
            }

            last_collection_time_ = std::chrono::system_clock::now();

            // Sleep for the remaining interval
            auto elapsed = std::chrono::steady_clock::now() - start;
            auto sleep_time = collection_config_.interval -
                            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

            if (sleep_time.count() > 0) {
                std::unique_lock<std::mutex> lock(collection_mutex_);
                collection_cv_.wait_for(lock, sleep_time,
                                       [this] { return stop_requested_.load(); });
            }
        }
    }

    std::unordered_map<std::string, thread_pool_metric_event::thread_pool_stats>
    collect_thread_pool_stats() {
        std::unordered_map<std::string, thread_pool_metric_event::thread_pool_stats> stats;

        // Simulated data for demonstration
        // In real implementation, this would interface with actual thread_system
        if (config_.auto_register_pools) {
            thread_pool_metric_event::thread_pool_stats default_stats;
            default_stats.active_threads = 4;
            default_stats.idle_threads = 4;
            default_stats.queued_tasks = 10;
            default_stats.completed_tasks = 1000;
            default_stats.cpu_usage_percent = 45.5;
            default_stats.avg_task_duration = std::chrono::milliseconds(25);

            stats["default_pool"] = default_stats;
        }

        std::lock_guard<std::mutex> lock(pools_mutex_);
        for (const auto& pool_name : registered_pools_) {
            thread_pool_metric_event::thread_pool_stats pool_stats;
            // Would collect real stats here
            stats[pool_name] = pool_stats;
        }

        return stats;
    }

    void handle_configuration_change(const configuration_change_event& event) {
        if (event.get_component() == "thread_system_adapter") {
            // Handle configuration changes
            if (event.get_config_key() == "collection_interval") {
                try {
                    auto new_interval = std::stoi(event.get_new_value());
                    config_.collection_interval = std::chrono::milliseconds(new_interval);
                } catch (...) {
                    // Invalid value, ignore
                }
            }
        }
    }

    std::shared_ptr<interface_event_bus> event_bus_;
    adapter_config config_;
    collection_config collection_config_;

    mutable std::mutex collection_mutex_;
    mutable std::mutex observers_mutex_;
    mutable std::mutex pools_mutex_;

    std::vector<std::shared_ptr<interface_monitoring_observer>> observers_;
    std::set<std::string> registered_pools_;

    std::thread collection_thread_;
    std::condition_variable collection_cv_;

    std::atomic<bool> is_collecting_;
    std::atomic<bool> stop_requested_;
    bool is_thread_system_available_;

    std::atomic<uint64_t> total_collections_;
    std::atomic<uint64_t> failed_collections_;
    std::chrono::system_clock::time_point last_collection_time_;
};

} // namespace monitoring_system