---
doc_id: "MON-FEAT-ALERT-001"
doc_title: "Monitoring System - Alerts, Tracing, and Exporters"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "monitoring_system"
category: "FEAT"
---

# Monitoring System - Alerts, Tracing, and Exporters

> **SSOT**: This document is the single source of truth for **Monitoring System - Alerts, Distributed Tracing, and Exporters**.

**Version**: 0.4.0.0
**Last Updated**: 2026-02-08

## Overview

This document describes the alert pipeline, distributed tracing subsystem, and trace/metric exporters. These components provide the foundation for observability signals (alerts and traces) as well as integration with third-party backends (Jaeger, Zipkin, OpenTelemetry).

For core capabilities and performance monitoring, see [FEATURES_CORE.md](FEATURES_CORE.md).
For collector implementations, see [FEATURES_COLLECTORS.md](FEATURES_COLLECTORS.md).

---

## Table of Contents

- [Alert Pipeline](#alert-pipeline)
- [Distributed Tracing](#distributed-tracing)

---

## Alert Pipeline

Automated alerting based on metric thresholds, patterns, and anomalies.

### Overview

The alert pipeline provides comprehensive alerting capabilities:

| Feature | Description |
|---------|-------------|
| **Threshold Triggers** | Simple comparison operators (>, >=, <, <=, ==, !=) |
| **Rate of Change** | Alert on rapid metric changes |
| **Anomaly Detection** | Statistical deviation from normal behavior |
| **Alert Grouping** | Reduce notification noise by grouping related alerts |
| **Flexible Routing** | Route alerts to webhooks, files, or custom handlers |

### Quick Example

```cpp
#include <kcenon/monitoring/alert/alert_manager.h>
#include <kcenon/monitoring/alert/alert_triggers.h>

using namespace kcenon::monitoring;

// Create manager and rule
alert_manager manager;
auto rule = std::make_shared<alert_rule>("high_cpu");
rule->set_metric_name("cpu_usage")
    .set_severity(alert_severity::critical)
    .set_for_duration(std::chrono::minutes(1))
    .set_trigger(threshold_trigger::above(80.0));

manager.add_rule(rule);
manager.add_notifier(std::make_shared<log_notifier>());
manager.start();

// Process metrics
manager.process_metric("cpu_usage", 85.0);
```

### Available Triggers

| Trigger Type | Use Case | Example |
|--------------|----------|---------|
| `threshold_trigger` | Value crosses threshold | `threshold_trigger::above(80.0)` |
| `rate_of_change_trigger` | Rapid changes | Rate > 20/min |
| `anomaly_trigger` | Statistical outliers | 3+ std devs from mean |
| `composite_trigger` | Combined conditions | CPU > 80 AND memory > 90 |
| `absent_trigger` | Missing data | No data for 5 minutes |

### Notification Options

- **webhook_notifier**: HTTP webhook integration with retries
- **file_notifier**: Append alerts to files
- **log_notifier**: Write to logging system
- **routing_notifier**: Route by severity or labels
- **multi_notifier**: Send to multiple targets

See the [Alert Pipeline Guide](guides/ALERT_PIPELINE.md) for full details.

---

## Distributed Tracing

### Span Management

Track request flow across service boundaries with comprehensive span lifecycle management.

**Span Operations**:
- **Create spans**: Root spans and child spans
- **Context propagation**: Trace ID and span ID across threads/services
- **Tag management**: Attach metadata to spans
- **Span export**: Batch export for analysis

**Example**:
```cpp
#include <monitoring/tracing/distributed_tracer.h>

auto& tracer = global_tracer();

// Start root span
auto span_result = tracer.start_span("user_request", "web_service");
if (span_result) {
    auto span = span_result.value();
    span->set_tag("user.id", "12345");
    span->set_tag("endpoint", "/api/users");
    span->set_tag("http.method", "GET");

    // Create child span for database operation
    auto db_span_result = tracer.start_child_span(span, "database_query");
    if (db_span_result) {
        auto db_span = db_span_result.value();
        db_span->set_tag("query.type", "SELECT");
        db_span->set_tag("table", "users");

        // ... database operation ...

        tracer.finish_span(db_span);
    }

    // Create child span for cache operation
    auto cache_span_result = tracer.start_child_span(span, "cache_lookup");
    if (cache_span_result) {
        auto cache_span = cache_span_result.value();
        cache_span->set_tag("cache.hit", "true");

        tracer.finish_span(cache_span);
    }

    tracer.finish_span(span);
}

// Export traces
auto export_result = tracer.export_traces();
```

### Context Propagation

Propagate trace context across service boundaries and thread boundaries.

**Features**:
- Thread-local context storage
- Automatic context inheritance
- Cross-service propagation
- Minimal overhead (<50ns per hop)

**Example**:
```cpp
#include <kcenon/monitoring/tracing/trace_context.h>

// Set context in parent thread
trace_context ctx;
ctx.trace_id = "abc123";
ctx.span_id = "span001";
ctx.set_baggage("user_id", "12345");

set_current_trace_context(ctx);

// Context automatically propagates to child operations
std::thread worker([&]() {
    auto ctx = get_current_trace_context();
    // ctx.trace_id == "abc123"
    // ctx.get_baggage("user_id") == "12345"
});
```

### Trace Export

Batch export traces for external analysis systems.

**Export Formats**:
- JSON (human-readable)
- Binary (compact, efficient)
- OpenTelemetry Protocol (OTLP)
- Custom exporters

**Example**:
```cpp
// Configure exporter
trace_exporter_config config;
config.format = trace_export_format::json;
config.batch_size = 100;
config.flush_interval = std::chrono::seconds(5);

auto exporter = create_trace_exporter(config);

// Export traces
auto export_result = tracer.export_traces();
if (!export_result) {
    std::cerr << "Export failed: " << export_result.error().message << "\n";
}
```

### Available Trace Exporters

The monitoring system ships with several trace exporter implementations:

- **jaeger_exporter**: Exports spans to a Jaeger collector
- **zipkin_exporter**: Exports spans in Zipkin JSON format
- **otlp_exporter**: Exports using the OpenTelemetry Protocol (HTTP/gRPC)
- **console_exporter**: Prints spans to stdout for debugging

### Available Metric Exporters

In addition to trace exporters, metric exporters are provided for pushing collected metrics to popular backends:

- **prometheus_exporter**: Exposes metrics in Prometheus text format
- **statsd_exporter**: Sends metrics via the StatsD UDP protocol
- **influxdb_exporter**: Sends metrics using InfluxDB line protocol
- **graphite_exporter**: Sends metrics using the Graphite plaintext protocol

---

## See Also

- [FEATURES.md](FEATURES.md) - Features index page
- [FEATURES_CORE.md](FEATURES_CORE.md) - Core capabilities and performance monitoring
- [FEATURES_COLLECTORS.md](FEATURES_COLLECTORS.md) - Collector implementations
- [API Reference](API_REFERENCE.md) - Complete API documentation
- [Alert Pipeline Guide](guides/ALERT_PIPELINE.md) - Alert pipeline usage guide
- [Architecture Guide](ARCHITECTURE.md) - System design and patterns
