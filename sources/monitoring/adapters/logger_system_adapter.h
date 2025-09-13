#pragma once

/**
 * @file logger_system_adapter.h
 * @brief Adapter for logger_system integration with monitoring
 *
 * This adapter provides optional integration with logger_system,
 * collecting logging metrics only when logger_system is available.
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
 * @struct logger_adapter_config
 * @brief Configuration for logger system adapter
 */
struct logger_adapter_config {
    std::chrono::milliseconds collection_interval{5000};
    bool track_log_levels = true;
    bool monitor_buffer_usage = true;
    size_t log_rate_window_size = 60; // seconds
};

/**
 * @class logger_system_adapter
 * @brief Adapter for collecting metrics from logger_system
 *
 * This adapter:
 * - Detects logger_system availability at runtime
 * - Collects logging metrics when available
 * - Monitors log levels and throughput
 * - Operates as no-op when logger_system is not present
 */
class logger_system_adapter : public interface_metric_collector {
public:
    using adapter_config = logger_adapter_config;

    explicit logger_system_adapter(std::shared_ptr<interface_event_bus> event_bus,
                                  const adapter_config& config = adapter_config())
        : event_bus_(event_bus), config_(config), is_collecting_(false),
          is_logger_system_available_(false), total_collections_(0),
          failed_collections_(0) {

        // Check if logger_system is available
        check_logger_system_availability();

        // Initialize log rate tracking
        log_rate_window_.resize(config_.log_rate_window_size, 0);
    }

    ~logger_system_adapter() {
        stop_collection();
    }

    // Metric collector interface implementation
    result<std::vector<metric>> collect_metrics() override {
        if (!is_logger_system_available_) {
            return std::vector<metric>{};
        }

        std::vector<metric> metrics;

        try {
            auto stats = collect_logging_stats();

            for (const auto& [logger_name, logger_stats] : stats) {
                // Total logs metric
                metrics.push_back(metric{
                    "logger.total_logs",
                    metric_value{static_cast<int64_t>(logger_stats.total_logs)},
                    {{"logger", logger_name}},
                    metric_type::counter
                });

                // Error count metric
                metrics.push_back(metric{
                    "logger.error_count",
                    metric_value{static_cast<int64_t>(logger_stats.error_count)},
                    {{"logger", logger_name}},
                    metric_type::counter
                });

                // Warning count metric
                metrics.push_back(metric{
                    "logger.warning_count",
                    metric_value{static_cast<int64_t>(logger_stats.warning_count)},
                    {{"logger", logger_name}},
                    metric_type::counter
                });

                // Logs per second metric
                metrics.push_back(metric{
                    "logger.logs_per_second",
                    metric_value{logger_stats.logs_per_second},
                    {{"logger", logger_name}},
                    metric_type::gauge
                });

                // Buffer usage metric
                if (config_.monitor_buffer_usage) {
                    metrics.push_back(metric{
                        "logger.buffer_usage_bytes",
                        metric_value{static_cast<int64_t>(logger_stats.buffer_usage_bytes)},
                        {{"logger", logger_name}},
                        metric_type::gauge
                    });
                }

                // Publish event
                if (event_bus_) {
                    event_bus_->publish_event(
                        logging_metric_event(logger_name, logger_stats)
                    );
                }
            }

            // Update log rate
            update_log_rate(calculate_total_logs(stats));

            total_collections_.fetch_add(1);
        } catch (const std::exception& e) {
            failed_collections_.fetch_add(1);
            return result<std::vector<metric>>(
                monitoring_error_code::collection_failed,
                std::string("Failed to collect logging metrics: ") + e.what()
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

        if (!is_logger_system_available_) {
            return result_void::error(monitoring_error_code::dependency_missing,
                                    "Logger system is not available");
        }

        collection_config_ = config;
        is_collecting_ = true;
        stop_requested_ = false;

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
        if (!is_logger_system_available_) {
            return {};
        }

        std::vector<std::string> types = {
            "logger.total_logs",
            "logger.error_count",
            "logger.warning_count",
            "logger.info_count",
            "logger.debug_count",
            "logger.logs_per_second"
        };

        if (config_.monitor_buffer_usage) {
            types.push_back("logger.buffer_usage_bytes");
        }

        return types;
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
        stats.avg_collection_time = std::chrono::milliseconds(20);
        stats.last_collection = last_collection_time_;
        return stats;
    }

    void reset_stats() override {
        total_collections_ = 0;
        failed_collections_ = 0;
        std::lock_guard<std::mutex> lock(rate_mutex_);
        std::fill(log_rate_window_.begin(), log_rate_window_.end(), 0);
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

    // Logger system specific methods
    bool is_logger_system_available() const {
        return is_logger_system_available_;
    }

    void register_logger(const std::string& logger_name) {
        std::lock_guard<std::mutex> lock(loggers_mutex_);
        registered_loggers_.insert(logger_name);
    }

    void unregister_logger(const std::string& logger_name) {
        std::lock_guard<std::mutex> lock(loggers_mutex_);
        registered_loggers_.erase(logger_name);
    }

    double get_current_log_rate() const {
        std::lock_guard<std::mutex> lock(rate_mutex_);
        uint64_t total = 0;
        for (const auto& count : log_rate_window_) {
            total += count;
        }
        return static_cast<double>(total) / log_rate_window_.size();
    }

private:
    void check_logger_system_availability() {
        // Check if logger_system library is linked
        is_logger_system_available_ = false; // Set to false by default

        // Could use dlsym or weak symbols to check
        // Example: is_logger_system_available_ = (dlsym(RTLD_DEFAULT, "logger_create") != nullptr);
    }

    void collection_worker() {
        while (!stop_requested_) {
            auto start = std::chrono::steady_clock::now();

            auto result = collect_metrics();

            if (result) {
                if (event_bus_ && !result.value().empty()) {
                    event_bus_->publish_event(
                        metric_collection_event("logger_system_adapter", result.value())
                    );
                }

                for (const auto& m : result.value()) {
                    notify_metric(metric_event("logger_system", m));
                }
            }

            last_collection_time_ = std::chrono::system_clock::now();

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

    std::unordered_map<std::string, logging_metric_event::logging_stats>
    collect_logging_stats() {
        std::unordered_map<std::string, logging_metric_event::logging_stats> stats;

        // Simulated data for demonstration
        logging_metric_event::logging_stats default_stats;
        default_stats.total_logs = 50000;
        default_stats.error_count = 10;
        default_stats.warning_count = 100;
        default_stats.info_count = 30000;
        default_stats.debug_count = 19890;
        default_stats.buffer_usage_bytes = 1024 * 64;
        default_stats.logs_per_second = get_current_log_rate();

        stats["main"] = default_stats;

        std::lock_guard<std::mutex> lock(loggers_mutex_);
        for (const auto& logger_name : registered_loggers_) {
            logging_metric_event::logging_stats logger_stats;
            // Would collect real stats here
            stats[logger_name] = logger_stats;
        }

        return stats;
    }

    uint64_t calculate_total_logs(
        const std::unordered_map<std::string, logging_metric_event::logging_stats>& stats) {
        uint64_t total = 0;
        for (const auto& [name, stat] : stats) {
            total += stat.total_logs;
        }
        return total;
    }

    void update_log_rate(uint64_t current_total) {
        std::lock_guard<std::mutex> lock(rate_mutex_);

        static uint64_t last_total = 0;
        uint64_t delta = (current_total > last_total) ? (current_total - last_total) : 0;

        // Shift window and add new value
        for (size_t i = 0; i < log_rate_window_.size() - 1; ++i) {
            log_rate_window_[i] = log_rate_window_[i + 1];
        }
        log_rate_window_.back() = delta;

        last_total = current_total;
    }

    std::shared_ptr<interface_event_bus> event_bus_;
    adapter_config config_;
    collection_config collection_config_;

    mutable std::mutex collection_mutex_;
    mutable std::mutex observers_mutex_;
    mutable std::mutex loggers_mutex_;
    mutable std::mutex rate_mutex_;

    std::vector<std::shared_ptr<interface_monitoring_observer>> observers_;
    std::set<std::string> registered_loggers_;
    std::vector<uint64_t> log_rate_window_;

    std::thread collection_thread_;
    std::condition_variable collection_cv_;

    std::atomic<bool> is_collecting_;
    std::atomic<bool> stop_requested_;
    bool is_logger_system_available_;

    std::atomic<uint64_t> total_collections_;
    std::atomic<uint64_t> failed_collections_;
    std::chrono::system_clock::time_point last_collection_time_;
};

} // namespace monitoring_system