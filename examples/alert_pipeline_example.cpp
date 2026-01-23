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
 * @file alert_pipeline_example.cpp
 * @brief Comprehensive example demonstrating AlertManager, AlertPipeline,
 *        and alert lifecycle management
 *
 * This example demonstrates:
 * - AlertManager initialization and configuration
 * - AlertRule creation with conditions
 * - AlertPipeline setup with evaluation loop
 * - Alert state transitions (pending -> firing -> resolved)
 * - Alert grouping and deduplication
 * - Cooldown and repeat interval configuration
 */

#include <chrono>
#include <iostream>
#include <thread>

#include "kcenon/monitoring/alert/alert_manager.h"
#include "kcenon/monitoring/alert/alert_pipeline.h"
#include "kcenon/monitoring/alert/alert_triggers.h"

using namespace kcenon::monitoring;
using namespace std::chrono_literals;

// Helper function to print alert state
void print_alert_state(const alert& a) {
    std::cout << "  Alert: " << a.name
              << " | State: " << alert_state_to_string(a.state)
              << " | Severity: " << alert_severity_to_string(a.severity)
              << " | Value: " << a.value << std::endl;
}

// Helper function to print all active alerts
void print_active_alerts(const alert_manager& manager) {
    auto alerts = manager.get_active_alerts();
    std::cout << "Active alerts (" << alerts.size() << "):" << std::endl;
    for (const auto& a : alerts) {
        print_alert_state(a);
    }
    if (alerts.empty()) {
        std::cout << "  (none)" << std::endl;
    }
}

int main() {
    std::cout << "=== Alert Pipeline Example ===" << std::endl;
    std::cout << std::endl;

    // =========================================================================
    // Section 1: AlertManager Configuration
    // =========================================================================
    std::cout << "1. Configuring AlertManager" << std::endl;
    std::cout << "   -------------------------" << std::endl;

    alert_manager_config config;
    config.default_evaluation_interval = 1000ms;  // Evaluate every 1 second
    config.default_repeat_interval = 5000ms;      // Repeat notifications every 5 seconds
    config.max_alerts_per_rule = 100;             // Maximum alerts per rule
    config.enable_grouping = true;                // Enable alert grouping
    config.group_wait = 2000ms;                   // Wait 2 seconds before sending group
    config.group_interval = 3000ms;               // Group batch interval
    config.resolve_timeout = 5000ms;              // Auto-resolve timeout

    std::cout << "   Evaluation interval: 1s" << std::endl;
    std::cout << "   Repeat interval: 5s" << std::endl;
    std::cout << "   Grouping enabled: true" << std::endl;
    std::cout << std::endl;

    // Validate configuration
    if (!config.validate()) {
        std::cerr << "Invalid configuration!" << std::endl;
        return 1;
    }

    // Create alert manager
    alert_manager manager(config);

    // =========================================================================
    // Section 2: Creating Alert Rules
    // =========================================================================
    std::cout << "2. Creating Alert Rules" << std::endl;
    std::cout << "   ---------------------" << std::endl;

    // Rule 1: High CPU usage alert
    auto cpu_rule = std::make_shared<alert_rule>("high_cpu_usage");
    cpu_rule->set_metric_name("cpu_usage")
            .set_severity(alert_severity::critical)
            .set_summary("CPU usage is critically high")
            .set_description("CPU usage exceeded 80% threshold")
            .add_label("team", "infrastructure")
            .add_label("service", "compute")
            .set_evaluation_interval(1000ms)
            .set_for_duration(2000ms)  // Must be above threshold for 2s
            .set_repeat_interval(5000ms)
            .set_trigger(threshold_trigger::above(80.0));

    if (auto result = manager.add_rule(cpu_rule); result.is_err()) {
        std::cerr << "Failed to add CPU rule: " << result.error().message << std::endl;
        return 1;
    }
    std::cout << "   Added rule: high_cpu_usage (threshold > 80%)" << std::endl;

    // Rule 2: Low memory alert
    auto memory_rule = std::make_shared<alert_rule>("low_memory");
    memory_rule->set_metric_name("memory_available")
               .set_severity(alert_severity::warning)
               .set_summary("Available memory is low")
               .set_description("Available memory dropped below 10%")
               .add_label("team", "infrastructure")
               .add_label("service", "memory")
               .set_evaluation_interval(1000ms)
               .set_for_duration(1000ms)
               .set_trigger(threshold_trigger::below(10.0));

    if (auto result = manager.add_rule(memory_rule); result.is_err()) {
        std::cerr << "Failed to add memory rule: " << result.error().message << std::endl;
        return 1;
    }
    std::cout << "   Added rule: low_memory (threshold < 10%)" << std::endl;

    // Rule 3: Disk I/O rule (using rule group)
    auto io_rule_group = std::make_shared<alert_rule_group>("disk_io_group");

    auto disk_read_rule = std::make_shared<alert_rule>("high_disk_read");
    disk_read_rule->set_metric_name("disk_read_iops")
                  .set_severity(alert_severity::warning)
                  .set_summary("Disk read IOPS is high")
                  .add_label("team", "storage")
                  .set_trigger(threshold_trigger::above(1000.0));

    auto disk_write_rule = std::make_shared<alert_rule>("high_disk_write");
    disk_write_rule->set_metric_name("disk_write_iops")
                   .set_severity(alert_severity::warning)
                   .set_summary("Disk write IOPS is high")
                   .add_label("team", "storage")
                   .set_trigger(threshold_trigger::above(500.0));

    io_rule_group->add_rule(disk_read_rule);
    io_rule_group->add_rule(disk_write_rule);
    io_rule_group->set_common_interval(2000ms);  // Common evaluation interval

    if (auto result = manager.add_rule_group(io_rule_group); result.is_err()) {
        std::cerr << "Failed to add IO rule group" << std::endl;
        return 1;
    }
    std::cout << "   Added rule group: disk_io_group (2 rules)" << std::endl;

    // Display all rules
    auto rules = manager.get_rules();
    std::cout << "   Total rules configured: " << rules.size() << std::endl;
    std::cout << std::endl;

    // =========================================================================
    // Section 3: Adding Notifiers
    // =========================================================================
    std::cout << "3. Setting Up Notifiers" << std::endl;
    std::cout << "   ---------------------" << std::endl;

    // Add a log notifier to see alerts in console
    auto log_notifier_ptr = std::make_shared<log_notifier>("console_logger");
    if (auto result = manager.add_notifier(log_notifier_ptr); result.is_err()) {
        std::cerr << "Failed to add log notifier" << std::endl;
        return 1;
    }
    std::cout << "   Added notifier: console_logger (log_notifier)" << std::endl;

    // Add a callback notifier for custom handling
    auto callback_notifier_ptr = std::make_shared<callback_notifier>(
        "custom_handler",
        [](const alert& a) {
            std::cout << "   [CALLBACK] Alert received: " << a.name
                      << " (" << alert_state_to_string(a.state) << ")" << std::endl;
        }
    );
    if (auto result = manager.add_notifier(callback_notifier_ptr); result.is_err()) {
        std::cerr << "Failed to add callback notifier" << std::endl;
        return 1;
    }
    std::cout << "   Added notifier: custom_handler (callback_notifier)" << std::endl;
    std::cout << std::endl;

    // =========================================================================
    // Section 4: Alert Aggregation Configuration
    // =========================================================================
    std::cout << "4. Configuring Alert Aggregator" << std::endl;
    std::cout << "   -----------------------------" << std::endl;

    alert_aggregator_config agg_config;
    agg_config.group_wait = 1000ms;       // Wait 1s before first send
    agg_config.group_interval = 3000ms;   // 3s between group sends
    agg_config.resolve_timeout = 5000ms;  // Remove resolved after 5s
    agg_config.group_by_labels = {"team", "service"};

    alert_aggregator aggregator(agg_config);
    std::cout << "   Group by labels: team, service" << std::endl;
    std::cout << "   Group wait: 1s, interval: 3s" << std::endl;
    std::cout << std::endl;

    // =========================================================================
    // Section 5: Cooldown Tracker Setup
    // =========================================================================
    std::cout << "5. Setting Up Cooldown Tracker" << std::endl;
    std::cout << "   ----------------------------" << std::endl;

    cooldown_tracker cooldown(3000ms);  // Default 3 second cooldown
    std::cout << "   Default cooldown: 3s" << std::endl;

    // Custom cooldown for critical alerts
    cooldown.set_cooldown("high_cpu_usage{}", 1000ms);  // Shorter cooldown for critical
    std::cout << "   Custom cooldown for high_cpu_usage: 1s" << std::endl;
    std::cout << std::endl;

    // =========================================================================
    // Section 6: Alert Deduplication
    // =========================================================================
    std::cout << "6. Setting Up Alert Deduplicator" << std::endl;
    std::cout << "   ------------------------------" << std::endl;

    alert_deduplicator deduplicator(10000ms);  // Cache for 10 seconds
    std::cout << "   Deduplication cache duration: 10s" << std::endl;
    std::cout << std::endl;

    // =========================================================================
    // Section 7: Alert Inhibition Rules
    // =========================================================================
    std::cout << "7. Configuring Alert Inhibition" << std::endl;
    std::cout << "   -----------------------------" << std::endl;

    alert_inhibitor inhibitor;

    // Critical alerts inhibit warning alerts from the same team
    inhibition_rule critical_inhibits_warning;
    critical_inhibits_warning.name = "critical_inhibits_warning";
    critical_inhibits_warning.source_match.set("severity", "critical");
    critical_inhibits_warning.target_match.set("severity", "warning");
    critical_inhibits_warning.equal = {"team"};

    inhibitor.add_rule(critical_inhibits_warning);
    std::cout << "   Added rule: critical alerts inhibit warning alerts (same team)" << std::endl;
    std::cout << std::endl;

    // =========================================================================
    // Section 8: Simulating Alert Lifecycle
    // =========================================================================
    std::cout << "8. Simulating Alert Lifecycle" << std::endl;
    std::cout << "   ---------------------------" << std::endl;

    // Start the alert manager
    if (auto result = manager.start(); result.is_err()) {
        std::cerr << "Failed to start alert manager: " << result.error().message << std::endl;
        return 1;
    }
    std::cout << "   Alert manager started" << std::endl;
    std::cout << std::endl;

    // Simulate metric values over time
    std::cout << "   Simulating metric values..." << std::endl;
    std::cout << std::endl;

    // Phase 1: Normal operation
    std::cout << "   [Phase 1] Normal operation (CPU: 50%, Memory: 80%)" << std::endl;
    manager.process_metric("cpu_usage", 50.0);
    manager.process_metric("memory_available", 80.0);
    print_active_alerts(manager);
    std::this_thread::sleep_for(1500ms);

    // Phase 2: CPU spike - should trigger pending state
    std::cout << std::endl;
    std::cout << "   [Phase 2] CPU spike detected (CPU: 85%)" << std::endl;
    manager.process_metric("cpu_usage", 85.0);
    print_active_alerts(manager);
    std::this_thread::sleep_for(1500ms);

    // Phase 3: CPU still high - should transition to firing
    std::cout << std::endl;
    std::cout << "   [Phase 3] CPU remains high (CPU: 90%)" << std::endl;
    manager.process_metric("cpu_usage", 90.0);
    print_active_alerts(manager);
    std::this_thread::sleep_for(1500ms);

    // Phase 4: Memory drops - additional alert
    std::cout << std::endl;
    std::cout << "   [Phase 4] Memory drops (Memory: 5%)" << std::endl;
    manager.process_metric("memory_available", 5.0);
    print_active_alerts(manager);

    // Check inhibition
    auto active = manager.get_active_alerts();
    if (!active.empty()) {
        for (const auto& a : active) {
            if (inhibitor.is_inhibited(a, active)) {
                std::cout << "   Note: " << a.name << " would be inhibited" << std::endl;
            }
        }
    }
    std::this_thread::sleep_for(1500ms);

    // Phase 5: Resolution - CPU back to normal
    std::cout << std::endl;
    std::cout << "   [Phase 5] CPU normalizes (CPU: 40%)" << std::endl;
    manager.process_metric("cpu_usage", 40.0);
    print_active_alerts(manager);
    std::this_thread::sleep_for(1500ms);

    // Phase 6: Full resolution
    std::cout << std::endl;
    std::cout << "   [Phase 6] Memory recovers (Memory: 50%)" << std::endl;
    manager.process_metric("memory_available", 50.0);
    print_active_alerts(manager);
    std::cout << std::endl;

    // =========================================================================
    // Section 9: Alert Grouping Demonstration
    // =========================================================================
    std::cout << "9. Alert Grouping Demonstration" << std::endl;
    std::cout << "   -----------------------------" << std::endl;

    // Create some alerts for grouping
    alert alert1("cpu_high_server1", alert_labels());
    alert1.labels.set("team", "infrastructure");
    alert1.labels.set("service", "compute");
    alert1.severity = alert_severity::warning;
    alert1.state = alert_state::firing;
    alert1.value = 85.0;

    alert alert2("cpu_high_server2", alert_labels());
    alert2.labels.set("team", "infrastructure");
    alert2.labels.set("service", "compute");
    alert2.severity = alert_severity::warning;
    alert2.state = alert_state::firing;
    alert2.value = 92.0;

    alert alert3("memory_low_server1", alert_labels());
    alert3.labels.set("team", "infrastructure");
    alert3.labels.set("service", "memory");
    alert3.severity = alert_severity::critical;
    alert3.state = alert_state::firing;
    alert3.value = 5.0;

    // Add to aggregator
    std::string group1 = aggregator.add_alert(alert1);
    std::string group2 = aggregator.add_alert(alert2);
    std::string group3 = aggregator.add_alert(alert3);

    std::cout << "   Added 3 alerts to aggregator" << std::endl;
    std::cout << "   Total groups: " << aggregator.group_count() << std::endl;
    std::cout << "   Total alerts: " << aggregator.total_alert_count() << std::endl;

    // Wait for group_wait to elapse
    std::this_thread::sleep_for(1500ms);

    // Get ready groups
    auto ready_groups = aggregator.get_ready_groups();
    std::cout << "   Ready groups: " << ready_groups.size() << std::endl;
    for (const auto& group : ready_groups) {
        std::cout << "   - Group: " << group.group_key
                  << " (alerts: " << group.size()
                  << ", max severity: " << alert_severity_to_string(group.max_severity())
                  << ")" << std::endl;
        aggregator.mark_sent(group.group_key);
    }
    std::cout << std::endl;

    // =========================================================================
    // Section 10: Cooldown and Deduplication Check
    // =========================================================================
    std::cout << "10. Cooldown and Deduplication Check" << std::endl;
    std::cout << "    ----------------------------------" << std::endl;

    std::string test_fingerprint = "test_alert{}";

    // First notification
    if (!cooldown.is_in_cooldown(test_fingerprint)) {
        std::cout << "    First notification sent for: " << test_fingerprint << std::endl;
        cooldown.record_notification(test_fingerprint);
    }

    // Immediate second notification (should be in cooldown)
    if (cooldown.is_in_cooldown(test_fingerprint)) {
        auto remaining = cooldown.remaining_cooldown(test_fingerprint);
        std::cout << "    In cooldown, remaining: "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count()
                  << "ms" << std::endl;
    }

    // Deduplication check
    alert dup_alert("duplicate_test", alert_labels());
    dup_alert.state = alert_state::firing;

    bool is_dup1 = deduplicator.is_duplicate(dup_alert);
    std::cout << "    First occurrence duplicate check: "
              << (is_dup1 ? "duplicate" : "new") << std::endl;

    bool is_dup2 = deduplicator.is_duplicate(dup_alert);
    std::cout << "    Second occurrence duplicate check: "
              << (is_dup2 ? "duplicate" : "new") << std::endl;

    // Change state - should not be duplicate
    dup_alert.state = alert_state::resolved;
    bool is_dup3 = deduplicator.is_duplicate(dup_alert);
    std::cout << "    After state change duplicate check: "
              << (is_dup3 ? "duplicate" : "new") << std::endl;
    std::cout << std::endl;

    // =========================================================================
    // Section 11: Cleanup
    // =========================================================================
    std::cout << "11. Cleanup" << std::endl;
    std::cout << "    -------" << std::endl;

    // Stop the alert manager
    if (auto result = manager.stop(); result.is_err()) {
        std::cerr << "Failed to stop alert manager: " << result.error().message << std::endl;
        return 1;
    }
    std::cout << "    Alert manager stopped" << std::endl;

    // Print final metrics
    auto metrics = manager.get_metrics();
    std::cout << "    Final metrics:" << std::endl;
    std::cout << "      Rules evaluated: " << metrics.rules_evaluated << std::endl;
    std::cout << "      Alerts created: " << metrics.alerts_created << std::endl;
    std::cout << "      Alerts resolved: " << metrics.alerts_resolved << std::endl;
    std::cout << "      Alerts suppressed: " << metrics.alerts_suppressed << std::endl;
    std::cout << "      Notifications sent: " << metrics.notifications_sent << std::endl;
    std::cout << std::endl;

    // Cleanup aggregator
    aggregator.cleanup();
    std::cout << "    Aggregator cleaned up" << std::endl;

    // Reset deduplicator
    deduplicator.reset();
    std::cout << "    Deduplicator reset" << std::endl;

    // Reset cooldown
    cooldown.reset();
    std::cout << "    Cooldown tracker reset" << std::endl;
    std::cout << std::endl;

    std::cout << "=== Alert Pipeline Example Completed ===" << std::endl;

    return 0;
}
