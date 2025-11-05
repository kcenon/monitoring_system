/**
 * @file logger_di_integration_example.cpp
 * @brief Monitoring system integration example with Result pattern
 *
 * Demonstrates how monitoring_system uses common_system interfaces
 * and Result<T> pattern for error handling.
 */

#include <kcenon/monitoring/core/performance_monitor.h>
#include <kcenon/monitoring/core/result_types.h>
#include <kcenon/common/interfaces/logger_interface.h>
#include <kcenon/common/interfaces/monitoring_interface.h>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <iomanip>

using namespace kcenon::monitoring;
using namespace common::interfaces;

/**
 * @brief Simple logger implementation for demonstration
 */
class simple_console_logger : public ILogger {
private:
    log_level min_level_ = log_level::debug;
    std::atomic<size_t> log_count_{0};

public:
    explicit simple_console_logger(log_level min = log_level::debug)
        : min_level_(min) {}

    common::VoidResult log(log_level level, const std::string& message) override {
        if (!is_enabled(level)) {
            return common::ok();
        }

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        // Thread-safe time conversion
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif

        std::cout << "[" << std::put_time(&tm_buf, "%H:%M:%S")
                  << "] [" << to_string(level) << "] "
                  << message << std::endl;

        log_count_++;
        return common::ok();
    }

    common::VoidResult log(log_level level, const std::string& message,
                          const std::string& file, int line, const std::string& function) override {
        return log(level, message + " [" + file + ":" + std::to_string(line) + " " + function + "]");
    }

    common::VoidResult log(const log_entry& entry) override {
        return log(entry.level, entry.message, entry.file, entry.line, entry.function);
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

    size_t get_log_count() const { return log_count_.load(); }
};

/**
 * @brief Example 1: Basic monitoring with Result pattern
 */
void example_1_basic_monitoring() {
    std::cout << "\n=== Example 1: Basic Monitoring ===" << std::endl;

    // Create monitor
    auto monitor = std::make_shared<performance_monitor>();

    std::cout << "\nRecording metrics..." << std::endl;

    // Record metrics using Result pattern
    auto result1 = monitor->record_metric("requests_total", 100.0);
    if (common::is_ok(result1)) {
        std::cout << "✓ Metric 'requests_total' recorded" << std::endl;
    }

    auto result2 = monitor->record_metric("errors_total", 5.0);
    if (common::is_ok(result2)) {
        std::cout << "✓ Metric 'errors_total' recorded" << std::endl;
    }

    // Get metrics snapshot
    auto metrics = monitor->get_metrics();
    if (common::is_ok(metrics)) {
        auto snapshot = common::get_value(metrics);
        std::cout << "✓ Retrieved " << snapshot.metrics.size() << " metrics" << std::endl;
    }
}

/**
 * @brief Example 2: Error handling with Result pattern
 */
void example_2_error_handling() {
    std::cout << "\n=== Example 2: Error Handling ===" << std::endl;

    auto monitor = std::make_shared<performance_monitor>();

    // Record metric and check result
    auto result = monitor->record_metric("cpu_usage", 45.5);
    if (common::is_ok(result)) {
        std::cout << "✓ Metric recorded successfully" << std::endl;
    } else {
        auto err = common::get_error(result);
        std::cout << "✗ Error: " << err.message << std::endl;
    }
}

/**
 * @brief Example 3: Health monitoring
 */
void example_3_health_monitoring() {
    std::cout << "\n=== Example 3: Health Monitoring ===" << std::endl;

    auto monitor = std::make_shared<performance_monitor>();

    std::cout << "\nPerforming health check..." << std::endl;

    // Perform health check
    auto health_result = monitor->check_health();

    if (common::is_ok(health_result)) {
        const auto& health = common::get_value(health_result);

        std::cout << "\nHealth Check Results:" << std::endl;
        std::cout << "  Status: " << to_string(health.status) << std::endl;
        std::cout << "  Message: " << health.message << std::endl;
        std::cout << "  Duration: " << health.check_duration.count() << "ms" << std::endl;

        if (!health.metadata.empty()) {
            std::cout << "  Metadata:" << std::endl;
            for (const auto& [key, value] : health.metadata) {
                std::cout << "    " << key << ": " << value << std::endl;
            }
        }
    }
}

/**
 * @brief Example 4: Multiple monitors
 */
void example_4_multiple_monitors() {
    std::cout << "\n=== Example 4: Multiple Monitors ===" << std::endl;

    // Create multiple monitors
    auto monitor1 = std::make_shared<performance_monitor>();
    auto monitor2 = std::make_shared<performance_monitor>();

    std::cout << "\nMonitor 1 recording metrics..." << std::endl;
    monitor1->record_metric("monitor1_metric", 100.0);

    std::cout << "Monitor 2 recording metrics..." << std::endl;
    monitor2->record_metric("monitor2_metric", 200.0);

    // Get metrics from both
    auto metrics1 = monitor1->get_metrics();
    auto metrics2 = monitor2->get_metrics();

    if (common::is_ok(metrics1) && common::is_ok(metrics2)) {
        auto snapshot1 = common::get_value(metrics1);
        auto snapshot2 = common::get_value(metrics2);
        std::cout << "✓ Monitor 1: " << snapshot1.metrics.size() << " metrics" << std::endl;
        std::cout << "✓ Monitor 2: " << snapshot2.metrics.size() << " metrics" << std::endl;
    }
}

/**
 * @brief Example 5: Metrics with tags
 */
void example_5_metrics_with_tags() {
    std::cout << "\n=== Example 5: Metrics with Tags ===" << std::endl;

    auto monitor = std::make_shared<performance_monitor>();

    // Record metrics with tags
    std::unordered_map<std::string, std::string> tags{
        {"service", "api"},
        {"region", "us-east-1"},
        {"instance", "i-12345"}
    };

    auto result = monitor->record_metric("request_latency", 150.0, tags);
    if (common::is_ok(result)) {
        std::cout << "✓ Metric with tags recorded successfully" << std::endl;
    }
}

/**
 * @brief Example 6: Simulating monitoring workflow
 */
void example_6_monitoring_workflow() {
    std::cout << "\n=== Example 6: Monitoring Workflow ===" << std::endl;

    auto monitor = std::make_shared<performance_monitor>();
    auto logger = std::make_shared<simple_console_logger>();

    std::cout << "\nSimulating application workload..." << std::endl;

    // Simulate application metrics
    for (int i = 0; i < 5; ++i) {
        auto result = monitor->record_metric("requests", static_cast<double>(i * 10));
        if (common::is_ok(result)) {
            logger->log(log_level::info, "Recorded metric: requests = " + std::to_string(i * 10));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Check health and log results
    auto health = monitor->check_health();
    if (common::is_ok(health)) {
        auto health_status = common::get_value(health);
        logger->log(log_level::info, "Monitor health: " + to_string(health_status.status));
    }

    // Get metrics and log summary
    auto metrics = monitor->get_metrics();
    if (common::is_ok(metrics)) {
        auto snapshot = common::get_value(metrics);
        logger->log(log_level::info, "Collected " + std::to_string(snapshot.metrics.size()) + " metrics");
    }

    std::cout << "\n✓ Workflow completed successfully" << std::endl;
    std::cout << "  Logger events: " << logger->get_log_count() << std::endl;
}

int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "Monitoring System - Integration Examples" << std::endl;
    std::cout << "Using common_system interfaces and Result<T> pattern" << std::endl;
    std::cout << "========================================================" << std::endl;

    try {
        example_1_basic_monitoring();
        example_2_error_handling();
        example_3_health_monitoring();
        example_4_multiple_monitors();
        example_5_metrics_with_tags();
        example_6_monitoring_workflow();

        std::cout << "\n========================================================" << std::endl;
        std::cout << "All integration examples completed!" << std::endl;
        std::cout << "Key Points:" << std::endl;
        std::cout << "  ✓ common_system interfaces used" << std::endl;
        std::cout << "  ✓ Result<T> pattern for error handling" << std::endl;
        std::cout << "  ✓ Interface-based loose coupling" << std::endl;
        std::cout << "  ✓ Production-ready monitoring" << std::endl;
        std::cout << "========================================================" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
