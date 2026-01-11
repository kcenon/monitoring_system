# Alert Pipeline Guide

This guide covers the alert pipeline feature, enabling automated alerting based on metric thresholds, patterns, and anomalies.

## Overview

The alert pipeline provides:

- **Threshold-based alerting**: Simple comparison operators (>, >=, <, <=, ==, !=)
- **Rate of change detection**: Alert on rapid metric changes
- **Anomaly detection**: Statistical deviation from normal behavior
- **Alert grouping and deduplication**: Reduce notification noise
- **Flexible notification routing**: Webhook, file, and custom notifiers

## Quick Start

```cpp
#include <kcenon/monitoring/alert/alert_manager.h>
#include <kcenon/monitoring/alert/alert_triggers.h>

using namespace kcenon::monitoring;

// Create alert manager
alert_manager_config config;
config.default_evaluation_interval = std::chrono::seconds(15);
alert_manager manager(config);

// Create a threshold rule
auto rule = std::make_shared<alert_rule>("high_cpu");
rule->set_metric_name("cpu_usage")
    .set_severity(alert_severity::critical)
    .set_summary("CPU usage is high")
    .set_description("CPU usage exceeded threshold")
    .set_for_duration(std::chrono::minutes(1))
    .set_trigger(threshold_trigger::above(80.0));

manager.add_rule(rule);

// Add a notifier
manager.add_notifier(std::make_shared<log_notifier>());

// Start the manager
manager.start();

// Process metrics (typically from a collector)
manager.process_metric("cpu_usage", 85.0);
```

## Trigger Types

### Threshold Trigger

Alert when a value crosses a threshold:

```cpp
// Alert when value > 80
auto trigger = threshold_trigger::above(80.0);

// Alert when value <= 10
auto trigger = threshold_trigger::below_or_equal(10.0);

// Alert when value is in range [20, 80]
auto trigger = threshold_trigger::in_range(20.0, 80.0);

// Alert when value is outside range
auto trigger = threshold_trigger::out_of_range(20.0, 80.0);
```

### Rate of Change Trigger

Alert on rapid metric changes:

```cpp
using direction = rate_of_change_trigger::rate_direction;

// Alert when value increases by more than 20 per minute
auto trigger = std::make_shared<rate_of_change_trigger>(
    20.0,                          // rate threshold
    std::chrono::minutes(1),       // time window
    direction::increasing
);

// Alert on any change rate
auto trigger = std::make_shared<rate_of_change_trigger>(
    50.0,
    std::chrono::seconds(30),
    direction::either
);
```

### Anomaly Trigger

Alert on statistical anomalies:

```cpp
// Alert when value is 3+ standard deviations from mean
auto trigger = std::make_shared<anomaly_trigger>(
    3.0,    // sensitivity (number of std devs)
    100     // window size for baseline
);
```

### Composite Trigger

Combine multiple conditions:

```cpp
auto cpu_high = threshold_trigger::above(80.0);
auto mem_high = threshold_trigger::above(90.0);

// Alert when both conditions are true
auto both = composite_trigger::all_of({cpu_high, mem_high});

// Alert when any condition is true
auto either = composite_trigger::any_of({cpu_high, mem_high});

// Invert a condition
auto cpu_normal = composite_trigger::invert(cpu_high);
```

## Alert States

Alerts follow a state machine:

```
inactive -> pending -> firing -> resolved
                |         |
                v         v
            inactive   pending (condition returns)
```

- **inactive**: Condition not met
- **pending**: Condition met, waiting for `for_duration`
- **firing**: Alert is active, notifications sent
- **resolved**: Condition cleared after firing

## Silencing Alerts

Temporarily suppress notifications:

```cpp
alert_silence silence;
silence.matchers.set("service", "database");
silence.starts_at = std::chrono::steady_clock::now();
silence.ends_at = silence.starts_at + std::chrono::hours(1);
silence.comment = "Maintenance window";
silence.created_by = "admin";

manager.create_silence(silence);
```

## Notification Integrations

### Webhook Notifier

```cpp
webhook_config config;
config.url = "https://hooks.example.com/alerts";
config.add_header("Authorization", "Bearer token");
config.timeout = std::chrono::seconds(30);
config.max_retries = 3;

auto notifier = std::make_shared<webhook_notifier>(config);

// Set HTTP sender (inject your HTTP client)
notifier->set_http_sender([](const std::string& url,
                             const std::string& method,
                             const auto& headers,
                             const std::string& body) {
    // Use your HTTP client here
    return make_void_success();
});

manager.add_notifier(notifier);
```

### File Notifier

```cpp
auto notifier = std::make_shared<file_notifier>("/var/log/alerts.log");
manager.add_notifier(notifier);
```

### Routing Notifier

Route alerts to different targets based on criteria:

```cpp
auto router = std::make_shared<routing_notifier>("alert_router");

// Critical alerts to PagerDuty
router->route_by_severity(alert_severity::critical, pagerduty_notifier);

// Warning alerts to Slack
router->route_by_severity(alert_severity::warning, slack_notifier);

// Default to email
router->set_default_route(email_notifier);

manager.add_notifier(router);
```

## Configuration from YAML

Define rules in configuration files:

```yaml
rules:
  - name: high_cpu
    group: system
    metric_name: cpu_usage
    severity: critical
    trigger:
      type: threshold
      operator: ">"
      threshold: 80
    evaluation_interval_seconds: 15
    for_duration_seconds: 60
    labels:
      team: infrastructure
    summary: "High CPU usage on ${labels.host}"
    runbook_url: "https://runbooks.example.com/high-cpu"

  - name: memory_anomaly
    group: system
    metric_name: memory_usage
    severity: warning
    trigger:
      type: anomaly
      sensitivity: 3.0
      window_seconds: 300
    summary: "Unusual memory usage pattern"
```

Load configuration:

```cpp
std::vector<rule_definition> definitions;
// Parse YAML into definitions...

rule_registry registry;
auto result = registry.load_definitions(definitions);
if (result.is_ok()) {
    std::cout << "Loaded " << result.value() << " rules\n";
}
```

## Message Templating

Use variables in alert messages:

```cpp
alert_template tmpl("CPU ${value}% on ${labels.host} (threshold: ${threshold})");
tmpl.set("threshold", "80");

std::string message = tmpl.render(alert);
// Output: "CPU 95% on server01 (threshold: 80)"
```

Available variables:
- `${name}` - Alert name
- `${state}` - Alert state
- `${severity}` - Alert severity
- `${value}` - Current metric value
- `${fingerprint}` - Alert fingerprint
- `${labels.X}` - Label value
- `${annotations.X}` - Annotation value

## Alert Aggregation

Group related alerts to reduce noise:

```cpp
alert_aggregator_config config;
config.group_by_labels = {"service", "environment"};
config.group_wait = std::chrono::seconds(30);
config.group_interval = std::chrono::minutes(5);

alert_aggregator aggregator(config);

// Add alerts
aggregator.add_alert(alert1);
aggregator.add_alert(alert2);

// Get ready groups for notification
auto groups = aggregator.get_ready_groups();
for (auto& group : groups) {
    notifier->notify_group(group);
    aggregator.mark_sent(group.group_key);
}
```

## Best Practices

1. **Start simple**: Begin with basic threshold triggers before using complex patterns
2. **Use for_duration**: Avoid flapping by requiring conditions to persist
3. **Group related alerts**: Reduce notification noise with aggregation
4. **Add runbook URLs**: Help responders with clear documentation links
5. **Test alert rules**: Verify triggers work as expected before production
6. **Monitor the alerting system**: Track notification failures and latency

## See Also

- [API Reference](../API_REFERENCE.md)
- [Architecture Guide](../ARCHITECTURE.md)
- [Performance Tuning](../performance/TUNING.md)
