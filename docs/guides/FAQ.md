# Monitoring System - Frequently Asked Questions

> **Version:** 0.1.0
> **Last Updated:** 2025-11-11
> **Audience:** Users, Developers, DevOps Engineers

This FAQ addresses common questions about the monitoring_system, covering setup, usage, integration, performance, and troubleshooting.

---

## Table of Contents

1. [General Questions](#general-questions)
2. [Installation & Setup](#installation--setup)
3. [Metrics Collection](#metrics-collection)
4. [Storage & Performance](#storage--performance)
5. [Alerting System](#alerting-system)
6. [Distributed Tracing](#distributed-tracing)
7. [Integration](#integration)
8. [Troubleshooting](#troubleshooting)
9. [Advanced Topics](#advanced-topics)

---

## General Questions

### 1. What is the monitoring_system?

The monitoring_system is a production-ready observability platform for C++20 applications that provides:
- High-performance metrics collection
- Time-series data storage with compression
- Distributed tracing with correlation
- Real-time alerting engine
- Interactive web dashboard
- Health monitoring

**Key Features:**
```cpp
// Comprehensive monitoring
auto system = monitoring_system::builder()
    .with_metrics_collection()
    .with_distributed_tracing()
    .with_alerting_engine()
    .with_dashboard(8080)
    .build();
```

**Target Use Cases:**
- Microservices monitoring
- Application performance management (APM)
- System health checks
- Real-time alerting
- Performance profiling

---

### 2. What C++ standard does monitoring_system require?

**Required:** C++20 or later

**Why C++20:**
- Concepts for compile-time interface validation
- Coroutines for async operations (optional)
- Ranges for efficient data processing
- std::span for zero-copy views
- Enhanced constexpr for compile-time optimization

**Compiler Support:**
- GCC 11+
- Clang 14+
- MSVC 2019 16.11+
- Apple Clang 13+

---

### 3. How does monitoring_system compare to Prometheus or Grafana?

**monitoring_system** is a C++-native solution designed for embedded monitoring:

| Feature | monitoring_system | Prometheus | Grafana |
|---------|-------------------|------------|---------|
| Language | C++20 | Go | TypeScript/Go |
| Deployment | Embedded in app | Separate service | Separate service |
| Latency | Sub-microsecond | Milliseconds | Seconds (UI) |
| Memory | Low (embedded) | High (separate) | High (UI) |
| Integration | Native C++ API | HTTP/gRPC | Data source plugins |
| Use Case | In-process | Centralized | Visualization |

**When to Choose monitoring_system:**
- ✅ Low-latency requirements (<1ms)
- ✅ Embedded systems with limited resources
- ✅ C++ applications requiring native integration
- ✅ Real-time monitoring without network overhead

**When to Choose Prometheus/Grafana:**
- Multi-language ecosystem
- Centralized monitoring across services
- Rich visualization requirements
- Existing Prometheus ecosystem

---

## Installation & Setup

### 4. How do I install monitoring_system?

**Method 1: FetchContent (Recommended)**
```cmake
include(FetchContent)

FetchContent_Declare(
    monitoring_system
    GIT_REPOSITORY https://github.com/kcenon/monitoring_system.git
    GIT_TAG v1.0.0
)

FetchContent_MakeAvailable(monitoring_system)

target_link_libraries(your_app PRIVATE monitoring_system::monitoring_system)
```

**Method 2: Git Submodule**
```bash
git submodule add https://github.com/kcenon/monitoring_system.git third_party/monitoring_system
```

```cmake
add_subdirectory(third_party/monitoring_system)
target_link_libraries(your_app PRIVATE monitoring_system::monitoring_system)
```

**Method 3: System Install**
```bash
git clone https://github.com/kcenon/monitoring_system.git
cd monitoring_system
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build . --target install
```

---

### 5. What are the minimum dependencies?

**Core Dependencies:**
- C++20 compiler
- CMake 3.16+
- common_system (Result<T> pattern)

**Optional Dependencies:**
```cmake
# Thread system integration
set(USE_THREAD_SYSTEM ON)

# Logger system integration
set(BUILD_WITH_LOGGER_SYSTEM ON)

# Enable dashboard
set(ENABLE_DASHBOARD ON)  # Requires web framework

# Enable benchmarks
set(BUILD_BENCHMARKS ON)  # Requires Google Benchmark
```

**Platform-Specific:**
- Linux: pthreads
- macOS: System frameworks
- Windows: Windows SDK

---

### 6. How do I create my first monitoring setup?

**5-Minute Quick Start:**

```cpp
#include <monitoring_system/monitoring_system.hpp>

using namespace kcenon::monitoring;

int main() {
    // 1. Create monitoring system
    auto result = monitoring_system::create();
    if (!result.has_value()) {
        std::cerr << "Failed to create monitoring system\n";
        return 1;
    }
    auto monitor = std::move(result.value());

    // 2. Record metrics
    monitor->record_counter("requests_total", 1.0);
    monitor->record_gauge("memory_usage_bytes", 1024 * 1024);
    monitor->record_histogram("request_duration_ms", 42.5);

    // 3. Query metrics
    auto metrics = monitor->get_metrics("requests_total");
    if (metrics.has_value()) {
        std::cout << "Total requests: " << metrics.value().value << "\n";
    }

    return 0;
}
```

**Build:**
```bash
g++ -std=c++20 -o my_app main.cpp -lmonitoring_system
./my_app
```

---

## Metrics Collection

### 7. What metric types are supported?

**Four Core Metric Types:**

**1. Counter** - Monotonically increasing value
```cpp
// Use for: requests, errors, bytes sent
monitor->record_counter("http_requests_total", 1.0, {
    {"method", "GET"},
    {"status", "200"}
});
```

**2. Gauge** - Point-in-time value (can increase/decrease)
```cpp
// Use for: memory usage, temperature, queue size
monitor->record_gauge("queue_size", 42.0);
monitor->record_gauge("cpu_temperature_celsius", 65.5);
```

**3. Histogram** - Distribution of values
```cpp
// Use for: latencies, request sizes
monitor->record_histogram("request_duration_seconds", 0.042, {
    {"endpoint", "/api/users"}
});
```

**4. Summary** - Similar to histogram, with percentiles
```cpp
// Use for: percentile tracking (p50, p95, p99)
monitor->record_summary("response_time_ms", 15.3);
```

---

### 8. How do I add labels/tags to metrics?

**Labels** provide multi-dimensional metrics:

```cpp
#include <monitoring_system/types/metric_labels.hpp>

// Method 1: Inline labels
monitor->record_counter("api_requests", 1.0, {
    {"method", "POST"},
    {"endpoint", "/api/users"},
    {"status", "201"}
});

// Method 2: Reusable labels
metric_labels labels;
labels.add("service", "auth");
labels.add("region", "us-west-2");
labels.add("env", "production");

monitor->record_gauge("active_connections", 150.0, labels);

// Method 3: Label builder
auto labels = metric_labels::builder()
    .with("app", "payment_service")
    .with("version", "v2.1.0")
    .with("instance", "pod-abc-123")
    .build();
```

**Best Practices:**
- ⚠️ **Cardinality Warning**: Avoid high-cardinality labels (e.g., user IDs, request IDs)
- ✅ Use: service, endpoint, method, status, region
- ❌ Avoid: user_id, session_id, trace_id (use tracing instead)

---

### 9. How can I collect metrics from multiple threads safely?

monitoring_system is **thread-safe** by default:

```cpp
#include <thread>
#include <vector>

auto monitor = monitoring_system::create().value();

// Multiple threads recording metrics concurrently
std::vector<std::thread> workers;
for (int i = 0; i < 10; ++i) {
    workers.emplace_back([&monitor, i]() {
        for (int j = 0; j < 1000; ++j) {
            monitor->record_counter("worker_operations", 1.0, {
                {"worker_id", std::to_string(i)}
            });
        }
    });
}

for (auto& t : workers) {
    t.join();
}
```

**Performance Optimizations:**
- **Thread-local buffering**: Reduces contention
- **Lock-free queues**: For high-throughput scenarios
- **Batch recording**: Reduce per-metric overhead

```cpp
// Batch recording for performance
std::vector<metric_sample> samples;
samples.push_back({"requests", 1.0, metric_type::counter});
samples.push_back({"errors", 0.0, metric_type::counter});
samples.push_back({"latency", 42.5, metric_type::histogram});

monitor->record_batch(samples);  // Single lock acquisition
```

---

### 10. How do I create custom metric collectors?

**Implement `metric_collector_interface`:**

```cpp
#include <monitoring_system/interfaces/metric_collector_interface.hpp>

class DatabaseCollector : public metric_collector_interface {
public:
    auto collect() -> Result<std::vector<metric_sample>> override {
        std::vector<metric_sample> metrics;

        // Collect connection pool metrics
        auto pool_stats = db_pool_->get_statistics();
        metrics.push_back({
            "db_connections_active",
            static_cast<double>(pool_stats.active),
            metric_type::gauge
        });
        metrics.push_back({
            "db_connections_idle",
            static_cast<double>(pool_stats.idle),
            metric_type::gauge
        });

        // Collect query performance
        auto query_stats = db_->get_query_stats();
        metrics.push_back({
            "db_query_duration_avg_ms",
            query_stats.avg_duration_ms,
            metric_type::gauge
        });

        return Result<std::vector<metric_sample>>::ok(std::move(metrics));
    }

    auto name() const -> std::string override {
        return "database_collector";
    }

private:
    std::shared_ptr<db_connection_pool> db_pool_;
    std::shared_ptr<database_interface> db_;
};

// Register collector
auto collector = std::make_shared<DatabaseCollector>();
monitor->register_collector(collector);

// Automatic collection every 15 seconds
monitor->start_collection(std::chrono::seconds(15));
```

---

## Storage & Performance

### 11. How is metric data stored?

**Storage Architecture:**

```cpp
// 1. In-memory ring buffer (default)
auto config = storage_config::builder()
    .with_backend(storage_backend::in_memory)
    .with_retention(std::chrono::hours(24))
    .with_max_samples(1'000'000)
    .build();

// 2. File-based storage (persistent)
auto config = storage_config::builder()
    .with_backend(storage_backend::file)
    .with_path("/var/lib/monitoring/data")
    .with_compression(compression_type::zstd)
    .build();

// 3. Custom backend
class RedisStorage : public storage_backend_interface {
    // Implement interface
};
```

**Storage Options:**

| Backend | Latency | Retention | Persistence | Use Case |
|---------|---------|-----------|-------------|----------|
| In-Memory | <1μs | Limited | No | Real-time monitoring |
| File-Based | <100μs | Unlimited | Yes | Long-term storage |
| Custom | Varies | Custom | Custom | Integration with external systems |

---

### 12. What is the performance overhead of monitoring?

**Benchmarked Performance** (on 3.2 GHz Intel Core i7):

| Operation | Latency | Throughput |
|-----------|---------|------------|
| record_counter | 80 ns | 12.5M ops/s |
| record_gauge | 85 ns | 11.7M ops/s |
| record_histogram | 120 ns | 8.3M ops/s |
| get_metrics | 200 ns | 5M ops/s |
| distributed trace | 300 ns | 3.3M ops/s |

**Memory Overhead:**
- Base system: ~2 MB
- Per metric: ~200 bytes
- Per trace span: ~500 bytes
- Ring buffer: Configurable (default 1M samples ≈ 200 MB)

**Optimization Tips:**
```cpp
// 1. Use thread-local collectors
monitor->enable_thread_local_collection();

// 2. Batch recording
monitor->record_batch(samples);

// 3. Sampling for high-frequency metrics
monitor->set_sampling_rate("high_freq_metric", 0.01);  // 1% sampling

// 4. Disable debug metrics in production
monitor->set_level(monitoring_level::production);
```

---

### 13. How do I configure data retention?

**Retention Policies:**

```cpp
#include <monitoring_system/storage/retention_policy.hpp>

// Time-based retention
auto policy = retention_policy::builder()
    .with_max_age(std::chrono::hours(24))
    .build();

// Size-based retention
auto policy = retention_policy::builder()
    .with_max_samples(1'000'000)
    .with_cleanup_threshold(0.9)  // Cleanup at 90% capacity
    .build();

// Combined policy
auto policy = retention_policy::builder()
    .with_max_age(std::chrono::days(7))
    .with_max_samples(10'000'000)
    .with_max_size_bytes(1024 * 1024 * 1024)  // 1 GB
    .build();

monitor->set_retention_policy(policy);

// Aggregation for long-term storage
auto aggregation = aggregation_policy::builder()
    .aggregate_after(std::chrono::hours(1))
    .with_resolution(std::chrono::minutes(5))
    .with_aggregations({
        aggregation_type::mean,
        aggregation_type::min,
        aggregation_type::max,
        aggregation_type::p95
    })
    .build();
```

---

## Alerting System

### 14. How do I create alerts?

**Alert Rule Definition:**

```cpp
#include <monitoring_system/alerting/alert_rule.hpp>

// Simple threshold alert
auto rule = alert_rule::builder()
    .with_name("high_cpu_usage")
    .with_metric("cpu_usage_percent")
    .with_condition(condition_type::greater_than, 80.0)
    .with_duration(std::chrono::minutes(5))
    .with_severity(severity_level::warning)
    .with_message("CPU usage above 80% for 5 minutes")
    .build();

monitor->add_alert_rule(rule);

// Complex alert with labels
auto rule = alert_rule::builder()
    .with_name("api_error_rate_high")
    .with_metric("api_errors_total")
    .with_labels({{"service", "payment"}, {"env", "prod"}})
    .with_condition(condition_type::rate_increase, 5.0)  // 5x increase
    .with_evaluation_window(std::chrono::minutes(10))
    .with_severity(severity_level::critical)
    .with_annotations({
        {"runbook", "https://wiki.company.com/runbooks/payment-errors"},
        {"dashboard", "https://grafana.company.com/d/payment"}
    })
    .build();
```

**Alert Conditions:**
- `greater_than`, `less_than`, `equal_to`
- `rate_increase`, `rate_decrease`
- `absent` (metric missing)
- `anomaly` (statistical anomaly detection)

---

### 15. How do I receive alert notifications?

**Multi-Channel Notifications:**

```cpp
#include <monitoring_system/alerting/notification_channel.hpp>

// Email notifications
auto email_channel = email_notification_channel::builder()
    .with_smtp_server("smtp.company.com", 587)
    .with_credentials("alerts@company.com", "password")
    .with_recipients({"oncall@company.com", "team-lead@company.com"})
    .build();

// Slack notifications
auto slack_channel = slack_notification_channel::builder()
    .with_webhook_url("https://hooks.slack.com/services/XXX")
    .with_channel("#alerts")
    .with_username("Monitoring Bot")
    .build();

// PagerDuty integration
auto pagerduty = pagerduty_notification_channel::builder()
    .with_api_key("your-api-key")
    .with_service_key("service-key")
    .with_severity_mapping({
        {severity_level::critical, "critical"},
        {severity_level::warning, "warning"}
    })
    .build();

// Register channels
monitor->add_notification_channel("email", email_channel);
monitor->add_notification_channel("slack", slack_channel);
monitor->add_notification_channel("pagerduty", pagerduty);

// Route alerts by severity
monitor->set_routing_rules({
    {severity_level::critical, {"pagerduty", "slack", "email"}},
    {severity_level::warning, {"slack", "email"}},
    {severity_level::info, {"slack"}}
});
```

---

### 16. How do I prevent alert fatigue?

**Alert Management Strategies:**

```cpp
// 1. Alert grouping
monitor->set_alert_grouping({
    .group_by = {"service", "env"},
    .group_interval = std::chrono::minutes(5)
});

// 2. Alert inhibition (suppress related alerts)
monitor->add_inhibition_rule({
    .source_alert = "service_down",
    .target_alerts = {"high_latency", "error_rate_high"},
    .equal_labels = {"service"}  // Only inhibit if same service
});

// 3. Silence windows
monitor->add_silence({
    .matchers = {{"env", "staging"}},
    .start_time = now(),
    .end_time = now() + std::chrono::hours(2),
    .reason = "Maintenance window"
});

// 4. Alert throttling
auto rule = alert_rule::builder()
    .with_name("disk_space_low")
    .with_throttle(std::chrono::hours(1))  // Max once per hour
    .build();

// 5. Escalation policies
monitor->set_escalation_policy("critical_alerts", {
    {std::chrono::minutes(0), {"slack"}},
    {std::chrono::minutes(5), {"email"}},
    {std::chrono::minutes(15), {"pagerduty"}}
});
```

---

## Distributed Tracing

### 17. How do I implement distributed tracing?

**Basic Tracing:**

```cpp
#include <monitoring_system/tracing/distributed_tracer.hpp>

auto tracer = monitor->get_tracer();

// Start a trace
auto trace_ctx = tracer->start_trace("user_request");
auto trace_id = trace_ctx.trace_id();

// Create spans
{
    auto span = tracer->start_span("database_query", trace_ctx);
    span->set_tag("query_type", "SELECT");
    span->set_tag("table", "users");

    // Perform operation
    auto result = db->query("SELECT * FROM users WHERE id = ?", user_id);

    if (!result.has_value()) {
        span->set_tag("error", true);
        span->log_event("query_failed", result.error().message());
    }

    // Span automatically closed when goes out of scope
}

// Nested spans
{
    auto parent_span = tracer->start_span("process_request", trace_ctx);

    {
        auto child_span = tracer->start_span("validate_input", trace_ctx);
        // ... validation logic
    }

    {
        auto child_span = tracer->start_span("business_logic", trace_ctx);
        // ... business logic
    }
}

// Finish trace
tracer->finish_trace(trace_ctx);
```

---

### 18. How do I propagate trace context across services?

**Inter-Service Tracing:**

```cpp
// Service A: Propagate context
auto trace_ctx = tracer->start_trace("api_request");

// Serialize context for transmission
auto headers = tracer->inject_context(trace_ctx);
// headers contains: X-Trace-Id, X-Span-Id, X-Parent-Span-Id

// Send HTTP request with headers
http_client client;
auto response = client.post("http://service-b/api/process",
                            request_data,
                            headers);

// Service B: Extract context
auto extracted_ctx = tracer->extract_context(request.headers());
if (extracted_ctx.has_value()) {
    // Continue trace in service B
    auto span = tracer->start_span("handle_request", extracted_ctx.value());
    // ... process request
}
```

**Supported Propagation Formats:**
- W3C Trace Context (default)
- B3 (Zipkin)
- Jaeger
- Custom format

```cpp
tracer->set_propagation_format(propagation_format::w3c_trace_context);
```

---

### 19. Can I integrate with OpenTelemetry or Jaeger?

**Yes! Export to standard tracing backends:**

```cpp
#include <monitoring_system/exporters/otlp_exporter.hpp>
#include <monitoring_system/exporters/jaeger_exporter.hpp>

// OpenTelemetry Exporter
auto otlp_exporter = otlp_exporter::builder()
    .with_endpoint("http://otel-collector:4317")
    .with_compression(true)
    .build();

monitor->add_trace_exporter(otlp_exporter);

// Jaeger Exporter
auto jaeger_exporter = jaeger_exporter::builder()
    .with_endpoint("http://jaeger:14268/api/traces")
    .with_service_name("my_service")
    .with_tags({{"env", "production"}})
    .build();

monitor->add_trace_exporter(jaeger_exporter);

// Sampling for high-volume services
tracer->set_sampling_strategy(
    sampling_strategy::probabilistic(0.1)  // Sample 10%
);
```

---

## Integration

### 20. How do I integrate with thread_system?

**Automatic Thread Pool Monitoring:**

```cpp
#include <monitoring_system/adapters/thread_system_adapter.hpp>

// Enable thread_system integration
auto config = monitoring_config::builder()
    .with_thread_system_integration()
    .build();

auto monitor = monitoring_system::create(config).value();

// Automatically collects:
// - thread_pool_active_threads
// - thread_pool_queued_tasks
// - thread_pool_completed_tasks
// - thread_pool_task_duration_ms

// Manual integration
auto thread_pool = thread_system::create_pool(4);
monitor->register_thread_pool("worker_pool", thread_pool);

// Metrics automatically updated
auto stats = monitor->get_metrics("thread_pool_queued_tasks", {
    {"pool_name", "worker_pool"}
});
```

---

### 21. How do I integrate with logger_system?

**Unified Logging and Monitoring:**

```cpp
#include <monitoring_system/adapters/logger_system_adapter.hpp>

// Correlate logs with metrics
auto config = monitoring_config::builder()
    .with_logger_system_integration()
    .with_log_level_metrics(true)
    .build();

// Automatic metrics:
// - log_messages_total{level="error"}
// - log_messages_total{level="warn"}
// - log_messages_total{level="info"}

// Add trace ID to logs
auto trace_ctx = tracer->start_trace("request");
logger->info("Processing request", {
    {"trace_id", trace_ctx.trace_id()},
    {"span_id", trace_ctx.span_id()}
});
```

---

## Troubleshooting

### 22. Why are my metrics not appearing?

**Common Issues:**

**1. Metric Not Registered:**
```cpp
// Ensure metric is recorded at least once
monitor->record_counter("my_metric", 0.0);  // Initialize
```

**2. Labels Mismatch:**
```cpp
// These create different metric series:
monitor->record_counter("requests", 1.0, {{"method", "GET"}});
monitor->record_counter("requests", 1.0, {{"method", "POST"}});

// Query with correct labels:
auto metrics = monitor->get_metrics("requests", {{"method", "GET"}});
```

**3. Retention Policy:**
```cpp
// Check if metrics expired
auto policy = monitor->get_retention_policy();
std::cout << "Max age: " << policy.max_age().count() << " seconds\n";
```

**4. Storage Backend Full:**
```cpp
auto stats = monitor->get_storage_stats();
std::cout << "Used: " << stats.used_samples
          << "/" << stats.max_samples << "\n";
```

---

### 23. How do I debug performance issues?

**Enable Debug Metrics:**

```cpp
// 1. Enable internal monitoring
monitor->enable_self_monitoring();

// Metrics available:
// - monitoring_record_duration_us
// - monitoring_query_duration_us
// - monitoring_storage_operations

// 2. Check operation latencies
auto latency = monitor->get_metrics("monitoring_record_duration_us");
if (latency.value().value > 1000.0) {  // > 1ms
    std::cerr << "High recording latency detected\n";
}

// 3. Profile with built-in profiler
monitor->enable_profiling();
// ... run workload ...
auto profile = monitor->get_profile_report();
profile.print_top_n(10);

// 4. Check thread contention
auto contention = monitor->get_contention_stats();
for (const auto& [lock_name, wait_time] : contention) {
    std::cout << lock_name << ": " << wait_time.count() << " ms\n";
}
```

---

## Advanced Topics

### 24. How do I implement custom storage backends?

**Implement `storage_backend_interface`:**

```cpp
#include <monitoring_system/interfaces/storage_backend_interface.hpp>

class PostgresStorage : public storage_backend_interface {
public:
    auto store(const metric_sample& sample) -> Result<void> override {
        // Store in PostgreSQL
        auto query = "INSERT INTO metrics (name, value, timestamp, labels) "
                     "VALUES ($1, $2, $3, $4)";
        return db_->execute(query, sample.name, sample.value,
                           sample.timestamp, sample.labels);
    }

    auto query(const metric_query& query) -> Result<std::vector<metric_sample>> override {
        // Query from PostgreSQL
        auto sql = build_query_sql(query);
        return db_->query(sql);
    }

    auto delete_before(std::chrono::system_clock::time_point cutoff)
        -> Result<size_t> override {
        auto query = "DELETE FROM metrics WHERE timestamp < $1";
        return db_->execute(query, cutoff);
    }

private:
    std::shared_ptr<database_connection> db_;

    auto build_query_sql(const metric_query& query) -> std::string {
        // Build SQL from query parameters
        // ...
    }
};

// Register custom backend
auto storage = std::make_shared<PostgresStorage>(db_connection);
monitor->set_storage_backend(storage);
```

---

### 25. How do I export metrics to Prometheus?

**Prometheus Exposition:**

```cpp
#include <monitoring_system/exporters/prometheus_exporter.hpp>

// Start Prometheus metrics endpoint
auto exporter = prometheus_exporter::builder()
    .with_port(9090)
    .with_path("/metrics")
    .with_help_text(true)
    .build();

monitor->add_exporter(exporter);
exporter->start();

// Metrics available at http://localhost:9090/metrics in Prometheus format:
// # HELP http_requests_total Total HTTP requests
// # TYPE http_requests_total counter
// http_requests_total{method="GET",status="200"} 1234
// http_requests_total{method="POST",status="201"} 567
```

**Prometheus Scrape Config:**
```yaml
scrape_configs:
  - job_name: 'my_cpp_app'
    static_configs:
      - targets: ['localhost:9090']
    scrape_interval: 15s
```

---

## Additional Resources

### Documentation
- **[Quick Start Guide](QUICK_START.md)** - Get started in 5 minutes
- **[API Reference](../API_REFERENCE.md)** - Complete API documentation
- **[Architecture Guide](../ARCHITECTURE_GUIDE.md)** - System design
- **[Best Practices](BEST_PRACTICES.md)** - Production patterns
- **[Troubleshooting](../TROUBLESHOOTING.md)** - Common issues

### Support
- **GitHub Issues**: [Report bugs](https://github.com/kcenon/monitoring_system/issues)
- **GitHub Discussions**: [Ask questions](https://github.com/kcenon/monitoring_system/discussions)
- **Contributing**: [CONTRIBUTING.md](../CONTRIBUTING.md)

---

**Last Updated:** 2025-11-11
**Next Review:** 2026-02-11
