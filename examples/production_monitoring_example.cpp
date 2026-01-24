// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
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
 * @file production_monitoring_example.cpp
 * @brief Complete production-ready monitoring setup demonstration
 *
 * This example demonstrates:
 * - Configure complete monitoring stack
 * - Integrate health checks with monitoring
 * - Set up alert pipeline with multiple channels
 * - Configure storage backend with retention
 * - Demonstrate graceful shutdown procedures
 * - Show configuration management patterns
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "kcenon/monitoring/alert/alert_manager.h"
#include "kcenon/monitoring/alert/alert_notifiers.h"
#include "kcenon/monitoring/alert/alert_triggers.h"
#include "kcenon/monitoring/core/performance_monitor.h"
#include "kcenon/monitoring/health/health_monitor.h"
#include "kcenon/monitoring/storage/storage_backends.h"

using namespace kcenon::monitoring;
using namespace std::chrono_literals;

// Global shutdown flag for graceful shutdown
std::atomic<bool> shutdown_requested{false};

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", initiating graceful shutdown..." << std::endl;
    shutdown_requested = true;
}

// Custom health check for database simulation
class database_health_check : public health_check {
private:
    std::string name_;

public:
    explicit database_health_check(const std::string& name) : name_(name) {}

    std::string get_name() const override {
        return name_;
    }

    health_check_type get_type() const override {
        return health_check_type::readiness;
    }

    health_check_result check() override {
        // Simulate database connectivity check
        return health_check_result::healthy("Database connection pool active");
    }

    bool is_critical() const override {
        return true;
    }
};

// Custom health check for external API
class external_api_health_check : public health_check {
private:
    std::string name_;

public:
    explicit external_api_health_check(const std::string& name) : name_(name) {}

    std::string get_name() const override {
        return name_;
    }

    health_check_type get_type() const override {
        return health_check_type::readiness;
    }

    health_check_result check() override {
        // Simulate external API health check
        return health_check_result::healthy("External API responding");
    }

    bool is_critical() const override {
        return false;
    }
};

int main() {
    std::cout << "=== Production Monitoring Stack Example ===" << std::endl;
    std::cout << std::endl;

    try {
        // =====================================================================
        // Section 1: Configuration Management
        // =====================================================================
        std::cout << "1. Configuring Production Monitoring Stack" << std::endl;
        std::cout << "   =========================================" << std::endl;
        std::cout << std::endl;

        // Configure performance monitoring
        monitoring_config perf_config;
        perf_config.history_size = 10000;
        perf_config.collection_interval = 5000ms;
        perf_config.enable_compression = true;

        std::cout << "   Performance Monitor:" << std::endl;
        std::cout << "   - History size: " << perf_config.history_size << std::endl;
        std::cout << "   - Collection interval: 5s" << std::endl;
        std::cout << std::endl;

        // Configure alert manager
        alert_manager_config alert_config;
        alert_config.default_evaluation_interval = 10000ms;
        alert_config.default_repeat_interval = 300000ms;
        alert_config.enable_grouping = true;

        std::cout << "   Alert Manager:" << std::endl;
        std::cout << "   - Evaluation interval: 10s" << std::endl;
        std::cout << "   - Grouping: enabled" << std::endl;
        std::cout << std::endl;

        // Configure health monitoring
        health_monitor_config health_config;
        health_config.check_interval = 5000ms;
        health_config.enable_auto_recovery = true;

        std::cout << "   Health Monitor:" << std::endl;
        std::cout << "   - Check interval: 5s" << std::endl;
        std::cout << "   - Auto-recovery: enabled" << std::endl;
        std::cout << std::endl;

        // =====================================================================
        // Section 2: Initialize Monitoring Components
        // =====================================================================
        std::cout << "2. Initializing Components" << std::endl;
        std::cout << "   =======================  " << std::endl;
        std::cout << std::endl;

        // Initialize performance monitor
        performance_monitor perf_monitor("production_monitor");
        if (auto result = perf_monitor.initialize(); result.is_err()) {
            std::cerr << "Failed to initialize performance monitor" << std::endl;
            return 1;
        }
        std::cout << "   [OK] Performance monitor" << std::endl;

        // Initialize health monitor
        health_monitor health_mon(health_config);
        std::cout << "   [OK] Health monitor" << std::endl;

        // Initialize alert manager
        alert_manager alert_mgr(alert_config);
        std::cout << "   [OK] Alert manager" << std::endl;
        std::cout << std::endl;

        // =====================================================================
        // Section 3: Configure Storage Backend
        // =====================================================================
        std::cout << "3. Configuring Storage" << std::endl;
        std::cout << "   ====================" << std::endl;
        std::cout << std::endl;

        storage_config storage_cfg;
        storage_cfg.type = storage_backend_type::file_json;
        storage_cfg.path = "production_metrics.json";

        auto storage = std::make_unique<file_storage_backend>(storage_cfg);
        std::cout << "   [OK] JSON file storage configured" << std::endl;
        std::cout << std::endl;

        // =====================================================================
        // Section 4: Register Health Checks
        // =====================================================================
        std::cout << "4. Registering Health Checks" << std::endl;
        std::cout << "   ===========================" << std::endl;
        std::cout << std::endl;

        auto db_check = std::make_shared<database_health_check>("database");
        health_mon.register_check("database", db_check);
        std::cout << "   [OK] Database health check" << std::endl;

        auto api_check = std::make_shared<external_api_health_check>("external_api");
        health_mon.register_check("external_api", api_check);
        std::cout << "   [OK] External API health check" << std::endl;
        std::cout << std::endl;

        // =====================================================================
        // Section 5: Configure Alert Rules
        // =====================================================================
        std::cout << "5. Configuring Alert Rules" << std::endl;
        std::cout << "   ========================" << std::endl;
        std::cout << std::endl;

        auto cpu_rule = std::make_shared<alert_rule>("high_cpu_usage");
        cpu_rule->set_metric_name("cpu_usage")
                .set_severity(alert_severity::warning)
                .set_summary("CPU usage exceeds 80%")
                .set_trigger(threshold_trigger::above(80.0));

        alert_mgr.add_rule(cpu_rule);
        std::cout << "   [OK] CPU usage alert rule" << std::endl;

        auto log_notifier_ptr = std::make_shared<log_notifier>("console_logger");
        alert_mgr.add_notifier(log_notifier_ptr);
        std::cout << "   [OK] Console notifier" << std::endl;
        std::cout << std::endl;

        // =====================================================================
        // Section 6: Start Monitoring
        // =====================================================================
        std::cout << "6. Starting Monitoring" << std::endl;
        std::cout << "   ====================" << std::endl;
        std::cout << std::endl;

        health_mon.start();
        std::cout << "   [OK] Health monitor started" << std::endl;

        alert_mgr.start();
        std::cout << "   [OK] Alert manager started" << std::endl;
        std::cout << std::endl;

        // Install signal handlers
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        std::cout << "7. Monitoring Active (Ctrl+C to shutdown)" << std::endl;
        std::cout << "   ========================================" << std::endl;
        std::cout << std::endl;

        // =====================================================================
        // Section 7: Workload Simulation
        // =====================================================================
        int iteration = 0;
        while (!shutdown_requested && iteration < 10) {
            std::cout << "   Iteration " << (iteration + 1) << "/10" << std::endl;

            // Collect performance metrics
            {
                auto timer = perf_monitor.time_operation("iteration_" + std::to_string(iteration));
                std::this_thread::sleep_for(200ms);
            }

            // Check health
            auto health_result = health_mon.check_health();
            if (health_result.status == health_status::healthy) {
                std::cout << "   Health: Healthy" << std::endl;
            }

            // Get system metrics
            auto system_metrics = perf_monitor.get_system_monitor().get_current_metrics();
            if (system_metrics.is_ok()) {
                const auto& metrics = system_metrics.value();
                std::cout << "   CPU: " << metrics.cpu_usage_percent
                         << "%, Memory: " << (metrics.memory_usage_bytes / (1024.0 * 1024.0))
                         << " MB" << std::endl;

                // Process metrics for alerting
                alert_mgr.process_metric("cpu_usage", metrics.cpu_usage_percent);
            }

            std::cout << std::endl;
            std::this_thread::sleep_for(2s);
            iteration++;
        }

        // =====================================================================
        // Section 8: Graceful Shutdown
        // =====================================================================
        std::cout << "8. Graceful Shutdown" << std::endl;
        std::cout << "   ==================" << std::endl;
        std::cout << std::endl;

        alert_mgr.stop();
        std::cout << "   [OK] Alert manager stopped" << std::endl;

        health_mon.stop();
        std::cout << "   [OK] Health monitor stopped" << std::endl;

        perf_monitor.cleanup();
        std::cout << "   [OK] Performance monitor cleaned up" << std::endl;

        storage->flush();
        std::cout << "   [OK] Storage flushed" << std::endl;
        std::cout << std::endl;

        std::cout << "=== Production Monitoring Completed Successfully ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
