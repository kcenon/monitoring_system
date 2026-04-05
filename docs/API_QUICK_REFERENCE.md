# API Quick Reference -- monitoring_system

Cheat-sheet for the most common `monitoring_system` APIs.
For full details see [API_REFERENCE.md](API_REFERENCE.md) and the Doxygen-generated docs.

---

## Header and Namespace

```cpp
#include <kcenon/monitoring/factory/metric_factory.h>
#include <kcenon/monitoring/collectors/collector_base.h>
#include <kcenon/monitoring/alert/alert_rule.h>
#include <kcenon/monitoring/alert/alert_triggers.h>
#include <kcenon/monitoring/tracing/trace_context.h>
#include <kcenon/monitoring/health/health_monitor.h>

using namespace kcenon::monitoring;
```

---

## Collector Factory

```cpp
auto& factory = metric_factory::instance();

// Register a custom collector
factory.register_collector<my_collector>("my_collector");

// Create with config
config_map config = {{"enabled", "true"}};
auto result = factory.create("my_collector", config);
if (result) {
    auto& collector = result.collector;
    auto name = collector->get_name();
    bool ok   = collector->is_healthy();
    auto types = collector->get_metric_types();
}
```

---

## Metric Types

```cpp
enum class metric_type : uint8_t {
    counter,      // Monotonically increasing
    gauge,        // Instantaneous value
    histogram,    // Distribution of values
    summary,      // Summary statistics
    timer,        // Duration measurements
    set           // Unique value counting
};
```

### Metric Metadata

```cpp
metric_metadata meta{
    .name_hash = hash("cpu_usage"),
    .type = metric_type::gauge,
    // ...
};
```

---

## Collector Base (CRTP)

```cpp
class my_collector : public collector_base<my_collector> {
public:
    static constexpr const char* collector_name = "my_collector";

    bool do_initialize(const config_map& config);
    std::vector<metric> do_collect();
    bool is_available() const;
    std::vector<std::string> do_get_metric_types() const;
    void do_add_statistics(stats_map& stats) const;
};
```

### Built-in Collectors

| Collector | Description |
|---|---|
| `system_resource_collector` | CPU, memory, disk |
| `process_metrics_collector` | Process CPU/memory |
| `network_metrics_collector` | Network I/O |
| `temperature_collector` | Hardware temps |
| `battery_collector` | Battery status |
| `gpu_collector` | GPU utilization |
| `uptime_collector` | System uptime |
| `smart_collector` | Disk S.M.A.R.T. |
| `container_collector` | Container metrics |
| `interrupt_collector` | IRQ statistics |
| `vm_collector` | Virtual machine metrics |
| `security_collector` | Security metrics |
| `power_collector` | Power usage |

---

## Alert Rules

```cpp
alert_rule rule("high_cpu");
rule.set_severity(alert_severity::critical)
    .set_summary("CPU usage is high")
    .set_description("CPU exceeded ${threshold}%")
    .add_label("team", "infrastructure")
    .set_for_duration(std::chrono::minutes(5));

rule.set_trigger(threshold_trigger::above(80.0));
```

### Alert Rule Config

```cpp
alert_rule_config config{
    .evaluation_interval = std::chrono::milliseconds(15000),
    .for_duration = std::chrono::milliseconds(0),
    .repeat_interval = std::chrono::milliseconds(300000),
    .keep_firing_for = false
};
```

---

## Alert Triggers

### Threshold Trigger

```cpp
auto trigger = threshold_trigger::above(80.0);    // value > 80
auto trigger = threshold_trigger::below(10.0);    // value < 10

auto trigger = std::make_shared<threshold_trigger>(
    5.0, comparison_operator::greater_or_equal);
```

### Comparison Operators

| Operator | Symbol |
|---|---|
| `greater_than` | `>` |
| `greater_or_equal` | `>=` |
| `less_than` | `<` |
| `less_or_equal` | `<=` |
| `equal` | `==` |
| `not_equal` | `!=` |

---

## Alert Pipeline (aggregation)

```cpp
alert_aggregator_config config{
    .group_wait = std::chrono::milliseconds(30000),
    .group_interval = std::chrono::milliseconds(300000),
    .resolve_timeout = std::chrono::milliseconds(300000),
    .group_by_labels = {"service", "environment"}
};

alert_aggregator aggregator(config);
aggregator.add_alert(alert);

auto groups = aggregator.get_ready_groups();
for (auto& group : groups) {
    send_notification(group);
    aggregator.mark_sent(group.group_key);
}
```

---

## Trace Context

```cpp
#include <kcenon/monitoring/tracing/trace_context.h>

// Root context
auto root = trace_context::create_root("handle_request");

// Child spans
auto child = root.create_child("query_database");
auto child2 = root.create_child("check_cache");

// Fields
root.trace_id;           // string
root.span_id;            // string
root.parent_span_id;     // optional<string>
root.start_time;         // time_point
```

### Distributed Tracer

```cpp
#include <kcenon/monitoring/tracing/distributed_tracer.h>

distributed_tracer tracer;
auto span = tracer.start_span("process_order", root_context);
span.set_attribute("order_id", "12345");
span.end();
tracer.export_spans();
```

---

## Health Monitor

```cpp
#include <kcenon/monitoring/health/health_monitor.h>

health_monitor monitor({
    .check_interval = std::chrono::milliseconds(5000),
    .enable_auto_recovery = true,
    .max_consecutive_failures = 3
});
```

### Health Check Builder

```cpp
auto check = health_check_builder()
    .with_name("database")
    .with_type(health_check_type::readiness)
    .with_check([] { return check_db(); })
    .critical(true)
    .build();

monitor.register_check("database", check);
monitor.start();
```

### Health Check Types

| Type | Description |
|---|---|
| `liveness` | Process is alive (restart if failing) |
| `readiness` | Service ready for traffic |
| `startup` | Application finished initializing |

### Health Monitor Stats

```cpp
auto stats = monitor.get_stats();
stats.total_checks;           // size_t
stats.healthy_checks;         // size_t
stats.unhealthy_checks;       // size_t
stats.recovery_attempts;      // size_t
stats.successful_recoveries;  // size_t
stats.last_check_time;        // time_point
```

---

## OTLP Exporter (gRPC)

```cpp
#include <kcenon/monitoring/exporters/otlp_grpc_exporter.h>

otlp_grpc_config config{
    .endpoint = "localhost:4317",
    .timeout = std::chrono::milliseconds(10000),
    .batch_timeout = std::chrono::milliseconds(5000),
    .max_batch_size = 512,
    .max_queue_size = 2048,
    .max_retry_attempts = 3,
    .use_tls = false,
    .service_name = "my_service",
    .service_version = "1.0.0"
};

auto exporter = std::make_shared<otlp_grpc_exporter>(config);
exporter->start();
exporter->export_spans(spans);
exporter->stop();
```

---

## Performance Monitor

```cpp
#include <kcenon/monitoring/core/performance_monitor.h>

// Implements common_system::IMonitor
performance_monitor monitor;
monitor.record_metric("request_latency", 42.5);
monitor.increment_counter("requests_total");
```

---

## Reliability Patterns

```cpp
#include <kcenon/monitoring/reliability/circuit_breaker.h>
#include <kcenon/monitoring/reliability/retry_policy.h>
#include <kcenon/monitoring/reliability/error_boundary.h>

// Circuit breaker
circuit_breaker cb(circuit_breaker_config{
    .failure_threshold = 5,
    .recovery_timeout = std::chrono::seconds(30)
});

auto result = cb.execute([] { return call_external_service(); });

// Retry policy
retry_policy policy{
    .max_retries = 3,
    .initial_backoff = std::chrono::milliseconds(100),
    .max_backoff = std::chrono::seconds(10)
};
```

---

## Quick Recipe

```cpp
#include <kcenon/monitoring/factory/metric_factory.h>
#include <kcenon/monitoring/alert/alert_rule.h>
#include <kcenon/monitoring/alert/alert_triggers.h>
#include <kcenon/monitoring/health/health_monitor.h>

// 1. Set up health monitoring
health_monitor monitor({.check_interval = std::chrono::seconds(5)});
monitor.register_check("api", health_check_builder()
    .with_name("api").with_type(health_check_type::readiness)
    .with_check([] { return ping_api(); }).build());
monitor.start();

// 2. Create alert rule
alert_rule cpu_rule("cpu_alert");
cpu_rule.set_severity(alert_severity::warning)
    .set_summary("High CPU usage")
    .set_trigger(threshold_trigger::above(75.0));

// 3. Collect metrics
auto& factory = metric_factory::instance();
auto result = factory.create("system_resource_collector", {{"enabled", "true"}});
```
