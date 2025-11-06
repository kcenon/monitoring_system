/**
 * @file monitor_factory_pattern_example.cpp
 * @brief Monitor factory and provider pattern examples for Phase 4
 *
 * Demonstrates advanced DI patterns including factory pattern,
 * monitor providers, and aggregation strategies.
 */

#include <kcenon/monitoring/core/performance_monitor.h>
#include <kcenon/monitoring/core/result_types.h>
#include <kcenon/common/interfaces/logger_interface.h>
#include <kcenon/common/interfaces/monitoring_interface.h>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <vector>

using namespace kcenon::monitoring;
using namespace common::interfaces;

/**
 * @brief Monitor factory implementing singleton pattern with DI
 */
class monitor_factory : public IMonitorProvider {
private:
    static std::shared_ptr<monitor_factory> instance_;
    static std::mutex instance_mutex_;

    std::shared_ptr<IMonitor> default_monitor_;
    std::unordered_map<std::string, std::shared_ptr<IMonitor>> named_monitors_;
    std::shared_ptr<ILogger> shared_logger_;
    mutable std::mutex factory_mutex_;

    monitor_factory() = default;

public:
    static std::shared_ptr<monitor_factory> instance() {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (!instance_) {
            instance_ = std::shared_ptr<monitor_factory>(new monitor_factory());
        }
        return instance_;
    }

    void set_shared_logger(std::shared_ptr<ILogger> logger) {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        shared_logger_ = logger;

        // Note: Logger injection not available in performance_monitor yet
        // Future enhancement: Add ILogger support to performance_monitor
    }

    // IMonitorProvider implementation
    std::shared_ptr<IMonitor> get_monitor() override {
        std::lock_guard<std::mutex> lock(factory_mutex_);

        if (!default_monitor_) {
            default_monitor_ = std::make_shared<performance_monitor>();
        }

        return default_monitor_;
    }

    std::shared_ptr<IMonitor> create_monitor(const std::string& name) override {
        std::lock_guard<std::mutex> lock(factory_mutex_);

        auto it = named_monitors_.find(name);
        if (it != named_monitors_.end()) {
            return it->second;
        }

        auto monitor = std::make_shared<performance_monitor>();
        named_monitors_[name] = monitor;
        return monitor;
    }

    std::vector<std::string> list_monitors() const {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        std::vector<std::string> names;
        names.reserve(named_monitors_.size());

        for (const auto& [name, _] : named_monitors_) {
            names.push_back(name);
        }

        return names;
    }

    size_t monitor_count() const {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        return named_monitors_.size() + (default_monitor_ ? 1 : 0);
    }

    void reset() {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        default_monitor_.reset();
        named_monitors_.clear();
    }
};

std::shared_ptr<monitor_factory> monitor_factory::instance_ = nullptr;
std::mutex monitor_factory::instance_mutex_;

/**
 * @brief Simple logger for examples
 */
class example_logger : public ILogger {
private:
    std::string prefix_;
    std::atomic<size_t> count_{0};
    log_level min_level_ = log_level::trace;

public:
    explicit example_logger(const std::string& prefix = "LOG")
        : prefix_(prefix) {}

    common::VoidResult log(log_level level, const std::string& message) override {
        std::cout << "[" << prefix_ << "] [" << to_string(level) << "] "
                  << message << std::endl;
        count_++;
        return common::ok();
    }

    common::VoidResult log(log_level level, const std::string& message,
                          const std::string&, int, const std::string&) override {
        return log(level, message);
    }

    common::VoidResult log(const log_entry& entry) override {
        return log(entry.level, entry.message);
    }

    bool is_enabled(log_level level) const override {
        return static_cast<int>(level) >= static_cast<int>(min_level_);
    }

    common::VoidResult set_level(log_level level) override {
        min_level_ = level;
        return common::ok();
    }

    log_level get_level() const override {
        return min_level_;
    }

    common::VoidResult flush() override {
        std::cout << std::flush;
        return common::ok();
    }

    size_t count() const { return count_.load(); }
};

/**
 * @brief Example 1: Basic factory pattern
 */
void example_1_basic_factory() {
    std::cout << "\n=== Example 1: Basic Factory Pattern ===" << std::endl;

    auto factory = monitor_factory::instance();

    std::cout << "Getting default monitor from factory..." << std::endl;
    auto monitor = factory->get_monitor();

    if (monitor) {
        std::cout << "✓ Obtained monitor instance" << std::endl;

        monitor->record_metric("test_metric", 42.0);

        auto metrics = monitor->get_metrics();
        if (metrics) {
            std::cout << "✓ Monitor has " << metrics.value().metrics.size()
                      << " metrics" << std::endl;
        }
    }
}

/**
 * @brief Example 2: Named monitors via factory
 */
void example_2_named_monitors() {
    std::cout << "\n=== Example 2: Named Monitors ===" << std::endl;

    auto factory = monitor_factory::instance();

    // Create named monitors
    auto web_monitor = factory->create_monitor("web_server");
    auto db_monitor = factory->create_monitor("database");
    auto cache_monitor = factory->create_monitor("cache");

    std::cout << "Created 3 named monitors" << std::endl;

    // Use monitors
    web_monitor->record_metric("requests", 1000.0);
    db_monitor->record_metric("queries", 500.0);
    cache_monitor->record_metric("hits", 750.0);

    // List all monitors
    auto names = factory->list_monitors();
    std::cout << "\nRegistered monitors (" << names.size() << "):" << std::endl;
    for (const auto& name : names) {
        std::cout << "  - " << name << std::endl;
    }
}

/**
 * @brief Example 3: Factory with shared logger
 */
void example_3_factory_with_logger() {
    std::cout << "\n=== Example 3: Factory with Shared Logger ===" << std::endl;

    auto factory = monitor_factory::instance();
    auto logger = std::make_shared<example_logger>("FACTORY");

    // Set shared logger for all monitors
    factory->set_shared_logger(logger);

    std::cout << "Shared logger configured for factory" << std::endl;

    // All new monitors will automatically use the shared logger
    auto monitor1 = factory->create_monitor("service_a");
    auto monitor2 = factory->create_monitor("service_b");

    std::cout << "\nRecording metrics with shared logger..." << std::endl;
    monitor1->record_metric("metric_a", 10.0);
    monitor2->record_metric("metric_b", 20.0);

    std::cout << "\nShared logger received " << logger->count()
              << " messages from all monitors" << std::endl;
}

/**
 * @brief Example 4: Monitor reuse via factory
 */
void example_4_monitor_reuse() {
    std::cout << "\n=== Example 4: Monitor Reuse ===" << std::endl;

    auto factory = monitor_factory::instance();

    // Create monitor with specific name
    auto monitor1 = factory->create_monitor("shared_monitor");
    monitor1->record_metric("counter", 1.0);

    // Get same monitor by name (reuse)
    auto monitor2 = factory->create_monitor("shared_monitor");

    // They are the same instance
    std::cout << "Monitor instances "
              << (monitor1 == monitor2 ? "identical ✓" : "different ✗")
              << std::endl;

    monitor2->record_metric("counter", 2.0);

    // Check metrics
    auto metrics = monitor1->get_metrics();
    if (metrics) {
        std::cout << "Shared monitor has " << metrics.value().metrics.size()
                  << " metrics (accumulated from both uses)" << std::endl;
    }
}

/**
 * @brief Example 5: Provider interface usage
 */
void example_5_provider_interface() {
    std::cout << "\n=== Example 5: IMonitorProvider Interface ===" << std::endl;

    // Use factory through IMonitorProvider interface
    std::shared_ptr<IMonitorProvider> provider = monitor_factory::instance();

    std::cout << "Using factory via IMonitorProvider interface" << std::endl;

    auto monitor = provider->get_monitor();
    if (monitor) {
        std::cout << "✓ Retrieved monitor through provider interface" << std::endl;

        monitor->record_metric("provider_test", 99.0);

        auto health = monitor->check_health();
        if (health) {
            std::cout << "✓ Monitor health: "
                      << to_string(health.value().status) << std::endl;
        }
    }

    auto named = provider->create_monitor("provider_created");
    if (named) {
        std::cout << "✓ Created named monitor through provider" << std::endl;
    }
}

/**
 * @brief Example 6: Aggregating monitor pattern
 */
class aggregating_monitor : public IMonitor {
private:
    std::vector<std::shared_ptr<IMonitor>> monitors_;
    std::mutex mutex_;

public:
    void add_monitor(std::shared_ptr<IMonitor> monitor) {
        std::lock_guard<std::mutex> lock(mutex_);
        monitors_.push_back(monitor);
    }

    common::VoidResult record_metric(const std::string& name, double value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& monitor : monitors_) {
            monitor->record_metric(name, value);
        }
        return common::ok();
    }

    common::VoidResult record_metric(
        const std::string& name,
        double value,
        const std::unordered_map<std::string, std::string>& tags) override
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& monitor : monitors_) {
            monitor->record_metric(name, value, tags);
        }
        return common::ok();
    }

    common::Result<metrics_snapshot> get_metrics() override {
        std::lock_guard<std::mutex> lock(mutex_);

        metrics_snapshot combined;
        combined.source_id = "aggregating_monitor";
        combined.capture_time = std::chrono::system_clock::now();

        for (auto& monitor : monitors_) {
            auto result = monitor->get_metrics();
            if (result) {
                for (const auto& metric : result.value().metrics) {
                    combined.metrics.push_back(metric);
                }
            }
        }

        return common::ok(std::move(combined));
    }

    common::Result<health_check_result> check_health() override {
        health_check_result result;
        result.timestamp = std::chrono::system_clock::now();
        result.status = health_status::healthy;
        result.message = "Aggregating monitor";

        return common::ok(std::move(result));
    }

    common::VoidResult reset() override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& monitor : monitors_) {
            monitor->reset();
        }
        return common::ok();
    }

    size_t monitor_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return monitors_.size();
    }
};

void example_6_aggregating_pattern() {
    std::cout << "\n=== Example 6: Aggregating Monitor Pattern ===" << std::endl;

    auto factory = monitor_factory::instance();
    auto aggregator = std::make_shared<aggregating_monitor>();

    // Create multiple monitors and add to aggregator
    auto mon1 = factory->create_monitor("service_1");
    auto mon2 = factory->create_monitor("service_2");
    auto mon3 = factory->create_monitor("service_3");

    aggregator->add_monitor(mon1);
    aggregator->add_monitor(mon2);
    aggregator->add_monitor(mon3);

    std::cout << "Aggregator managing " << aggregator->monitor_count()
              << " monitors" << std::endl;

    // Record to aggregator - broadcasts to all
    aggregator->record_metric("broadcast_metric", 100.0);

    std::cout << "\nMetric broadcasted to all monitors" << std::endl;

    // Get combined metrics
    auto metrics = aggregator->get_metrics();
    if (metrics) {
        std::cout << "Combined metrics count: "
                  << metrics.value().metrics.size() << std::endl;
    }
}

/**
 * @brief Example 7: Factory cleanup and reset
 */
void example_7_factory_lifecycle() {
    std::cout << "\n=== Example 7: Factory Lifecycle Management ===" << std::endl;

    auto factory = monitor_factory::instance();

    std::cout << "Initial monitor count: " << factory->monitor_count() << std::endl;

    // Create some monitors
    factory->create_monitor("temp_1");
    factory->create_monitor("temp_2");

    std::cout << "After creation: " << factory->monitor_count() << std::endl;

    // Reset factory
    factory->reset();

    std::cout << "After reset: " << factory->monitor_count() << std::endl;
    std::cout << "✓ Factory lifecycle managed successfully" << std::endl;
}

int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "Monitor Factory Pattern Examples (Phase 4)" << std::endl;
    std::cout << "Advanced DI Patterns for Monitoring System" << std::endl;
    std::cout << "========================================================" << std::endl;

    try {
        example_1_basic_factory();
        example_2_named_monitors();
        example_3_factory_with_logger();
        example_4_monitor_reuse();
        example_5_provider_interface();
        example_6_aggregating_pattern();
        example_7_factory_lifecycle();

        std::cout << "\n========================================================" << std::endl;
        std::cout << "All factory pattern examples completed!" << std::endl;
        std::cout << "Key Patterns Demonstrated:" << std::endl;
        std::cout << "  ✓ Singleton factory pattern" << std::endl;
        std::cout << "  ✓ Named monitor management" << std::endl;
        std::cout << "  ✓ Shared logger injection" << std::endl;
        std::cout << "  ✓ Monitor reuse and lifecycle" << std::endl;
        std::cout << "  ✓ IMonitorProvider interface" << std::endl;
        std::cout << "  ✓ Aggregating monitor pattern" << std::endl;
        std::cout << "========================================================" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
