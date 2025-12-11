# Monitoring System - Best Practices Guide

> **Version:** 0.1.0
> **Last Updated:** 2025-11-11
> **Audience:** Developers, DevOps Engineers, SREs

This guide provides comprehensive best practices for using monitoring_system in production environments, covering metrics design, alerting, performance, and operational excellence.

---

## Table of Contents

1. [Metrics Design](#metrics-design)
2. [Label Management](#label-management)
3. [Alert Design](#alert-design)
4. [Distributed Tracing](#distributed-tracing)
5. [Performance Optimization](#performance-optimization)
6. [Storage Configuration](#storage-configuration)
7. [Production Deployment](#production-deployment)
8. [Security](#security)
9. [Observability Patterns](#observability-patterns)
10. [Common Anti-Patterns](#common-anti-patterns)

---

## Metrics Design

### Choose the Right Metric Type

**DO:** Select metric type based on data characteristics

| Type | When to Use | Example |
|------|-------------|---------|
| **Counter** | Cumulative values that only increase | `requests_total`, `errors_total` |
| **Gauge** | Values that can increase/decrease | `memory_bytes`, `queue_size` |
| **Histogram** | Distribution of values | `request_duration_ms`, `response_size_bytes` |
| **Summary** | Percentile tracking | `api_latency_p99` |

```cpp
// ✅ DO: Counter for cumulative counts
monitor->record_counter("http_requests_total", 1.0, {
    {"method", "GET"},
    {"status", "200"}
});

// ✅ DO: Gauge for current state
monitor->record_gauge("active_connections", conn_pool.size());

// ✅ DO: Histogram for latencies
monitor->record_histogram("request_duration_ms", latency);

// ❌ DON'T: Use counter for values that can decrease
monitor->record_counter("memory_usage_bytes", memory);  // WRONG! Use gauge
```

---

### Metric Naming Conventions

**DO:** Follow consistent naming conventions

**Standard Format:** `<namespace>_<subsystem>_<name>_<unit>`

```cpp
// ✅ DO: Clear, descriptive names with units
monitor->record_counter("http_requests_total", 1.0);
monitor->record_gauge("memory_usage_bytes", mem_bytes);
monitor->record_histogram("request_duration_seconds", duration_s);
monitor->record_gauge("cpu_usage_percent", cpu_pct);
monitor->record_counter("disk_writes_total", 1.0);

// ❌ DON'T: Ambiguous or inconsistent names
monitor->record_counter("requests", 1.0);           // Missing suffix and unit
monitor->record_gauge("mem", memory);               // Unclear abbreviation
monitor->record_histogram("latency", lat);          // No unit
monitor->record_gauge("CPUUsage", cpu);             // Inconsistent case
```

**Units:**
- Time: `_seconds`, `_milliseconds`, `_microseconds`
- Size: `_bytes`, `_kilobytes`, `_megabytes`
- Percentage: `_percent`, `_ratio`
- Rate: `_per_second`, `_total`

---

### Metric Granularity

**DO:** Balance detail with cardinality

```cpp
// ✅ DO: Aggregate by meaningful dimensions
monitor->record_counter("api_requests_total", 1.0, {
    {"service", "payment"},
    {"endpoint", "/api/v1/checkout"},
    {"method", "POST"},
    {"status_class", "2xx"}  // Not individual status codes
});

// ❌ DON'T: Excessive granularity
monitor->record_counter("api_requests_total", 1.0, {
    {"service", "payment"},
    {"endpoint", "/api/v1/checkout"},
    {"method", "POST"},
    {"status", "200"},              // Too granular
    {"user_id", user_id},           // High cardinality!
    {"request_id", request_id},     // High cardinality!
    {"timestamp", timestamp}        // Infinite cardinality!
});
```

**Cardinality Budget:**
- Low cardinality (<100 values): Safe for labels
- Medium cardinality (100-1000): Use with caution
- High cardinality (>1000): Avoid as labels, use tracing instead

---

### Metric Lifecycle Management

**DO:** Initialize metrics at startup

```cpp
class ApplicationMetrics {
public:
    ApplicationMetrics(std::shared_ptr<monitoring_system> monitor)
        : monitor_(std::move(monitor)) {
        // Initialize all metrics at startup
        initialize_metrics();
    }

private:
    void initialize_metrics() {
        // ✅ DO: Initialize counters to 0
        monitor_->record_counter("http_requests_total", 0.0, {{"status", "2xx"}});
        monitor_->record_counter("http_requests_total", 0.0, {{"status", "4xx"}});
        monitor_->record_counter("http_requests_total", 0.0, {{"status", "5xx"}});

        // ✅ DO: Set initial gauge values
        monitor_->record_gauge("app_version", 2.0);
        monitor_->record_gauge("app_start_time_seconds",
            std::chrono::system_clock::now().time_since_epoch().count());
    }

    std::shared_ptr<monitoring_system> monitor_;
};
```

---

## Label Management

### Label Cardinality

**DO:** Keep label cardinality under control

**Cardinality** = Number of unique label combinations

```cpp
// ✅ DO: Low cardinality labels
monitor->record_counter("api_requests", 1.0, {
    {"method", "GET"},       // ~10 values (GET, POST, PUT, DELETE, ...)
    {"endpoint", endpoint},  // ~50 values (distinct API endpoints)
    {"status", "2xx"}        // ~5 values (1xx, 2xx, 3xx, 4xx, 5xx)
});
// Total cardinality: 10 × 50 × 5 = 2,500 time series

// ❌ DON'T: High cardinality labels
monitor->record_counter("api_requests", 1.0, {
    {"user_id", user_id},       // Millions of users!
    {"session_id", session_id}, // Billions of sessions!
    {"request_id", request_id}  // Infinite!
});
// Total cardinality: BILLIONS of time series → System overload
```

**High-Cardinality Data → Use Distributed Tracing Instead:**
```cpp
// ✅ DO: Use tracing for high-cardinality data
auto trace = tracer->start_trace("api_request");
trace->set_tag("user_id", user_id);
trace->set_tag("session_id", session_id);
trace->set_tag("request_id", request_id);

// Keep metrics low-cardinality
monitor->record_counter("api_requests", 1.0, {
    {"endpoint", endpoint},
    {"status_class", "2xx"}
});
```

---

### Label Consistency

**DO:** Use consistent label names across metrics

```cpp
// ✅ DO: Consistent labels across related metrics
monitor->record_counter("http_requests_total", 1.0, {
    {"service", "api"},
    {"method", "GET"},
    {"status", "200"}
});

monitor->record_histogram("http_request_duration_seconds", duration, {
    {"service", "api"},
    {"method", "GET"},
    {"status", "200"}
});

monitor->record_counter("http_errors_total", 1.0, {
    {"service", "api"},
    {"method", "GET"},
    {"status", "500"}
});

// ❌ DON'T: Inconsistent labels
monitor->record_counter("http_requests_total", 1.0, {
    {"svc", "api"},         // Different label name
    {"http_method", "GET"}, // Different label name
    {"code", "200"}         // Different label name
});
```

**Standard Label Names:**
- `service`, `component`, `instance`
- `method`, `endpoint`, `status`, `status_class`
- `env`, `region`, `zone`, `datacenter`
- `version`, `release`, `build`

---

### Label Values

**DO:** Use normalized, predictable label values

```cpp
// ✅ DO: Normalized label values
monitor->record_counter("http_requests", 1.0, {
    {"method", to_upper(request.method())},  // Always uppercase
    {"endpoint", normalize_endpoint(uri)},   // /users/:id, not /users/123
    {"status_class", status / 100 + "xx"}   // 2xx, not 200
});

// Helper function
auto normalize_endpoint(const std::string& uri) -> std::string {
    // /users/123 → /users/:id
    // /orders/456/items/789 → /orders/:id/items/:id
    return replace_ids_with_placeholder(uri);
}

// ❌ DON'T: Raw, unpredictable values
monitor->record_counter("http_requests", 1.0, {
    {"method", request.method()},      // Mixed case: "get", "GET", "Get"
    {"endpoint", request.uri()},       // /users/123, /users/456, ...
    {"status", std::to_string(status)} // 200, 201, 202, 203, ...
});
```

---

## Alert Design

### Alert Rule Best Practices

**DO:** Design actionable alerts

```cpp
// ✅ DO: Actionable alert with clear thresholds
auto alert = alert_rule::builder()
    .with_name("high_error_rate")
    .with_metric("http_errors_total")
    .with_labels({{"status_class", "5xx"}})
    .with_condition(condition_type::rate, 0.05)  // > 5% error rate
    .with_duration(std::chrono::minutes(5))      // Sustained for 5 minutes
    .with_severity(severity_level::critical)
    .with_message(
        "HTTP 5xx error rate above 5% for 5 minutes. "
        "Check service logs and dependencies."
    )
    .with_annotations({
        {"runbook", "https://wiki.company.com/runbooks/http-5xx-errors"},
        {"dashboard", "https://grafana.company.com/d/service-health"},
        {"oncall", "https://pagerduty.com/schedules/platform-oncall"}
    })
    .build();

// ❌ DON'T: Vague, non-actionable alert
auto bad_alert = alert_rule::builder()
    .with_name("errors")  // Unclear name
    .with_metric("errors")
    .with_condition(condition_type::greater_than, 0)  // Fires on single error!
    .with_severity(severity_level::critical)  // Everything is critical?
    .with_message("Errors detected")  // No context
    .build();
```

---

### Alert Severity Levels

**DO:** Use severity levels appropriately

| Severity | When to Use | Response | Examples |
|----------|-------------|----------|----------|
| **Critical** | Service down, data loss imminent | Immediate page | Service unavailable, database down |
| **Warning** | Potential issues, degraded performance | Check during business hours | High latency, disk 80% full |
| **Info** | Noteworthy events | Log, no action | Deployment completed, config changed |

```cpp
// ✅ DO: Appropriate severity
auto critical_alert = alert_rule::builder()
    .with_name("service_down")
    .with_severity(severity_level::critical)
    .with_message("Service is unavailable - immediate action required")
    .build();

auto warning_alert = alert_rule::builder()
    .with_name("high_latency")
    .with_severity(severity_level::warning)
    .with_message("Latency above SLA - investigate during business hours")
    .build();

// ❌ DON'T: Everything is critical
auto bad_alert = alert_rule::builder()
    .with_name("disk_80_percent")
    .with_severity(severity_level::critical)  // Not critical yet!
    .build();
```

---

### Alert Fatigue Prevention

**DO:** Implement alert management strategies

```cpp
// ✅ DO: Alert grouping
monitor->set_alert_grouping({
    .group_by = {"service", "env"},
    .group_interval = std::chrono::minutes(5),
    .group_wait = std::chrono::seconds(30)
});

// ✅ DO: Alert inhibition (suppress dependent alerts)
monitor->add_inhibition_rule({
    .source_alert = "service_down",
    .target_alerts = {"high_latency", "high_error_rate"},
    .equal_labels = {"service"}
});

// ✅ DO: Alert throttling
auto alert = alert_rule::builder()
    .with_name("disk_space_low")
    .with_throttle(std::chrono::hours(1))  // Max once per hour
    .with_repeat_interval(std::chrono::hours(6))  // Repeat every 6h if unresolved
    .build();

// ✅ DO: Maintenance windows
monitor->add_silence({
    .matchers = {{"env", "staging"}},
    .start_time = now(),
    .end_time = now() + std::chrono::hours(2),
    .reason = "Planned maintenance: database upgrade",
    .created_by = "ops-team"
});
```

---

### Alert Conditions

**DO:** Use appropriate conditions and thresholds

```cpp
// ✅ DO: Rate-based alerting for errors
auto error_alert = alert_rule::builder()
    .with_name("error_rate_high")
    .with_metric("http_errors_total")
    .with_condition(condition_type::rate, 0.05)  // 5% error rate
    .with_evaluation_window(std::chrono::minutes(5))
    .build();

// ✅ DO: Sustained threshold for capacity issues
auto disk_alert = alert_rule::builder()
    .with_name("disk_space_low")
    .with_metric("disk_usage_percent")
    .with_condition(condition_type::greater_than, 85.0)
    .with_duration(std::chrono::minutes(10))  // Sustained for 10 min
    .build();

// ✅ DO: Anomaly detection for unexpected changes
auto anomaly_alert = alert_rule::builder()
    .with_name("traffic_anomaly")
    .with_metric("http_requests_total")
    .with_condition(condition_type::anomaly)
    .with_sensitivity(0.95)  // 95% confidence
    .build();

// ❌ DON'T: Alert on single data point
auto bad_alert = alert_rule::builder()
    .with_metric("response_time_ms")
    .with_condition(condition_type::greater_than, 100)
    .build();  // Fires on every slow request!
```

---

## Distributed Tracing

### Span Design

**DO:** Create meaningful spans

```cpp
// ✅ DO: Span hierarchy reflects operation structure
auto trace_ctx = tracer->start_trace("user_request");

{
    auto auth_span = tracer->start_span("authenticate", trace_ctx);
    auth_span->set_tag("user_id", user_id);
    auth_span->set_tag("auth_method", "oauth2");

    // ... authentication ...

    if (!authenticated) {
        auth_span->set_tag("error", true);
        auth_span->log_event("auth_failed", error_message);
    }
}

{
    auto business_span = tracer->start_span("process_business_logic", trace_ctx);

    {
        auto db_span = tracer->start_span("database_query", trace_ctx);
        db_span->set_tag("db_type", "postgresql");
        db_span->set_tag("query_type", "SELECT");
        db_span->set_tag("table", "users");

        // ... database query ...
    }

    {
        auto cache_span = tracer->start_span("cache_lookup", trace_ctx);
        cache_span->set_tag("cache_type", "redis");
        cache_span->set_tag("cache_hit", cache_hit);

        // ... cache operation ...
    }
}

tracer->finish_trace(trace_ctx);

// ❌ DON'T: Flat spans without hierarchy
auto span1 = tracer->start_span("auth", trace_ctx);
auto span2 = tracer->start_span("db", trace_ctx);
auto span3 = tracer->start_span("cache", trace_ctx);
```

---

### Sampling Strategies

**DO:** Use appropriate sampling for different services

```cpp
// ✅ DO: Adaptive sampling based on traffic
auto sampling = sampling_strategy::builder()
    // Always trace errors
    .always_sample_errors(true)

    // 100% sampling for low-traffic endpoints
    .sample_rate_for_pattern("/admin/*", 1.0)

    // 10% sampling for high-traffic endpoints
    .sample_rate_for_pattern("/api/*", 0.1)

    // 1% sampling for health checks
    .sample_rate_for_pattern("/health", 0.01)

    // Tail-based sampling: Keep interesting traces
    .with_tail_sampling([](const trace& t) {
        return t.has_error() ||
               t.duration() > std::chrono::seconds(1) ||
               t.span_count() > 10;
    })

    .build();

tracer->set_sampling_strategy(sampling);

// ❌ DON'T: Uniform sampling everywhere
tracer->set_sampling_strategy(
    sampling_strategy::probabilistic(0.01)  // Loses important traces!
);
```

---

### Span Tags and Events

**DO:** Add meaningful context to spans

```cpp
// ✅ DO: Standard tags and custom tags
auto span = tracer->start_span("http_request", trace_ctx);

// Standard tags
span->set_tag("http.method", "POST");
span->set_tag("http.url", "/api/v1/orders");
span->set_tag("http.status_code", 201);
span->set_tag("component", "api_gateway");

// Custom application tags
span->set_tag("order_id", order_id);
span->set_tag("user_id", user_id);
span->set_tag("payment_method", "credit_card");
span->set_tag("total_amount", total);

// Log events
span->log_event("validation_started");
span->log_event("payment_processed", {
    {"transaction_id", txn_id},
    {"amount", amount}
});

if (error) {
    span->set_tag("error", true);
    span->set_tag("error.kind", "ValidationError");
    span->log_event("error", {
        {"message", error_message},
        {"stack", stack_trace}
    });
}

// ❌ DON'T: Sparse spans with no context
auto span = tracer->start_span("operation", trace_ctx);
// ... operation ...
// No tags, no events, no context!
```

---

## Performance Optimization

### Minimize Recording Overhead

**DO:** Use efficient recording patterns

```cpp
// ✅ DO: Batch recording for high-frequency metrics
std::vector<metric_sample> samples;
samples.reserve(100);

for (const auto& request : requests) {
    samples.push_back({
        "requests_total",
        1.0,
        metric_type::counter,
        {{"status", std::to_string(request.status)}}
    });
}

monitor->record_batch(samples);  // Single lock acquisition

// ✅ DO: Thread-local buffering
monitor->enable_thread_local_collection();

// Each thread buffers metrics locally, flushes periodically
monitor->record_counter("high_frequency_metric", 1.0);

// ❌ DON'T: Record individually in hot path
for (const auto& request : requests) {
    monitor->record_counter("requests_total", 1.0);  // Lock per call!
}
```

---

### Sampling High-Frequency Metrics

**DO:** Sample metrics that fire millions of times

```cpp
// ✅ DO: Sample high-frequency metrics
class RateLimiter {
private:
    std::atomic<uint64_t> counter_{0};
    std::shared_ptr<monitoring_system> monitor_;

public:
    void record_request() {
        // Only record every 100th request
        if (++counter_ % 100 == 0) {
            monitor->record_counter("rate_limiter_requests_total", 100.0);
        }
    }
};

// ✅ DO: Reservoir sampling for histograms
class LatencyTracker {
private:
    std::atomic<uint64_t> count_{0};
    static constexpr double SAMPLE_RATE = 0.01;  // 1%

public:
    void record_latency(double latency_ms) {
        if (random_double() < SAMPLE_RATE) {
            monitor->record_histogram("request_duration_ms", latency_ms);
        }
        ++count_;
    }
};

// ❌ DON'T: Record every single event
void process_packet(const Packet& packet) {
    monitor->record_counter("packets_processed", 1.0);  // Millions per second!
}
```

---

### Lazy Initialization

**DO:** Initialize monitoring components lazily

```cpp
// ✅ DO: Lazy initialization
class ServiceMetrics {
public:
    void record_request(const std::string& endpoint) {
        get_or_create_counter(endpoint)->increment();
    }

private:
    auto get_or_create_counter(const std::string& endpoint)
        -> std::shared_ptr<Counter> {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = counters_.find(endpoint);
        if (it != counters_.end()) {
            return it->second;
        }

        auto counter = monitor_->create_counter("api_requests_total", {
            {"endpoint", endpoint}
        });

        counters_[endpoint] = counter;
        return counter;
    }

    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Counter>> counters_;
    std::shared_ptr<monitoring_system> monitor_;
};
```

---

## Storage Configuration

### Retention Policies

**DO:** Configure appropriate retention based on needs

```cpp
// ✅ DO: Tiered retention
auto retention = retention_policy::builder()
    // High-resolution data for 24 hours
    .with_tier(retention_tier::builder()
        .with_resolution(std::chrono::seconds(10))
        .with_max_age(std::chrono::hours(24))
        .build())

    // Downsampled data for 7 days
    .with_tier(retention_tier::builder()
        .with_resolution(std::chrono::minutes(1))
        .with_max_age(std::chrono::days(7))
        .with_aggregations({
            aggregation_type::mean,
            aggregation_type::min,
            aggregation_type::max
        })
        .build())

    // Long-term aggregates for 90 days
    .with_tier(retention_tier::builder()
        .with_resolution(std::chrono::hours(1))
        .with_max_age(std::chrono::days(90))
        .with_aggregations({
            aggregation_type::mean,
            aggregation_type::p95,
            aggregation_type::p99
        })
        .build())

    .build();

monitor->set_retention_policy(retention);

// ❌ DON'T: Keep everything at full resolution forever
auto bad_retention = retention_policy::builder()
    .with_max_age(std::chrono::days(365))  // 1 year of raw data!
    .with_resolution(std::chrono::seconds(1))  // Every second!
    .build();  // Massive storage cost
```

---

### Storage Backend Selection

**DO:** Choose appropriate storage backend

```cpp
// ✅ DO: In-memory for real-time dashboards
auto realtime_storage = storage_config::builder()
    .with_backend(storage_backend::in_memory)
    .with_max_samples(1'000'000)
    .with_retention(std::chrono::hours(1))
    .build();

// ✅ DO: File-based for persistence
auto persistent_storage = storage_config::builder()
    .with_backend(storage_backend::file)
    .with_path("/var/lib/monitoring/metrics")
    .with_compression(compression_type::zstd)
    .with_fsync_interval(std::chrono::seconds(30))
    .build();

// ✅ DO: Hybrid approach
monitor->set_primary_storage(realtime_storage);
monitor->set_secondary_storage(persistent_storage);

// Recent data from memory, historical from disk
auto query = monitor->query_range(
    "request_duration_ms",
    now() - std::chrono::hours(24),
    now()
);
```

---

## Production Deployment

### Configuration Management

**DO:** Externalize configuration

```cpp
// ✅ DO: Load config from file
auto config = monitoring_config::load_from_file("/etc/myapp/monitoring.yaml");

// monitoring.yaml:
//   metrics:
//     retention: 7d
//     sampling_rate: 0.1
//   alerting:
//     evaluation_interval: 30s
//   storage:
//     backend: file
//     path: /var/lib/myapp/metrics
//   exporters:
//     - type: prometheus
//       port: 9090
//     - type: otlp
//       endpoint: http://otel-collector:4317

auto monitor = monitoring_system::create(config).value();

// ❌ DON'T: Hardcode configuration
auto monitor = monitoring_system::builder()
    .with_retention(std::chrono::days(7))  // Hardcoded!
    .with_sampling_rate(0.1)               // Hardcoded!
    .build();
```

---

### Health Checks

**DO:** Implement comprehensive health checks

```cpp
// ✅ DO: Multi-level health checks
class ApplicationHealth {
public:
    auto check_health() -> health_status {
        auto status = health_status::healthy;

        // Check monitoring system itself
        if (!monitor_->is_healthy()) {
            status = health_status::degraded;
        }

        // Check metric recording
        auto test_result = monitor_->record_counter("health_check", 1.0);
        if (!test_result.has_value()) {
            status = health_status::unhealthy;
        }

        // Check storage
        auto storage_stats = monitor_->get_storage_stats();
        if (storage_stats.used_percent > 0.95) {
            status = health_status::degraded;
        }

        // Check alerting
        if (!monitor_->is_alerting_enabled()) {
            status = health_status::degraded;
        }

        return status;
    }

private:
    std::shared_ptr<monitoring_system> monitor_;
};

// Expose health endpoint
app.get("/health", [&health_checker](auto req, auto res) {
    auto status = health_checker.check_health();
    res.status(status == health_status::healthy ? 200 : 503)
       .json({{"status", to_string(status)}});
});
```

---

### Graceful Shutdown

**DO:** Implement proper shutdown sequence

```cpp
// ✅ DO: Graceful shutdown with metric flushing
class Application {
public:
    void shutdown() {
        shutdown_requested_ = true;

        // 1. Stop accepting new requests
        server_->stop_accepting();

        // 2. Flush pending metrics
        monitor_->flush_metrics();

        // 3. Wait for in-flight operations
        wait_for_in_flight_operations();

        // 4. Record shutdown metric
        monitor_->record_counter("app_shutdown_total", 1.0);

        // 5. Stop monitoring system
        monitor_->stop();

        // 6. Wait for final export
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

private:
    std::shared_ptr<monitoring_system> monitor_;
    std::shared_ptr<http_server> server_;
    std::atomic<bool> shutdown_requested_{false};
};
```

---

## Security

### Credential Management

**DO:** Secure monitoring credentials

```cpp
// ✅ DO: Use environment variables or secret management
auto alerting_config = alerting_config::builder()
    .with_email(
        std::getenv("SMTP_HOST"),
        std::stoi(std::getenv("SMTP_PORT")),
        std::getenv("SMTP_USER"),
        std::getenv("SMTP_PASSWORD")  // From environment
    )
    .with_pagerduty_key(
        secret_manager.get_secret("pagerduty_api_key")  // From secret manager
    )
    .build();

// ❌ DON'T: Hardcode credentials
auto bad_config = alerting_config::builder()
    .with_email("smtp.gmail.com", 587, "user@gmail.com", "password123")  // WRONG!
    .build();
```

---

### Metric Data Privacy

**DO:** Sanitize sensitive data from metrics

```cpp
// ✅ DO: Sanitize PII from metrics
auto sanitized_endpoint = sanitize_endpoint(request.uri());
monitor->record_counter("api_requests", 1.0, {
    {"endpoint", sanitized_endpoint}  // /users/:id, not /users/john.doe@email.com
});

// ✅ DO: Hash sensitive identifiers
auto hashed_user = hash_user_id(user_id);
monitor->record_counter("user_actions", 1.0, {
    {"user_hash", hashed_user}
});

// ❌ DON'T: Include PII in metrics
monitor->record_counter("api_requests", 1.0, {
    {"email", user_email},        // PII!
    {"ip_address", client_ip},    // PII!
    {"ssn", user_ssn}             // PII!
});
```

---

### Access Control

**DO:** Restrict dashboard access

```cpp
// ✅ DO: Enable authentication
auto dashboard_config = dashboard_config::builder()
    .with_port(8080)
    .with_authentication(true)
    .with_auth_provider(auth_provider::oauth2)
    .with_oauth_config({
        {"client_id", std::getenv("OAUTH_CLIENT_ID")},
        {"client_secret", std::getenv("OAUTH_CLIENT_SECRET")},
        {"auth_url", "https://oauth.company.com/authorize"}
    })
    .with_rbac(true)
    .with_roles({
        {"admin", {"read", "write", "admin"}},
        {"developer", {"read"}},
        {"oncall", {"read", "silence_alerts"}}
    })
    .build();

monitor->start_dashboard(dashboard_config);
```

---

## Observability Patterns

### RED Method (Requests, Errors, Duration)

**DO:** Implement RED metrics for services

```cpp
// ✅ DO: RED metrics for every service
class ServiceMetrics {
public:
    void record_request(const Request& req, const Response& res, double duration_s) {
        // Rate: Requests per second
        monitor_->record_counter("service_requests_total", 1.0, {
            {"method", req.method()},
            {"endpoint", req.endpoint()}
        });

        // Errors: Error rate
        if (res.status() >= 500) {
            monitor_->record_counter("service_errors_total", 1.0, {
                {"method", req.method()},
                {"endpoint", req.endpoint()},
                {"status", std::to_string(res.status())}
            });
        }

        // Duration: Latency distribution
        monitor_->record_histogram("service_request_duration_seconds", duration_s, {
            {"method", req.method()},
            {"endpoint", req.endpoint()}
        });
    }

private:
    std::shared_ptr<monitoring_system> monitor_;
};
```

---

### USE Method (Utilization, Saturation, Errors)

**DO:** Implement USE metrics for resources

```cpp
// ✅ DO: USE metrics for resources
class ResourceMetrics {
public:
    void collect() {
        // Utilization: % of time resource is busy
        monitor_->record_gauge("cpu_utilization_percent", cpu_usage());
        monitor_->record_gauge("memory_utilization_percent", memory_usage());
        monitor_->record_gauge("disk_utilization_percent", disk_usage());

        // Saturation: Queue depth, wait time
        monitor_->record_gauge("cpu_load_average_1m", load_average_1m());
        monitor_->record_gauge("disk_queue_length", disk_queue_length());
        monitor_->record_gauge("network_buffer_usage_percent", net_buffer_usage());

        // Errors: Error counts
        monitor_->record_counter("disk_errors_total", disk_errors());
        monitor_->record_counter("network_errors_total", network_errors());
    }

private:
    std::shared_ptr<monitoring_system> monitor_;
};
```

---

### Golden Signals (Latency, Traffic, Errors, Saturation)

**DO:** Implement Golden Signals for SRE

```cpp
// ✅ DO: Golden Signals
class GoldenSignals {
public:
    void record_request(const Request& req, const Response& res, double latency_s) {
        // 1. Latency: How long does it take?
        monitor_->record_histogram("request_latency_seconds", latency_s, {
            {"endpoint", req.endpoint()}
        });

        // 2. Traffic: How much demand?
        monitor_->record_counter("request_count_total", 1.0, {
            {"endpoint", req.endpoint()},
            {"method", req.method()}
        });

        // 3. Errors: How many requests fail?
        if (res.status() >= 400) {
            monitor_->record_counter("error_count_total", 1.0, {
                {"endpoint", req.endpoint()},
                {"status_class", std::to_string(res.status() / 100) + "xx"}
            });
        }

        // 4. Saturation: How full is the service?
        monitor_->record_gauge("connection_pool_utilization", pool_utilization());
        monitor_->record_gauge("thread_pool_queue_length", queue_length());
        monitor_->record_gauge("memory_pressure_percent", memory_pressure());
    }

private:
    std::shared_ptr<monitoring_system> monitor_;
};
```

---

## Common Anti-Patterns

### Anti-Pattern 1: Monitoring as an Afterthought

**❌ DON'T:**
```cpp
// Add monitoring after everything else is done
void process_request() {
    // Complex business logic ...
    // 500 lines of code ...
    // Finally, at the end:
    monitor->record_counter("requests", 1.0);  // Missing context!
}
```

**✅ DO:**
```cpp
// Design with observability from the start
void process_request(const Request& req) {
    auto trace = tracer->start_trace("process_request");

    auto start = std::chrono::steady_clock::now();

    try {
        // Business logic with instrumentation
        auto result = business_logic(req, trace);

        auto duration_s = elapsed_seconds(start);
        monitor->record_histogram("request_duration_s", duration_s);
        monitor->record_counter("requests_total", 1.0, {{"status", "success"}});

        return result;

    } catch (const std::exception& e) {
        auto duration_s = elapsed_seconds(start);
        monitor->record_histogram("request_duration_s", duration_s);
        monitor->record_counter("requests_total", 1.0, {{"status", "error"}});
        monitor->record_counter("errors_total", 1.0, {{"type", typeid(e).name()}});

        trace->set_tag("error", true);
        trace->log_event("exception", e.what());

        throw;
    }
}
```

---

### Anti-Pattern 2: Alert Spam

**❌ DON'T:**
```cpp
// Fire alerts on every anomaly
monitor->add_alert_rule(alert_rule::builder()
    .with_name("disk_usage")
    .with_condition(condition_type::greater_than, 50.0)  // Too sensitive!
    .with_severity(severity_level::critical)  // Everything is critical!
    .build());
```

**✅ DO:**
```cpp
// Thoughtful alerting with escalation
monitor->add_alert_rule(alert_rule::builder()
    .with_name("disk_usage_warning")
    .with_condition(condition_type::greater_than, 80.0)
    .with_duration(std::chrono::minutes(15))
    .with_severity(severity_level::warning)
    .with_throttle(std::chrono::hours(1))
    .build());

monitor->add_alert_rule(alert_rule::builder()
    .with_name("disk_usage_critical")
    .with_condition(condition_type::greater_than, 95.0)
    .with_duration(std::chrono::minutes(5))
    .with_severity(severity_level::critical)
    .build());
```

---

### Anti-Pattern 3: High-Cardinality Labels

**❌ DON'T:**
```cpp
// User ID as label = millions of time series
monitor->record_counter("api_requests", 1.0, {
    {"user_id", user_id}  // BAD!
});
```

**✅ DO:**
```cpp
// Use tracing for high-cardinality data
auto trace = tracer->start_trace("api_request");
trace->set_tag("user_id", user_id);  // In trace, not metric

// Low-cardinality metrics
monitor->record_counter("api_requests", 1.0, {
    {"endpoint", endpoint},
    {"user_tier", user_tier}  // "free", "premium", "enterprise"
});
```

---

## Additional Resources

### Documentation
- **[FAQ](FAQ.md)** - 25+ frequently asked questions
- **[Quick Start](QUICK_START.md)** - Get started in 5 minutes
- **[API Reference](../API_REFERENCE.md)** - Complete API documentation
- **[Troubleshooting](../TROUBLESHOOTING.md)** - Common issues

### Community
- **GitHub Issues**: [Report issues](https://github.com/kcenon/monitoring_system/issues)
- **Discussions**: [Ask questions](https://github.com/kcenon/monitoring_system/discussions)
- **Contributing**: [CONTRIBUTING.md](../CONTRIBUTING.md)

---

**Last Updated:** 2025-11-11
**Next Review:** 2026-02-11
