# Advanced Alert Configuration Guide

> **Version**: 0.1.0
> **Module**: `kcenon::monitoring::alert`
> **Audience**: Developers integrating the alerting framework into monitoring pipelines

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Core Types and State Machine](#2-core-types-and-state-machine)
3. [Alert Rule Configuration](#3-alert-rule-configuration)
4. [Trigger Types](#4-trigger-types)
5. [Alert Pipeline](#5-alert-pipeline)
6. [Notification Channels](#6-notification-channels)
7. [Configuration and Templates](#7-configuration-and-templates)
8. [Alert Manager](#8-alert-manager)
9. [Production Examples](#9-production-examples)
10. [Performance Considerations](#10-performance-considerations)
11. [API Reference](#11-api-reference)

---

## 1. Architecture Overview

The alerting framework provides a complete pipeline from metric evaluation to notification delivery:

```
┌─────────────────────────────────────────────────────────────────────┐
│                        alert_manager                                │
│  ┌──────────┐    ┌──────────┐    ┌──────────────┐    ┌───────────┐ │
│  │  Rules    │───▶│ Triggers │───▶│   Pipeline   │───▶│ Notifiers │ │
│  │ Registry  │    │ Evaluate │    │  (stages)    │    │           │ │
│  └──────────┘    └──────────┘    └──────────────┘    └───────────┘ │
│       │                               │                     │       │
│       │          ┌────────────────────┼─────────────────┐  │       │
│       │          │    Pipeline Stages │                  │  │       │
│       │          │  ┌─────────────┐   │  ┌───────────┐  │  │       │
│       │          │  │ Aggregator  │   │  │ Inhibitor │  │  │       │
│       │          │  └──────┬──────┘   │  └─────┬─────┘  │  │       │
│       │          │  ┌──────▼──────┐   │  ┌─────▼─────┐  │  │       │
│       │          │  │Deduplicator │   │  │ Cooldown  │  │  │       │
│       │          │  └─────────────┘   │  │ Tracker   │  │  │       │
│       │          │                    │  └───────────┘  │  │       │
│       │          └────────────────────┴─────────────────┘  │       │
│  ┌────▼─────┐                                         ┌────▼────┐  │
│  │ Silences │                                         │ Metrics │  │
│  └──────────┘                                         └─────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

### Key Components

| Component | Header | Purpose |
|-----------|--------|---------|
| Core types | `alert_types.h` | Severity, state machine, labels, annotations |
| Rules | `alert_rule.h` | Rule definition with fluent builder API |
| Triggers | `alert_triggers.h` | 7 evaluation strategies |
| Config | `alert_config.h` | Templates, YAML definitions, registry |
| Notifiers | `alert_notifiers.h` | 6 notification channel types |
| Pipeline | `alert_pipeline.h` | Aggregation, inhibition, dedup, cooldown |
| Manager | `alert_manager.h` | Central coordinator and lifecycle manager |

### Include Structure

```cpp
// Public API headers
#include <kcenon/monitoring/alert/alert_manager.h>    // Central coordinator
#include <kcenon/monitoring/alert/alert_rule.h>        // Rule definitions
#include <kcenon/monitoring/alert/alert_triggers.h>    // Trigger types
#include <kcenon/monitoring/alert/alert_config.h>      // Configuration
#include <kcenon/monitoring/alert/alert_notifiers.h>   // Notification channels
#include <kcenon/monitoring/alert/alert_pipeline.h>    // Pipeline stages
#include <kcenon/monitoring/alert/alert_types.h>       // Core types
```

---

## 2. Core Types and State Machine

### Alert Severity Levels

Alerts are classified by four severity levels, listed in ascending order of urgency:

```cpp
enum class alert_severity {
    info,       // Informational — no action required
    warning,    // Attention needed — potential issue developing
    critical,   // Immediate action — service degradation occurring
    emergency   // System-wide failure — page on-call immediately
};
```

**Comparison support**: Severity values support `<`, `>`, `<=`, `>=` for priority comparison. Higher severity compares greater:

```cpp
if (alert.sev >= alert_severity::critical) {
    // Page the on-call engineer
}
```

**String conversion**: Use `severity_to_string()` and `severity_from_string()` for serialization:

```cpp
auto str = severity_to_string(alert_severity::critical);  // "critical"
auto sev = severity_from_string("warning");                // alert_severity::warning
```

### Alert State Machine

Alerts transition through a well-defined state machine:

```
                    ┌──────────────┐
                    │   inactive   │  (initial state)
                    └──────┬───────┘
                           │ trigger evaluates true
                           ▼
                    ┌──────────────┐
            ┌──────│   pending    │──────┐
            │      └──────┬───────┘      │
            │             │ for_duration  │ trigger evaluates false
            │             │ elapsed       │
            │             ▼              ▼
            │      ┌──────────────┐   ┌──────────────┐
  silence───┼─────▶│   firing     │   │   resolved   │
  applied   │      └──────┬───────┘   └──────────────┘
            │             │ trigger evaluates false
            │             ▼
            │      ┌──────────────┐
            │      │   resolved   │
            │      └──────────────┘
            ▼
     ┌──────────────┐
     │  suppressed  │  (from any state via silence)
     └──────────────┘
```

```cpp
enum class alert_state {
    inactive,     // Not triggered — initial state
    pending,      // Triggered but waiting for for_duration
    firing,       // Actively firing — notifications sent
    resolved,     // Was firing, now resolved
    suppressed    // Silenced by a silence rule
};
```

**State transitions**: Use `transition_to()` on alert objects — it validates transitions and records timestamps:

```cpp
alert a;
a.transition_to(alert_state::pending);  // Records pending timestamp
a.transition_to(alert_state::firing);   // Records firing timestamp
a.transition_to(alert_state::resolved); // Records resolved timestamp
```

### Alert Labels and Annotations

**Labels** are key-value pairs used for routing, grouping, and deduplication:

```cpp
alert_labels labels;
labels.set("service", "payment-api");
labels.set("instance", "prod-01");
labels.set("severity", "critical");

// Fingerprint for deduplication — deterministic hash of sorted labels
uint64_t fp = labels.fingerprint();

// Check if labels match
bool matches = labels.matches({{"service", "payment-api"}});  // true
```

**Annotations** carry human-readable context that does not affect routing:

```cpp
alert_annotations annotations;
annotations.summary = "High CPU usage on payment-api";
annotations.description = "CPU utilization exceeded 90% for 5 minutes";
annotations.runbook_url = "https://runbooks.internal/high-cpu";
annotations.custom["dashboard"] = "https://grafana.internal/d/cpu";
```

### Alert Groups and Silences

**Alert groups** batch related alerts for grouped notification:

```cpp
alert_group group("payment-alerts");
group.add(alert1);
group.add(alert2);

auto max_sev = group.max_severity();  // Highest severity in group
auto count = group.size();
```

**Silences** suppress alerts matching specific label criteria:

```cpp
alert_silence silence;
silence.matchers = {{"service", "payment-api"}, {"instance", "prod-01"}};
silence.starts_at = std::chrono::system_clock::now();
silence.ends_at = silence.starts_at + std::chrono::hours(2);
silence.created_by = "oncall-engineer";
silence.comment = "Maintenance window for prod-01";

// Check if a silence applies
bool active = silence.is_active();             // Within time window?
bool applies = silence.matches(alert.labels);  // Labels match?
```

---

## 3. Alert Rule Configuration

### Rule Config

Each rule has timing configuration that controls evaluation and notification behavior:

```cpp
struct alert_rule_config {
    std::chrono::seconds evaluation_interval{15};  // How often to evaluate
    std::chrono::seconds for_duration{0};          // Time trigger must hold
    std::chrono::seconds repeat_interval{300};     // Re-notification interval
    std::chrono::seconds keep_firing_for{0};       // Grace period after resolve
};
```

| Parameter | Default | Purpose |
|-----------|---------|---------|
| `evaluation_interval` | 15s | How often the rule's trigger is checked |
| `for_duration` | 0s | Trigger must be true for this duration before firing |
| `repeat_interval` | 5min | Minimum interval between repeated notifications |
| `keep_firing_for` | 0s | Continue firing state after trigger clears |

### Fluent Builder API

Rules are constructed with a chainable fluent API:

```cpp
alert_rule rule("HighCPU");
rule.set_severity(alert_severity::critical)
    .set_metric_name("cpu.usage")
    .set_trigger(threshold_trigger::above(90.0))
    .add_label("service", "api-gateway")
    .add_label("team", "infrastructure")
    .set_for_duration(std::chrono::seconds(300))
    .set_evaluation_interval(std::chrono::seconds(15))
    .set_repeat_interval(std::chrono::seconds(600));
```

### Rule Groups

Group related rules with shared configuration:

```cpp
alert_rule_group group("database-alerts");
group.set_common_interval(std::chrono::seconds(30));

group.add_rule(std::move(high_connections_rule));
group.add_rule(std::move(slow_queries_rule));
group.add_rule(std::move(replication_lag_rule));

// All rules in the group inherit the 30s evaluation interval
for (auto& rule : group.get_rules()) {
    // rule.get_config().evaluation_interval == 30s
}
```

---

## 4. Trigger Types

The framework provides 7 trigger types for different detection strategies.

### 4.1 Threshold Trigger

Simple comparison against a fixed value. Most common trigger type.

```cpp
// Factory methods for common patterns
auto cpu_high   = threshold_trigger::above(90.0);          // > 90
auto cpu_low    = threshold_trigger::below(10.0);          // < 10
auto mem_high   = threshold_trigger::above_or_equal(95.0); // >= 95
auto disk_low   = threshold_trigger::below_or_equal(5.0);  // <= 5

// Custom comparison operator
threshold_trigger custom(75.0, comparison_operator::greater_than);
```

**Comparison operators**:

| Operator | Enum | Example |
|----------|------|---------|
| `>` | `greater_than` | Value exceeds threshold |
| `>=` | `greater_than_or_equal` | Value at or above threshold |
| `<` | `less_than` | Value below threshold |
| `<=` | `less_than_or_equal` | Value at or below threshold |
| `==` | `equal` | Value equals threshold exactly |
| `!=` | `not_equal` | Value differs from threshold |

### 4.2 Range Trigger

Checks whether a value falls inside or outside a defined range:

```cpp
// In-range: alert when value is between 80 and 100
auto in_range = threshold_trigger::in_range(80.0, 100.0);

// Out-of-range: alert when value is outside 20-80
auto out_range = threshold_trigger::out_of_range(20.0, 80.0);

// Explicit range_trigger construction
range_trigger healthy_range(20.0, 80.0, /*inside=*/false);  // Alert outside range
```

### 4.3 Rate of Change Trigger

Detects rapid changes using linear regression over a sliding window:

```cpp
// Alert when value increases faster than 10 units/second
rate_of_change_trigger rising(10.0, rate_direction::increasing);

// Alert when value decreases faster than 5 units/second
rate_of_change_trigger falling(5.0, rate_direction::decreasing);

// Alert on any rapid change (either direction)
rate_of_change_trigger volatile_metric(20.0, rate_direction::either);
```

**Configuration parameters**:

| Parameter | Purpose |
|-----------|---------|
| `threshold` | Rate threshold in units/second |
| `direction` | `increasing`, `decreasing`, or `either` |
| `window` | Time window for regression (default: sliding) |
| `min_samples` | Minimum data points before evaluation |

**How it works**: The trigger maintains a sliding window of `(timestamp, value)` pairs. At evaluation time, it computes linear regression to determine the rate of change (slope). The alert fires when the absolute rate exceeds the threshold in the configured direction.

```
Rate Calculation (Linear Regression):

  slope = Σ((tᵢ - t̄)(vᵢ - v̄)) / Σ((tᵢ - t̄)²)

  where tᵢ = timestamp, vᵢ = value, t̄/v̄ = means
```

### 4.4 Anomaly Trigger

Statistical anomaly detection using z-score with a sliding window:

```cpp
// Alert when value is > 3 standard deviations from mean
anomaly_trigger anomaly(/*sensitivity=*/3.0);

// Custom window size and minimum samples
anomaly_trigger custom_anomaly(2.5);  // 2.5 std devs = more sensitive
```

| Parameter | Purpose | Default |
|-----------|---------|---------|
| `sensitivity` | Number of standard deviations for threshold | 3.0 |
| `window_size` | Number of samples in sliding window | — |
| `min_samples` | Minimum samples before evaluation begins | — |

**How it works**: The trigger maintains a sliding window of recent values and computes mean (μ) and standard deviation (σ). A new value is anomalous if:

```
|value - μ| > sensitivity × σ
```

This approach adapts automatically to the metric's baseline behavior — no need to set fixed thresholds for metrics with varying baselines.

### 4.5 Composite Trigger

Combines multiple triggers with boolean logic:

```cpp
// AND — all conditions must be true
auto all = composite_trigger::all_of({
    threshold_trigger::above(80.0),       // CPU > 80%
    threshold_trigger::above(70.0)        // AND memory > 70%
});

// OR — any condition fires the alert
auto any = composite_trigger::any_of({
    threshold_trigger::above(95.0),       // CPU > 95%
    rate_of_change_trigger(50.0, rate_direction::increasing)  // OR rapid spike
});

// NOT — invert a trigger
auto not_healthy = composite_trigger::invert(
    threshold_trigger::in_range(20.0, 80.0)  // NOT in healthy range
);
```

**Supported operations**:

| Operation | Factory | Behavior |
|-----------|---------|----------|
| AND | `all_of()` | All sub-triggers must evaluate true |
| OR | `any_of()` | At least one sub-trigger must evaluate true |
| XOR | — | Exactly one sub-trigger must evaluate true |
| NOT | `invert()` | Inverts a single trigger's result |

**Multi-metric evaluation**: Composite triggers support `evaluate_multi()` for evaluating across multiple metric values simultaneously.

### 4.6 Absent Trigger

Fires when no data has been received for a specified duration:

```cpp
// Alert if no data received for 5 minutes
absent_trigger no_data(std::chrono::minutes(5));
```

This trigger is essential for detecting:
- Service crashes (metric stops reporting)
- Network partitions (data pipeline interrupted)
- Collection failures (agent disconnected)

### 4.7 Delta Trigger

Fires when the metric value changes from its previous reading:

```cpp
// Alert on any change > 10 units from previous value
delta_trigger big_change(10.0, /*absolute=*/true);

// Alert on signed change (only positive changes)
delta_trigger increase_only(5.0, /*absolute=*/false);
```

| Mode | Behavior |
|------|----------|
| Absolute | Fires if `|current - previous| > threshold` |
| Signed | Fires if `current - previous > threshold` |

---

## 5. Alert Pipeline

The pipeline processes alerts through sequential stages before notification delivery.

### Pipeline Architecture

```
Alert ──▶ Aggregator ──▶ Deduplicator ──▶ Inhibitor ──▶ Cooldown ──▶ Notifier
             │                │                │            │
             │ Group by       │ Fingerprint    │ Suppress   │ Rate
             │ labels         │ match          │ targets    │ limit
             ▼                ▼                ▼            ▼
          alert_group    state-change      dropped     delayed
```

### 5.1 Alert Aggregator

Groups related alerts by label values and batches them for notification:

```cpp
alert_aggregator_config agg_config;
agg_config.group_wait     = std::chrono::seconds(30);   // Wait before first send
agg_config.group_interval = std::chrono::minutes(5);    // Interval between group sends
agg_config.resolve_timeout = std::chrono::minutes(5);   // Auto-resolve timeout
agg_config.group_by_labels = {"service", "instance"};   // Grouping labels

alert_aggregator aggregator(agg_config);

// Add alerts — grouped by service + instance labels
aggregator.add_alert(alert1);  // service=api, instance=prod-01
aggregator.add_alert(alert2);  // service=api, instance=prod-01 → same group
aggregator.add_alert(alert3);  // service=db, instance=prod-02  → different group

// Get groups ready for notification
auto ready = aggregator.get_ready_groups();
for (auto& group : ready) {
    notifier->notify_group(group);
    aggregator.mark_sent(group.name());
}

// Periodic cleanup of resolved groups
aggregator.cleanup();
```

**Timing parameters explained**:

| Parameter | Default | Purpose |
|-----------|---------|---------|
| `group_wait` | 30s | Buffer time for initial alert grouping |
| `group_interval` | 5min | Minimum time between sending the same group |
| `resolve_timeout` | 5min | Auto-resolve after no updates for this duration |
| `group_by_labels` | — | Labels used to determine alert grouping |

### 5.2 Alert Deduplicator

Prevents duplicate notifications using fingerprint-based tracking:

```cpp
alert_deduplicator dedup;

// Returns true for new alerts or state changes
bool is_new = dedup.is_new(alert);  // true — first occurrence
bool is_dup = dedup.is_new(alert);  // false — same fingerprint, same state

// State changes are NOT duplicates
alert.transition_to(alert_state::resolved);
bool changed = dedup.is_new(alert); // true — state changed
```

The deduplicator uses `alert_labels::fingerprint()` combined with `alert_state` for tracking. An alert is considered a duplicate only if both fingerprint and state match a previously seen alert within `cache_duration`.

### 5.3 Alert Inhibitor

Suppresses lower-priority alerts when higher-priority alerts are already firing:

```cpp
inhibition_rule rule;
rule.source_match = {{"severity", "critical"}};   // When critical fires...
rule.target_match = {{"severity", "warning"}};     // ...suppress warnings...
rule.equal = {"service", "instance"};              // ...for same service+instance

alert_inhibitor inhibitor;
inhibitor.add_rule(rule);

// Check if an alert should be suppressed
std::vector<alert> active_alerts = get_firing_alerts();
bool suppressed = inhibitor.is_inhibited(target_alert, active_alerts);
```

**How inhibition works**:

1. For each inhibition rule, find `source` alerts matching `source_match` labels
2. For each matching source, check if source and target share the same values for `equal` labels
3. If match found, the target alert is inhibited (suppressed)

**Example**: When `HostDown (critical)` fires for `instance=prod-01`, suppress `HighCPU (warning)` and `DiskFull (warning)` for the same instance — the root cause is already alerted.

### 5.4 Cooldown Tracker

Rate-limits notifications per alert to prevent alert storms:

```cpp
cooldown_tracker cooldown(std::chrono::minutes(5));  // 5 minute default cooldown

// Check if enough time has elapsed since last notification
bool can_notify = cooldown.can_notify(alert_id);

if (can_notify) {
    notifier->notify(alert);
    cooldown.record_notification(alert_id);
}

// Custom cooldown for specific alerts
cooldown.set_custom_cooldown(alert_id, std::chrono::minutes(15));

// Check remaining cooldown time
auto remaining = cooldown.remaining_cooldown(alert_id);
```

### 5.5 Custom Pipeline Stages

Create custom stages by extending `pipeline_stage`:

```cpp
class custom_enrichment_stage : public pipeline_stage {
public:
    bool process(alert& a) override {
        // Add metadata from external source
        a.annotations.custom["team"] = lookup_team(a.labels.get("service"));
        a.annotations.custom["escalation"] = lookup_escalation_policy(a);
        return true;  // Continue to next stage
    }

    std::string name() const override { return "enrichment"; }
};
```

**Return value**: `process()` returns `true` to continue the pipeline, `false` to stop (drop the alert).

### 5.6 Building the Pipeline

Chain stages together using `alert_pipeline`:

```cpp
alert_pipeline pipeline;

// Add stages in processing order
pipeline.add_stage(std::make_unique<dedup_stage>());
pipeline.add_stage(std::make_unique<inhibition_stage>());
pipeline.add_stage(std::make_unique<cooldown_stage>());
pipeline.add_stage(std::make_unique<custom_enrichment_stage>());
pipeline.add_stage(std::make_unique<notification_stage>());

// Process an alert through the entire pipeline
bool delivered = pipeline.process(alert);
// false means alert was dropped by some stage
```

---

## 6. Notification Channels

### 6.1 Log Notifier

Simple logging — useful for development and debugging:

```cpp
log_notifier logger;
logger.notify(alert);
// Output: [ALERT] critical - HighCPU: CPU usage > 90% (service=api, instance=prod-01)
```

### 6.2 Callback Notifier

Invoke custom functions on alert delivery:

```cpp
callback_notifier cb([](const alert& a) {
    // Custom handling logic
    metrics_service.increment("alerts.delivered", {
        {"severity", severity_to_string(a.severity)},
        {"rule", a.rule_name}
    });
});

cb.notify(alert);
```

### 6.3 Webhook Notifier

HTTP-based notification with retry logic:

```cpp
webhook_config wh_config;
wh_config.url = "https://hooks.slack.com/services/T00/B00/xxxx";
wh_config.method = "POST";
wh_config.timeout = std::chrono::seconds(10);
wh_config.headers = {{"Content-Type", "application/json"}};
wh_config.max_retries = 3;
wh_config.retry_delay = std::chrono::seconds(5);
wh_config.send_resolved = true;  // Also notify on resolution

// Inject HTTP sender function (for testability)
auto http_sender = [](const std::string& url, const std::string& method,
                      const std::string& body,
                      const std::map<std::string, std::string>& headers,
                      std::chrono::milliseconds timeout) -> bool {
    // Actual HTTP implementation
    return http_client::send(url, method, body, headers, timeout);
};

webhook_notifier webhook(wh_config, http_sender);
webhook.set_formatter(std::make_unique<json_alert_formatter>());

webhook.notify(alert);
```

**Alert formatters**:

| Formatter | Output |
|-----------|--------|
| `json_alert_formatter` | Structured JSON payload |
| `text_alert_formatter` | Human-readable text |

### 6.4 File Notifier

Append alerts to a file — useful for audit trails:

```cpp
file_notifier file_out("/var/log/alerts/notifications.log");
file_out.set_formatter(std::make_unique<text_alert_formatter>());
file_out.notify(alert);
```

### 6.5 Multi Notifier (Fan-out)

Send to multiple channels simultaneously:

```cpp
multi_notifier fan_out;
fan_out.add(std::make_unique<webhook_notifier>(slack_config, http_sender));
fan_out.add(std::make_unique<webhook_notifier>(pagerduty_config, http_sender));
fan_out.add(std::make_unique<file_notifier>("/var/log/alerts.log"));
fan_out.add(std::make_unique<log_notifier>());

// Single call delivers to all channels
fan_out.notify(alert);
```

### 6.6 Buffered Notifier

Batch alerts for efficient delivery:

```cpp
buffered_notifier buffered(
    std::make_unique<webhook_notifier>(config, http_sender),
    /*buffer_size=*/10,
    /*flush_interval=*/std::chrono::seconds(30)
);

// Alerts are buffered until buffer_size reached or flush_interval elapsed
buffered.notify(alert1);
buffered.notify(alert2);
// ... alerts accumulate ...
// Flushed automatically when 10 alerts buffered or 30 seconds pass
```

### 6.7 Routing Notifier

Route alerts to different channels based on conditions:

```cpp
routing_notifier router;

// Route by severity
router.route_by_severity(alert_severity::critical,
    std::make_unique<webhook_notifier>(pagerduty_config, http_sender));
router.route_by_severity(alert_severity::warning,
    std::make_unique<webhook_notifier>(slack_config, http_sender));
router.route_by_severity(alert_severity::info,
    std::make_unique<file_notifier>("/var/log/alerts/info.log"));

// Route by label value
router.route_by_label("team", "infrastructure",
    std::make_unique<webhook_notifier>(infra_slack_config, http_sender));
router.route_by_label("team", "platform",
    std::make_unique<webhook_notifier>(platform_slack_config, http_sender));

// Default route for unmatched alerts
router.set_default_route(
    std::make_unique<log_notifier>());

// Custom routing condition
router.add_route(
    [](const alert& a) { return a.labels.get("env") == "production"; },
    std::make_unique<webhook_notifier>(prod_pagerduty_config, http_sender));

router.notify(alert);  // Routed to matching channel
```

---

## 7. Configuration and Templates

### 7.1 Alert Template Engine

Templates support `${variable}` substitution for dynamic alert content:

```cpp
alert_template tmpl("${severity}: ${name} - ${annotations.summary}");

alert a;
a.name = "HighCPU";
a.severity = alert_severity::critical;
a.annotations.summary = "CPU usage exceeded 90%";

std::string rendered = tmpl.render(a);
// "critical: HighCPU - CPU usage exceeded 90%"
```

**Built-in template variables**:

| Variable | Source | Example |
|----------|--------|---------|
| `${name}` | `alert.name` | `HighCPU` |
| `${state}` | `alert.state` (string) | `firing` |
| `${severity}` | `alert.severity` (string) | `critical` |
| `${value}` | `alert.value` | `95.3` |
| `${labels.KEY}` | `alert.labels[KEY]` | `${labels.service}` → `api` |
| `${annotations.KEY}` | `alert.annotations.custom[KEY]` | `${annotations.runbook_url}` |
| `${annotations.summary}` | `alert.annotations.summary` | Alert summary text |
| `${annotations.description}` | `alert.annotations.description` | Detailed description |

### 7.2 Rule Definition (YAML/JSON Format)

Rules can be defined in configuration files using `rule_definition`:

```cpp
struct rule_definition {
    std::string name;
    std::string metric_name;
    std::string severity;          // "info", "warning", "critical", "emergency"
    std::string group;

    struct trigger_config {
        std::string type;          // "threshold", "rate_of_change", "anomaly", etc.
        std::string op;            // "above", "below", "in_range", etc.
        double value{0};
        double upper_value{0};     // For range triggers
        double sensitivity{3.0};   // For anomaly triggers
    } trigger;

    std::map<std::string, std::string> labels;
    std::map<std::string, std::string> annotations;

    uint32_t for_duration_seconds{0};
    uint32_t evaluation_interval_seconds{15};
    uint32_t repeat_interval_seconds{300};
};
```

**YAML configuration format**:

```yaml
# alert_rules.yaml
groups:
  - name: infrastructure
    interval: 30s
    rules:
      - name: HighCPU
        metric: cpu.usage
        severity: critical
        trigger:
          type: threshold
          op: above
          value: 90.0
        for: 300s
        labels:
          team: infrastructure
          service: api-gateway
        annotations:
          summary: "High CPU usage detected"
          description: "CPU usage has been above 90% for 5 minutes"
          runbook_url: "https://runbooks.internal/high-cpu"

      - name: MemoryLeak
        metric: memory.heap_used
        severity: warning
        trigger:
          type: rate_of_change
          value: 100.0
          direction: increasing
        for: 600s
        labels:
          team: platform

      - name: AnomalousLatency
        metric: http.request_duration_p99
        severity: warning
        trigger:
          type: anomaly
          sensitivity: 2.5
        labels:
          team: platform
```

### 7.3 Rule Builder

Convert `rule_definition` to `alert_rule` programmatically:

```cpp
rule_definition def;
def.name = "HighCPU";
def.metric_name = "cpu.usage";
def.severity = "critical";
def.trigger.type = "threshold";
def.trigger.op = "above";
def.trigger.value = 90.0;
def.for_duration_seconds = 300;
def.labels = {{"team", "infra"}};
def.annotations = {{"summary", "CPU > 90%"}};

// Build the rule from definition
auto result = rule_builder::build(def);
if (result.is_ok()) {
    auto rule = std::move(result.value());
    manager.add_rule(std::move(rule));
}
```

Internal parsing methods:

| Method | Purpose |
|--------|---------|
| `parse_severity()` | String → `alert_severity` enum |
| `parse_operator()` | String → `comparison_operator` enum |
| `build_trigger()` | `trigger_config` → `alert_trigger` unique_ptr |

### 7.4 Rule Registry (Hot Reload)

Dynamic rule management with change notification:

```cpp
rule_registry registry;

// Register callback for rule changes
registry.on_rule_change([&](const std::string& rule_name,
                            const std::string& action) {
    log::info("Rule {} was {}", rule_name, action);
    // action: "added", "removed", "updated"
});

// Load rules from definitions
std::vector<rule_definition> defs = parse_yaml_config("alert_rules.yaml");
registry.load_definitions(defs);

// Query rules by group
auto infra_rules = registry.get_rules_by_group("infrastructure");

// Dynamic rule management
registry.add_rule(new_rule_def);
registry.remove_rule("ObsoleteRule");
```

This enables runtime configuration changes without application restart.

---

## 8. Alert Manager

The `alert_manager` is the central coordinator that ties all components together.

### Configuration

```cpp
alert_manager_config config;
config.default_evaluation_interval = std::chrono::seconds(15);
config.default_repeat_interval = std::chrono::seconds(300);
config.max_alerts_per_rule = 100;
config.max_silences = 1000;

alert_manager manager(config);
```

| Parameter | Default | Purpose |
|-----------|---------|---------|
| `default_evaluation_interval` | 15s | Default rule evaluation frequency |
| `default_repeat_interval` | 5min | Default repeat notification interval |
| `max_alerts_per_rule` | 100 | Cap on active alerts per rule |
| `max_silences` | 1000 | Maximum concurrent silences |

### Rule Management

```cpp
// Add a rule
manager.add_rule(std::move(high_cpu_rule));
manager.add_rule(std::move(memory_leak_rule));

// Remove a rule by name
manager.remove_rule("ObsoleteRule");
```

### Metric Processing

```cpp
// Process a single metric value against all rules
manager.process_metric("cpu.usage", 95.3);

// Process multiple metrics at once
std::map<std::string, double> metrics = {
    {"cpu.usage", 95.3},
    {"memory.used_pct", 82.1},
    {"disk.used_pct", 45.0}
};
manager.process_metrics(metrics);

// Set a metric provider function for pull-based evaluation
manager.set_metric_provider([](const std::string& metric_name) -> std::optional<double> {
    return metric_collector.get_latest(metric_name);
});
```

### Silence Management

```cpp
// Create a silence
auto silence_id = manager.create_silence(
    {{"service", "payment-api"}},             // matchers
    std::chrono::system_clock::now(),          // starts_at
    std::chrono::system_clock::now() + 2h,     // ends_at
    "oncall",                                   // created_by
    "Planned maintenance"                       // comment
);

// Check if an alert is silenced
bool silenced = manager.is_silenced(alert);

// Remove a silence early
manager.delete_silence(silence_id);
```

### Notifier Integration

```cpp
// Add notification channels
manager.add_notifier("slack", std::make_unique<webhook_notifier>(slack_cfg, http_fn));
manager.add_notifier("pagerduty", std::make_unique<webhook_notifier>(pd_cfg, http_fn));
manager.add_notifier("log", std::make_unique<log_notifier>());

// Remove a notifier
manager.remove_notifier("log");
```

### Event Bus Integration

```cpp
// Connect to event bus for alert event broadcasting
manager.set_event_bus(event_bus);
// Alert events: alert_created, alert_resolved, alert_suppressed, notification_sent
```

### Lifecycle Management

```cpp
// Start background evaluation thread
manager.start();

// The evaluation thread:
// 1. Pulls metrics via metric_provider
// 2. Evaluates all rules at their configured intervals
// 3. Processes state transitions
// 4. Runs pipeline stages
// 5. Delivers notifications

// Stop evaluation
manager.stop();
```

### Metrics

```cpp
auto& metrics = manager.get_metrics();
// atomic counters:
// metrics.rules_evaluated
// metrics.alerts_created
// metrics.alerts_resolved
// metrics.alerts_suppressed
// metrics.notifications_sent
// metrics.notifications_failed
```

---

## 9. Production Examples

### Example 1: CPU Spike Detection with Escalation

```cpp
#include <kcenon/monitoring/alert/alert_manager.h>
#include <kcenon/monitoring/alert/alert_triggers.h>
#include <kcenon/monitoring/alert/alert_notifiers.h>

// Create rules with escalating severity
alert_rule cpu_warning("CPUWarning");
cpu_warning.set_severity(alert_severity::warning)
    .set_metric_name("cpu.usage")
    .set_trigger(threshold_trigger::above(80.0))
    .set_for_duration(std::chrono::minutes(5))
    .add_label("team", "infrastructure")
    .add_label("component", "compute");

alert_rule cpu_critical("CPUCritical");
cpu_critical.set_severity(alert_severity::critical)
    .set_metric_name("cpu.usage")
    .set_trigger(threshold_trigger::above(95.0))
    .set_for_duration(std::chrono::minutes(2))
    .add_label("team", "infrastructure")
    .add_label("component", "compute");

// Set up routing notifier
routing_notifier router;
router.route_by_severity(alert_severity::warning,
    std::make_unique<webhook_notifier>(slack_config, http_sender));
router.route_by_severity(alert_severity::critical,
    std::make_unique<webhook_notifier>(pagerduty_config, http_sender));

// Configure inhibition: critical suppresses warning for same instance
inhibition_rule inhibit;
inhibit.source_match = {{"component", "compute"}, {"severity", "critical"}};
inhibit.target_match = {{"component", "compute"}, {"severity", "warning"}};
inhibit.equal = {"instance"};

// Build the alert manager
alert_manager_config config;
config.default_evaluation_interval = std::chrono::seconds(15);
alert_manager manager(config);

manager.add_rule(std::move(cpu_warning));
manager.add_rule(std::move(cpu_critical));
manager.add_notifier("router", std::make_unique<routing_notifier>(std::move(router)));

manager.start();
```

### Example 2: Memory Leak Detection with Anomaly Trigger

```cpp
// Detect gradual memory increase using rate of change
alert_rule memory_leak("MemoryLeak");
memory_leak.set_severity(alert_severity::warning)
    .set_metric_name("memory.heap_used_bytes")
    .set_trigger(rate_of_change_trigger(
        1048576.0,                           // 1MB/s increase rate
        rate_direction::increasing))
    .set_for_duration(std::chrono::minutes(10))  // Sustained for 10 minutes
    .add_label("service", "api-server")
    .add_label("team", "platform");

// Detect sudden memory anomaly using z-score
alert_rule memory_anomaly("MemoryAnomaly");
memory_anomaly.set_severity(alert_severity::critical)
    .set_metric_name("memory.heap_used_bytes")
    .set_trigger(anomaly_trigger(2.5))  // 2.5 standard deviations
    .add_label("service", "api-server")
    .add_label("team", "platform");

// Combined notification — both rules alert the same team
manager.add_rule(std::move(memory_leak));
manager.add_rule(std::move(memory_anomaly));
```

### Example 3: Service Health Composite Alert

```cpp
// Multi-condition health check: high errors AND high latency
auto error_trigger = threshold_trigger::above(5.0);        // Error rate > 5%
auto latency_trigger = threshold_trigger::above(500.0);    // P99 latency > 500ms

auto composite = composite_trigger::all_of({
    std::move(error_trigger),
    std::move(latency_trigger)
});

alert_rule service_degraded("ServiceDegraded");
service_degraded.set_severity(alert_severity::critical)
    .set_metric_name("service.health")  // Composite metric
    .set_trigger(std::move(composite))
    .set_for_duration(std::chrono::minutes(3))
    .add_label("service", "payment-api")
    .add_label("team", "payments");

// Also detect complete service absence
alert_rule service_down("ServiceDown");
service_down.set_severity(alert_severity::emergency)
    .set_metric_name("service.heartbeat")
    .set_trigger(absent_trigger(std::chrono::minutes(2)))
    .add_label("service", "payment-api")
    .add_label("team", "payments");

// Group related rules
alert_rule_group health_group("payment-service-health");
health_group.set_common_interval(std::chrono::seconds(10));
health_group.add_rule(std::move(service_degraded));
health_group.add_rule(std::move(service_down));
```

### Example 4: On-Call Routing with Buffered Notifications

```cpp
// Team-based routing with buffered delivery
routing_notifier team_router;

// Infrastructure team — PagerDuty for critical, Slack for warnings
auto infra_critical = std::make_unique<webhook_notifier>(
    infra_pagerduty_config, http_sender);
auto infra_warning = std::make_unique<buffered_notifier>(
    std::make_unique<webhook_notifier>(infra_slack_config, http_sender),
    /*buffer_size=*/5,
    /*flush_interval=*/std::chrono::minutes(1));

// Platform team — separate Slack channels
auto platform_notifier = std::make_unique<webhook_notifier>(
    platform_slack_config, http_sender);

// Route by team label
team_router.route_by_label("team", "infrastructure",
    std::make_unique<multi_notifier>(std::move(infra_critical)));
team_router.route_by_label("team", "platform",
    std::move(platform_notifier));

// File logging as default fallback
team_router.set_default_route(
    std::make_unique<file_notifier>("/var/log/alerts/unrouted.log"));

// Configure silence for planned maintenance
auto maintenance_end = std::chrono::system_clock::now() + std::chrono::hours(2);
manager.create_silence(
    {{"instance", "prod-db-01"}},
    std::chrono::system_clock::now(),
    maintenance_end,
    "ops-team",
    "Database migration maintenance window"
);

manager.add_notifier("team-router",
    std::make_unique<routing_notifier>(std::move(team_router)));
manager.start();
```

---

## 10. Performance Considerations

### Evaluation Overhead

| Factor | Impact | Mitigation |
|--------|--------|------------|
| Rule count | Linear scan per evaluation cycle | Use rule groups to batch |
| Trigger complexity | Varies by type (threshold is O(1), anomaly is O(window)) | Tune window sizes |
| Label cardinality | Hash computation for fingerprinting | Limit label count per alert |
| Notification latency | Network I/O for webhooks | Use buffered notifier |

### Memory Usage

| Component | Memory Per Instance |
|-----------|-------------------|
| `alert` | ~256 bytes + label/annotation strings |
| `alert_rule` | ~128 bytes + trigger + config |
| `alert_silence` | ~192 bytes + matcher strings |
| `anomaly_trigger` window | ~16 bytes × window_size |
| `rate_of_change_trigger` window | ~24 bytes × sample_count |
| `alert_deduplicator` cache | ~32 bytes × unique_fingerprints |

### Recommended Defaults

| Parameter | Small (< 100 rules) | Medium (100-1000) | Large (1000+) |
|-----------|---------------------|-------------------|---------------|
| `evaluation_interval` | 15s | 30s | 60s |
| `max_alerts_per_rule` | 100 | 50 | 20 |
| `group_wait` | 30s | 60s | 120s |
| Notifier type | Direct | Buffered (10) | Buffered (50) |

### Threading Model

- `alert_manager` runs one background evaluation thread
- Notifiers may run synchronously or asynchronously
- Rule evaluation is sequential within a cycle
- Use `buffered_notifier` to decouple notification I/O from evaluation

---

## 11. API Reference

### Core Types (`alert_types.h`)

| Type | Purpose |
|------|---------|
| `alert_severity` | `info`, `warning`, `critical`, `emergency` |
| `alert_state` | `inactive`, `pending`, `firing`, `resolved`, `suppressed` |
| `alert_labels` | Key-value labels with `fingerprint()`, `matches()`, `merge()` |
| `alert_annotations` | `summary`, `description`, `runbook_url`, `custom` map |
| `alert` | Core alert struct with state machine |
| `alert_group` | Batch of related alerts with `max_severity()` |
| `alert_silence` | Time-bounded label-matching silence |

### Rules (`alert_rule.h`)

| Type | Purpose |
|------|---------|
| `alert_rule_config` | Timing parameters (evaluation, for, repeat, keep_firing) |
| `alert_rule` | Fluent builder for rule definition |
| `alert_trigger` | Abstract base for trigger evaluation |
| `alert_rule_group` | Group with common evaluation interval |

### Triggers (`alert_triggers.h`)

| Type | Detection Method |
|------|-----------------|
| `threshold_trigger` | Fixed value comparison |
| `range_trigger` | Inside/outside range check |
| `rate_of_change_trigger` | Linear regression slope |
| `anomaly_trigger` | Z-score statistical detection |
| `composite_trigger` | Boolean combination (AND/OR/XOR/NOT) |
| `absent_trigger` | Missing data detection |
| `delta_trigger` | Change from previous value |

### Configuration (`alert_config.h`)

| Type | Purpose |
|------|---------|
| `alert_template` | `${variable}` substitution engine |
| `rule_definition` | YAML/JSON-serializable rule spec |
| `rule_builder` | Converts `rule_definition` → `alert_rule` |
| `rule_registry` | Dynamic rule management with hot reload |

### Notifiers (`alert_notifiers.h`)

| Type | Channel |
|------|---------|
| `log_notifier` | Console/log output |
| `callback_notifier` | Custom function invocation |
| `webhook_notifier` | HTTP POST/PUT with retry |
| `file_notifier` | File append |
| `multi_notifier` | Fan-out to multiple channels |
| `buffered_notifier` | Batched delivery |
| `routing_notifier` | Conditional routing |

### Pipeline (`alert_pipeline.h`)

| Type | Purpose |
|------|---------|
| `alert_aggregator` | Group alerts by labels, timed batching |
| `alert_deduplicator` | Fingerprint + state-based dedup |
| `inhibition_rule` | Source/target alert suppression |
| `alert_inhibitor` | Manages multiple inhibition rules |
| `cooldown_tracker` | Per-alert notification rate limiting |
| `pipeline_stage` | Abstract stage base class |
| `alert_pipeline` | Ordered chain of stages |

### Manager (`alert_manager.h`)

| Type | Purpose |
|------|---------|
| `alert_manager_config` | Manager-wide settings |
| `alert_manager_metrics` | Atomic performance counters |
| `alert_manager` | Central coordinator and lifecycle |
| `alert_notifier` | Abstract notifier base class |

---

## See Also

- [Reliability Patterns Guide](RELIABILITY_PATTERNS.md) — Circuit breakers, retry strategies
- [Stream Processing Guide](STREAM_PROCESSING.md) — Metrics aggregation and time series
- Source headers in `include/kcenon/monitoring/alert/`
