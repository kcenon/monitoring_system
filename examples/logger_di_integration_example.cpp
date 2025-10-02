/**
 * @file logger_di_integration_example.cpp
 * @brief Logger integration example using DI pattern for Phase 4
 *
 * Demonstrates how monitoring_system integrates with logger_system
 * using only common_system interfaces (no circular dependencies).
 */

#include <monitoring/core/performance_monitor.h>
#include <monitoring/core/result_types.h>
#include <kcenon/common/interfaces/logger_interface.h>
#include <kcenon/common/interfaces/monitoring_interface.h>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <iomanip>

using namespace monitoring_system;
using namespace kcenon::common::interfaces;

/**
 * @brief Simple logger implementation for demonstration
 *
 * This demonstrates that monitoring_system can work with ANY ILogger
 * implementation without depending on specific logger_system code.
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
            return common::VoidResult::success();
        }

        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        std::cout << "[" << std::put_time(std::localtime(&time), "%H:%M:%S")
                  << "] [" << to_string(level) << "] "
                  << message << std::endl;

        log_count_++;
        return common::VoidResult::success();
    }

    bool is_enabled(log_level level) const override {
        return static_cast<int>(level) >= static_cast<int>(min_level_);
    }

    size_t get_log_count() const { return log_count_.load(); }
};

/**
 * @brief Example 1: Basic logger injection into monitor
 */
void example_1_basic_logger_injection() {
    std::cout << "\n=== Example 1: Basic Logger Injection ===" << std::endl;

    // Step 1: Create logger (any ILogger implementation)
    auto logger = std::make_shared<simple_console_logger>(log_level::info);

    // Step 2: Create monitor
    auto monitor = std::make_shared<performance_monitor>();

    // Step 3: Inject logger into monitor via DI
    monitor->set_logger(logger);

    std::cout << "\nMonitor configured with logger. Recording metrics..." << std::endl;

    // Step 4: Monitor operations automatically log events
    monitor->record_metric("requests_total", 100.0);
    monitor->record_metric("errors_total", 5.0);
    monitor->record_metric("cpu_usage", 45.5);

    std::cout << "\nLogger received " << logger->get_log_count()
              << " log messages from monitor" << std::endl;
}

/**
 * @brief Example 2: Monitor without logger (optional dependency)
 */
void example_2_optional_logger() {
    std::cout << "\n=== Example 2: Optional Logger (No Logger) ===" << std::endl;

    // Create monitor without logger
    auto monitor = std::make_shared<performance_monitor>();

    std::cout << "Monitor operates without logger (DI optional)" << std::endl;

    // Monitor works fine without logger
    auto result = monitor->record_metric("metric_1", 42.0);
    if (result) {
        std::cout << "✓ Metric recorded successfully without logger" << std::endl;
    }

    // Get metrics
    auto metrics = monitor->get_metrics();
    if (metrics) {
        std::cout << "✓ Retrieved " << metrics.value().metrics.size()
                  << " metrics without logger" << std::endl;
    }
}

/**
 * @brief Example 3: Runtime logger injection and swapping
 */
void example_3_runtime_logger_management() {
    std::cout << "\n=== Example 3: Runtime Logger Management ===" << std::endl;

    auto monitor = std::make_shared<performance_monitor>();
    auto logger1 = std::make_shared<simple_console_logger>(log_level::info);
    auto logger2 = std::make_shared<simple_console_logger>(log_level::debug);

    std::cout << "\nPhase 1: Operating without logger" << std::endl;
    monitor->record_metric("phase1_metric", 10.0);

    std::cout << "\nPhase 2: Inject logger at runtime" << std::endl;
    monitor->set_logger(logger1);
    monitor->record_metric("phase2_metric", 20.0);

    std::cout << "\nPhase 3: Swap to different logger" << std::endl;
    monitor->set_logger(logger2);
    monitor->record_metric("phase3_metric", 30.0);

    std::cout << "\nLogger 1 received: " << logger1->get_log_count() << " messages" << std::endl;
    std::cout << "Logger 2 received: " << logger2->get_log_count() << " messages" << std::endl;
}

/**
 * @brief Example 4: Using IMonitorProvider interface
 */
void example_4_monitor_provider_pattern() {
    std::cout << "\n=== Example 4: IMonitorProvider Pattern ===" << std::endl;

    auto logger = std::make_shared<simple_console_logger>();

    // Create monitor that implements both IMonitor and IMonitorProvider
    auto monitor = std::make_shared<performance_monitor>();
    monitor->set_logger(logger);

    // Use as IMonitorProvider
    std::shared_ptr<IMonitorProvider> provider = monitor;

    std::cout << "Obtaining monitor through IMonitorProvider interface..." << std::endl;

    // Get monitor instance
    auto monitor_instance = provider->get_monitor();
    if (monitor_instance) {
        std::cout << "✓ Retrieved monitor via provider interface" << std::endl;

        // Use the monitor
        monitor_instance->record_metric("provider_test", 99.0);

        auto metrics = monitor_instance->get_metrics();
        if (metrics) {
            std::cout << "✓ Metrics available through provider: "
                      << metrics.value().metrics.size() << std::endl;
        }
    }

    // Create named monitor
    auto named_monitor = provider->create_monitor("test_monitor");
    if (named_monitor) {
        std::cout << "✓ Created named monitor via provider" << std::endl;
    }
}

/**
 * @brief Example 5: Health monitoring with logger integration
 */
void example_5_health_monitoring() {
    std::cout << "\n=== Example 5: Health Monitoring with Logger ===" << std::endl;

    auto logger = std::make_shared<simple_console_logger>(log_level::info);
    auto monitor = std::make_shared<performance_monitor>();
    monitor->set_logger(logger);

    std::cout << "\nPerforming health check..." << std::endl;

    // Perform health check
    auto health_result = monitor->check_health();

    if (health_result) {
        const auto& health = health_result.value();

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

    std::cout << "\nLogger captured health check events: "
              << logger->get_log_count() << std::endl;
}

/**
 * @brief Example 6: Simulating bidirectional integration
 */
void example_6_bidirectional_integration() {
    std::cout << "\n=== Example 6: Bidirectional Integration Simulation ===" << std::endl;
    std::cout << "Demonstrates logger-monitor cooperation via interfaces" << std::endl;

    // Create components
    auto logger = std::make_shared<simple_console_logger>();
    auto monitor = std::make_shared<performance_monitor>();

    // Setup bidirectional relationship via DI
    monitor->set_logger(logger);  // Monitor -> Logger

    std::cout << "\nScenario: Application monitoring with logging" << std::endl;

    // Simulate application metrics
    std::cout << "\nRecording application metrics..." << std::endl;
    for (int i = 0; i < 5; ++i) {
        monitor->record_metric("requests", static_cast<double>(i * 10));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Monitor can check its own health and log results
    auto health = monitor->check_health();
    if (health) {
        logger->log(log_level::info,
            "Monitor health: " + to_string(health.value().status));
    }

    // Get metrics and log summary
    auto metrics = monitor->get_metrics();
    if (metrics) {
        logger->log(log_level::info,
            "Collected " + std::to_string(metrics.value().metrics.size()) + " metrics");
    }

    std::cout << "\n✓ Bidirectional integration successful" << std::endl;
    std::cout << "  Monitor -> Logger: " << logger->get_log_count() << " events" << std::endl;
    std::cout << "  No circular dependencies!" << std::endl;
}

/**
 * @brief Example 7: Multiple monitors with shared logger
 */
void example_7_shared_logger() {
    std::cout << "\n=== Example 7: Multiple Monitors, Shared Logger ===" << std::endl;

    auto shared_logger = std::make_shared<simple_console_logger>();

    // Create multiple monitors sharing the same logger
    auto monitor1 = std::make_shared<performance_monitor>();
    auto monitor2 = std::make_shared<performance_monitor>();

    monitor1->set_logger(shared_logger);
    monitor2->set_logger(shared_logger);

    std::cout << "\nMonitor 1 and 2 share the same logger instance" << std::endl;

    // Both monitors log to same logger
    monitor1->record_metric("monitor1_metric", 100.0);
    monitor2->record_metric("monitor2_metric", 200.0);

    std::cout << "\nShared logger received " << shared_logger->get_log_count()
              << " messages from all monitors" << std::endl;
}

int main() {
    std::cout << "========================================================" << std::endl;
    std::cout << "Monitoring System - Logger DI Integration Examples" << std::endl;
    std::cout << "Phase 4: Dependency Injection Pattern" << std::endl;
    std::cout << "========================================================" << std::endl;

    try {
        example_1_basic_logger_injection();
        example_2_optional_logger();
        example_3_runtime_logger_management();
        example_4_monitor_provider_pattern();
        example_5_health_monitoring();
        example_6_bidirectional_integration();
        example_7_shared_logger();

        std::cout << "\n========================================================" << std::endl;
        std::cout << "All logger DI integration examples completed!" << std::endl;
        std::cout << "Key Points:" << std::endl;
        std::cout << "  ✓ Zero circular dependencies" << std::endl;
        std::cout << "  ✓ Interface-based loose coupling" << std::endl;
        std::cout << "  ✓ Optional logger support" << std::endl;
        std::cout << "  ✓ Runtime configuration flexibility" << std::endl;
        std::cout << "========================================================" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
