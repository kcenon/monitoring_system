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
 * @file alert_notifiers_example.cpp
 * @brief Comprehensive example demonstrating alert notification implementations
 *
 * This example demonstrates:
 * - WebhookNotifier setup and configuration
 * - LogNotifier for file-based alerts
 * - Custom Notifier interface implementation
 * - Alert routing to multiple notifiers
 * - Error handling for notification failures
 * - Alert formatters (JSON, text)
 * - Buffered and routing notifiers
 */

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include "kcenon/monitoring/alert/alert_notifiers.h"
#include "kcenon/monitoring/alert/alert_types.h"
#include "kcenon/monitoring/core/result_types.h"

using namespace std::chrono_literals;

namespace kcenon::monitoring {

// Helper function to create sample alerts
alert create_sample_alert(const std::string& name,
                          alert_severity severity,
                          alert_state state,
                          double value,
                          const std::string& team = "ops") {
    alert a;
    a.name = name;
    a.severity = severity;
    a.state = state;
    a.value = value;
    a.labels.set("team", team);
    a.labels.set("environment", "production");
    a.annotations.summary = "Alert: " + name;
    a.annotations.description = "Detailed description for " + name;
    a.rule_name = name + "_rule";
    return a;
}

// Custom notifier implementation: Console notifier with color
class console_color_notifier : public alert_notifier {
public:
    explicit console_color_notifier(std::string notifier_name)
        : name_(std::move(notifier_name)) {}

    std::string name() const override { return name_; }

    common::VoidResult notify(const alert& a) override {
        std::string color = get_severity_color(a.severity);
        std::string reset = "\033[0m";

        std::cout << color << "[" << name_ << "] "
                  << alert_severity_to_string(a.severity) << ": "
                  << a.name << " (" << alert_state_to_string(a.state) << ")"
                  << reset << std::endl;
        std::cout << "    Summary: " << a.annotations.summary << std::endl;
        std::cout << "    Value: " << a.value << std::endl;

        return common::ok();
    }

    common::VoidResult notify_group(const alert_group& group) override {
        std::cout << "[" << name_ << "] Alert Group: " << group.group_key
                  << " (" << group.size() << " alerts)" << std::endl;

        for (const auto& alert_item : group.alerts) {
            auto result = notify(alert_item);
            if (!result.is_ok()) {
                return result;
            }
        }
        return common::ok();
    }

    bool is_ready() const override { return true; }

private:
    static std::string get_severity_color(alert_severity sev) {
        switch (sev) {
            case alert_severity::emergency: return "\033[41m\033[37m";  // White on red
            case alert_severity::critical:  return "\033[31m";          // Red
            case alert_severity::warning:   return "\033[33m";          // Yellow
            case alert_severity::info:      return "\033[32m";          // Green
            default:                        return "\033[0m";           // Reset
        }
    }

    std::string name_;
};

// Custom notifier: Statistics collector
class statistics_notifier : public alert_notifier {
public:
    explicit statistics_notifier(std::string notifier_name)
        : name_(std::move(notifier_name)) {}

    std::string name() const override { return name_; }

    common::VoidResult notify(const alert& a) override {
        std::lock_guard<std::mutex> lock(mutex_);
        total_alerts_++;
        severity_counts_[a.severity]++;
        state_counts_[a.state]++;
        return common::ok();
    }

    common::VoidResult notify_group(const alert_group& group) override {
        for (const auto& alert_item : group.alerts) {
            auto result = notify(alert_item);
            if (!result.is_ok()) {
                return result;
            }
        }
        return common::ok();
    }

    bool is_ready() const override { return true; }

    void print_statistics() const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::cout << "Statistics from " << name_ << ":" << std::endl;
        std::cout << "  Total alerts: " << total_alerts_ << std::endl;
        std::cout << "  By severity:" << std::endl;
        for (const auto& [sev, count] : severity_counts_) {
            std::cout << "    " << alert_severity_to_string(sev) << ": " << count << std::endl;
        }
        std::cout << "  By state:" << std::endl;
        for (const auto& [state, count] : state_counts_) {
            std::cout << "    " << alert_state_to_string(state) << ": " << count << std::endl;
        }
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        total_alerts_ = 0;
        severity_counts_.clear();
        state_counts_.clear();
    }

private:
    std::string name_;
    mutable std::mutex mutex_;
    size_t total_alerts_ = 0;
    std::map<alert_severity, size_t> severity_counts_;
    std::map<alert_state, size_t> state_counts_;
};

} // namespace kcenon::monitoring

using namespace kcenon::monitoring;

int main() {
    std::cout << "=== Alert Notifiers Example ===" << std::endl;
    std::cout << std::endl;

    // Temporary directory for file output
    const std::string temp_dir = "/tmp/alert_notifiers_example";
    std::filesystem::create_directories(temp_dir);

    // =========================================================================
    // Section 1: Alert Formatters
    // =========================================================================
    std::cout << "1. Alert Formatters" << std::endl;
    std::cout << "   -----------------" << std::endl;

    // Create sample alert for formatting demo
    alert sample = create_sample_alert(
        "high_cpu_usage",
        alert_severity::critical,
        alert_state::firing,
        95.5
    );

    // JSON formatter
    json_alert_formatter json_fmt;
    std::cout << "   JSON format:" << std::endl;
    std::cout << "   " << json_fmt.format(sample) << std::endl;
    std::cout << std::endl;

    // Text formatter
    text_alert_formatter text_fmt;
    std::cout << "   Text format:" << std::endl;
    std::cout << "   " << text_fmt.format(sample) << std::endl;
    std::cout << std::endl;

    // =========================================================================
    // Section 2: LogNotifier (Built-in)
    // =========================================================================
    std::cout << "2. Log Notifier" << std::endl;
    std::cout << "   -------------" << std::endl;

    auto log_notifier_ptr = std::make_shared<log_notifier>("system_logger");

    std::cout << "   Notifier name: " << log_notifier_ptr->name() << std::endl;
    std::cout << "   Ready: " << (log_notifier_ptr->is_ready() ? "yes" : "no") << std::endl;

    // Send alert to log notifier
    std::cout << "   Sending alert to log notifier..." << std::endl;
    if (auto result = log_notifier_ptr->notify(sample); result.is_ok()) {
        std::cout << "   Alert logged successfully" << std::endl;
    } else {
        std::cout << "   Failed to log alert: " << result.error().message << std::endl;
    }
    std::cout << std::endl;

    // =========================================================================
    // Section 3: FileNotifier
    // =========================================================================
    std::cout << "3. File Notifier" << std::endl;
    std::cout << "   --------------" << std::endl;

    std::string alert_log_path = temp_dir + "/alerts.log";
    auto file_notifier_ptr = std::make_shared<file_notifier>(
        alert_log_path,
        std::make_shared<text_alert_formatter>()
    );

    std::cout << "   Notifier name: " << file_notifier_ptr->name() << std::endl;
    std::cout << "   Output file: " << alert_log_path << std::endl;

    // Create several alerts and write to file
    std::vector<alert> alerts_to_log = {
        create_sample_alert("cpu_high", alert_severity::critical, alert_state::firing, 92.0),
        create_sample_alert("memory_low", alert_severity::warning, alert_state::pending, 15.0),
        create_sample_alert("disk_full", alert_severity::emergency, alert_state::firing, 98.0),
    };

    for (const auto& a : alerts_to_log) {
        if (auto result = file_notifier_ptr->notify(a); !result.is_ok()) {
            std::cout << "   Failed to write alert: " << result.error().message << std::endl;
        }
    }

    std::cout << "   Wrote " << alerts_to_log.size() << " alerts to file" << std::endl;

    // Read back and display file contents
    std::cout << "   File contents:" << std::endl;
    std::ifstream file(alert_log_path);
    std::string line;
    while (std::getline(file, line)) {
        std::cout << "     " << line << std::endl;
    }
    std::cout << std::endl;

    // =========================================================================
    // Section 4: WebhookNotifier Configuration
    // =========================================================================
    std::cout << "4. Webhook Notifier Configuration" << std::endl;
    std::cout << "   -------------------------------" << std::endl;

    webhook_config webhook_cfg;
    webhook_cfg.url = "https://hooks.example.com/alerts";
    webhook_cfg.method = "POST";
    webhook_cfg.timeout = 30000ms;
    webhook_cfg.max_retries = 3;
    webhook_cfg.retry_delay = 1000ms;
    webhook_cfg.send_resolved = true;
    webhook_cfg.content_type = "application/json";
    webhook_cfg.add_header("Authorization", "Bearer token-xxx")
               .add_header("X-Alert-Source", "monitoring-system");

    std::cout << "   URL: " << webhook_cfg.url << std::endl;
    std::cout << "   Method: " << webhook_cfg.method << std::endl;
    std::cout << "   Timeout: " << webhook_cfg.timeout.count() << "ms" << std::endl;
    std::cout << "   Max retries: " << webhook_cfg.max_retries << std::endl;
    std::cout << "   Headers:" << std::endl;
    for (const auto& [key, value] : webhook_cfg.headers) {
        std::cout << "     " << key << ": " << value << std::endl;
    }
    std::cout << std::endl;

    // Create webhook notifier
    auto webhook_notifier_ptr = std::make_shared<webhook_notifier>(
        webhook_cfg,
        std::make_shared<json_alert_formatter>()
    );

    std::cout << "   Notifier name: " << webhook_notifier_ptr->name() << std::endl;
    std::cout << "   Ready: " << (webhook_notifier_ptr->is_ready() ? "yes" : "no")
              << " (no HTTP sender configured)" << std::endl;

    // Configure mock HTTP sender for testing
    int http_call_count = 0;
    webhook_notifier_ptr->set_http_sender(
        [&http_call_count](const std::string& url,
                           const std::string& method,
                           const std::unordered_map<std::string, std::string>& headers,
                           const std::string& body) -> kcenon::common::VoidResult {
            http_call_count++;
            std::cout << "   [MOCK HTTP] " << method << " " << url << std::endl;
            std::cout << "   [MOCK HTTP] Headers: " << headers.size() << std::endl;
            std::cout << "   [MOCK HTTP] Body length: " << body.length() << " chars" << std::endl;
            return kcenon::common::ok();
        }
    );

    std::cout << "   Ready after setting HTTP sender: "
              << (webhook_notifier_ptr->is_ready() ? "yes" : "no") << std::endl;

    // Test webhook notification
    std::cout << "   Testing webhook notification:" << std::endl;
    if (auto result = webhook_notifier_ptr->notify(sample); result.is_ok()) {
        std::cout << "   Webhook notification sent (HTTP calls: " << http_call_count << ")" << std::endl;
    }
    std::cout << std::endl;

    // =========================================================================
    // Section 5: CallbackNotifier
    // =========================================================================
    std::cout << "5. Callback Notifier" << std::endl;
    std::cout << "   ------------------" << std::endl;

    size_t callback_count = 0;
    auto callback_notifier_ptr = std::make_shared<callback_notifier>(
        "custom_callback",
        [&callback_count](const alert& a) {
            callback_count++;
            std::cout << "   [CALLBACK] Received: " << a.name
                      << " (severity: " << alert_severity_to_string(a.severity) << ")"
                      << std::endl;
        },
        [&callback_count](const alert_group& group) {
            callback_count += group.size();
            std::cout << "   [CALLBACK GROUP] Received group: " << group.group_key
                      << " (" << group.size() << " alerts)" << std::endl;
        }
    );

    std::cout << "   Testing callback notifier:" << std::endl;
    callback_notifier_ptr->notify(sample);
    std::cout << "   Callbacks executed: " << callback_count << std::endl;
    std::cout << std::endl;

    // =========================================================================
    // Section 6: MultiNotifier - Multiple Targets
    // =========================================================================
    std::cout << "6. Multi Notifier (Multiple Targets)" << std::endl;
    std::cout << "   -----------------------------------" << std::endl;

    auto multi = std::make_shared<multi_notifier>("multi_channel");

    // Add multiple child notifiers
    auto log_child = std::make_shared<log_notifier>("log_child");
    auto stats_child = std::make_shared<statistics_notifier>("stats_child");
    auto console_child = std::make_shared<console_color_notifier>("console_child");

    multi->add_notifier(log_child);
    multi->add_notifier(stats_child);
    multi->add_notifier(console_child);

    std::cout << "   Added 3 child notifiers to multi_channel" << std::endl;
    std::cout << "   Sending alert to all channels:" << std::endl;

    if (auto result = multi->notify(sample); result.is_ok()) {
        std::cout << "   All notifiers succeeded" << std::endl;
    } else {
        std::cout << "   Some notifiers failed: " << result.error().message << std::endl;
    }
    std::cout << std::endl;

    // =========================================================================
    // Section 7: BufferedNotifier - Batching
    // =========================================================================
    std::cout << "7. Buffered Notifier (Batching)" << std::endl;
    std::cout << "   -----------------------------" << std::endl;

    auto inner_notifier = std::make_shared<statistics_notifier>("buffered_inner");
    auto buffered = std::make_shared<buffered_notifier>(
        inner_notifier,
        5,       // buffer size
        10000ms  // flush interval
    );

    std::cout << "   Buffer size: 5, flush interval: 10s" << std::endl;
    std::cout << "   Sending alerts (will buffer until size reached):" << std::endl;

    for (int i = 1; i <= 7; ++i) {
        auto a = create_sample_alert(
            "buffered_alert_" + std::to_string(i),
            alert_severity::warning,
            alert_state::firing,
            static_cast<double>(i * 10)
        );
        buffered->notify(a);
        std::cout << "   Sent alert " << i << ", pending: " << buffered->pending_count() << std::endl;
    }

    // Force flush remaining
    std::cout << "   Forcing flush of remaining alerts..." << std::endl;
    buffered->flush();
    std::cout << "   Pending after flush: " << buffered->pending_count() << std::endl;

    inner_notifier->print_statistics();
    std::cout << std::endl;

    // =========================================================================
    // Section 8: RoutingNotifier - Conditional Routing
    // =========================================================================
    std::cout << "8. Routing Notifier (Conditional Routing)" << std::endl;
    std::cout << "   ---------------------------------------" << std::endl;

    auto router = std::make_shared<routing_notifier>("alert_router");

    // Create notifiers for different routes
    auto critical_notifier = std::make_shared<console_color_notifier>("critical_channel");
    auto warning_notifier = std::make_shared<console_color_notifier>("warning_channel");
    auto default_notifier = std::make_shared<console_color_notifier>("default_channel");

    // Route by severity
    router->route_by_severity(alert_severity::critical, critical_notifier);
    router->route_by_severity(alert_severity::emergency, critical_notifier);
    router->route_by_severity(alert_severity::warning, warning_notifier);
    router->set_default_route(default_notifier);

    std::cout << "   Routing rules configured:" << std::endl;
    std::cout << "     - critical/emergency -> critical_channel" << std::endl;
    std::cout << "     - warning -> warning_channel" << std::endl;
    std::cout << "     - default -> default_channel" << std::endl;
    std::cout << std::endl;

    std::cout << "   Testing routing with different severities:" << std::endl;

    std::vector<alert> routing_tests = {
        create_sample_alert("critical_alert", alert_severity::critical, alert_state::firing, 99.0),
        create_sample_alert("warning_alert", alert_severity::warning, alert_state::firing, 75.0),
        create_sample_alert("info_alert", alert_severity::info, alert_state::firing, 50.0),
    };

    for (const auto& a : routing_tests) {
        std::cout << "   Routing '" << a.name << "' (severity: "
                  << alert_severity_to_string(a.severity) << "):" << std::endl;
        router->notify(a);
    }
    std::cout << std::endl;

    // Route by label
    std::cout << "   Adding label-based routing:" << std::endl;
    auto ops_notifier = std::make_shared<console_color_notifier>("ops_team_channel");
    router->route_by_label("team", "ops", ops_notifier);

    auto ops_alert = create_sample_alert("ops_alert", alert_severity::info, alert_state::firing, 60.0, "ops");
    std::cout << "   Routing alert with team=ops:" << std::endl;
    router->notify(ops_alert);
    std::cout << std::endl;

    // =========================================================================
    // Section 9: Custom Notifier Implementation
    // =========================================================================
    std::cout << "9. Custom Notifier Implementation" << std::endl;
    std::cout << "   -------------------------------" << std::endl;

    // Statistics notifier demonstration
    auto stats = std::make_shared<statistics_notifier>("alert_statistics");

    // Send various alerts
    std::vector<alert> stat_alerts = {
        create_sample_alert("alert1", alert_severity::critical, alert_state::firing, 90.0),
        create_sample_alert("alert2", alert_severity::warning, alert_state::pending, 70.0),
        create_sample_alert("alert3", alert_severity::critical, alert_state::resolved, 40.0),
        create_sample_alert("alert4", alert_severity::info, alert_state::firing, 50.0),
        create_sample_alert("alert5", alert_severity::warning, alert_state::firing, 65.0),
    };

    for (const auto& a : stat_alerts) {
        stats->notify(a);
    }

    stats->print_statistics();
    std::cout << std::endl;

    // =========================================================================
    // Section 10: Alert Group Notification
    // =========================================================================
    std::cout << "10. Alert Group Notification" << std::endl;
    std::cout << "    -------------------------" << std::endl;

    // Create an alert group
    alert_group group("infrastructure-alerts");
    group.common_labels.set("environment", "production");
    group.common_labels.set("datacenter", "us-west-2");

    group.add_alert(create_sample_alert("cpu_server1", alert_severity::critical, alert_state::firing, 95.0));
    group.add_alert(create_sample_alert("cpu_server2", alert_severity::warning, alert_state::firing, 82.0));
    group.add_alert(create_sample_alert("cpu_server3", alert_severity::critical, alert_state::firing, 91.0));

    std::cout << "   Group: " << group.group_key << std::endl;
    std::cout << "   Alerts: " << group.size() << std::endl;
    std::cout << "   Max severity: " << alert_severity_to_string(group.max_severity()) << std::endl;
    std::cout << std::endl;

    // Test JSON formatter with group
    std::cout << "   JSON formatted group:" << std::endl;
    std::cout << "   " << json_fmt.format_group(group) << std::endl;
    std::cout << std::endl;

    // Send to multiple notifiers
    auto group_stats = std::make_shared<statistics_notifier>("group_stats");
    group_stats->notify_group(group);
    group_stats->print_statistics();
    std::cout << std::endl;

    // =========================================================================
    // Section 11: Error Handling
    // =========================================================================
    std::cout << "11. Error Handling" << std::endl;
    std::cout << "    ---------------" << std::endl;

    // Webhook with failing HTTP sender
    webhook_config fail_cfg;
    fail_cfg.url = "https://failing.example.com/alerts";
    fail_cfg.max_retries = 2;
    fail_cfg.retry_delay = 100ms;

    auto failing_webhook = std::make_shared<webhook_notifier>(fail_cfg);
    int retry_count = 0;

    failing_webhook->set_http_sender(
        [&retry_count](const std::string& /*url*/,
                       const std::string& /*method*/,
                       const std::unordered_map<std::string, std::string>& /*headers*/,
                       const std::string& /*body*/) -> kcenon::common::VoidResult {
            retry_count++;
            std::cout << "    HTTP attempt " << retry_count << " - simulating failure" << std::endl;
            return kcenon::common::VoidResult::err(500, "Simulated server error");
        }
    );

    std::cout << "   Testing webhook with simulated failures:" << std::endl;
    auto fail_result = failing_webhook->notify(sample);
    if (!fail_result.is_ok()) {
        std::cout << "   Expected failure after " << retry_count << " attempts: "
                  << fail_result.error().message << std::endl;
    }
    std::cout << std::endl;

    // =========================================================================
    // Cleanup
    // =========================================================================
    std::cout << "12. Cleanup" << std::endl;
    std::cout << "    -------" << std::endl;

    // Remove temporary files
    std::filesystem::remove_all(temp_dir);
    std::cout << "    Removed temporary directory: " << temp_dir << std::endl;
    std::cout << std::endl;

    std::cout << "=== Alert Notifiers Example Completed ===" << std::endl;
    std::cout << std::endl;
    std::cout << "Notifiers demonstrated:" << std::endl;
    std::cout << "  - LogNotifier (built-in logging)" << std::endl;
    std::cout << "  - FileNotifier (file-based alerts)" << std::endl;
    std::cout << "  - WebhookNotifier (HTTP webhooks)" << std::endl;
    std::cout << "  - CallbackNotifier (custom callbacks)" << std::endl;
    std::cout << "  - MultiNotifier (multiple targets)" << std::endl;
    std::cout << "  - BufferedNotifier (batching)" << std::endl;
    std::cout << "  - RoutingNotifier (conditional routing)" << std::endl;
    std::cout << "  - Custom implementations (color console, statistics)" << std::endl;
    std::cout << "  - Alert formatters (JSON, text)" << std::endl;

    return 0;
}
