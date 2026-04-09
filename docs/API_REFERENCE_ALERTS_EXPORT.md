---
doc_id: "MON-API-ALERT-001"
doc_title: "Monitoring System API Reference - Alerts, Exporters, Tracing"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "monitoring_system"
category: "API"
---

# Monitoring System API Reference - Alerts, Exporters, Tracing

> **SSOT**: This document is the single source of truth for **Monitoring System API - Alerts, Exporters, Tracing** and related subsystems (health monitoring, storage backends, stream processing, reliability features, OpenTelemetry integration).

For core components and performance monitoring, see [API_REFERENCE_CORE.md](API_REFERENCE_CORE.md).
For collector classes, see [API_REFERENCE_COLLECTORS.md](API_REFERENCE_COLLECTORS.md).

## Table of Contents

1. [Health Monitoring](#health-monitoring)
2. [Distributed Tracing](#distributed-tracing)
3. [Storage Backends](#storage-backends)
4. [Stream Processing](#stream-processing)
5. [Reliability Features](#reliability-features)
6. [OpenTelemetry Integration](#opentelemetry-integration)
7. [Usage Examples](#usage-examples)

---

## Health Monitoring


### Health Monitor
**Header:** `sources/monitoring/health/health_monitor.h`

#### `health_monitor`
Manages health checks and service dependencies.

```cpp
class health_monitor {
public:
    health_monitor(const health_monitor_config& config = {});
    
    // Register health checks
    common::Result<bool> register_check(const std::string& name,
                                std::shared_ptr<health_check> check);

    // Add dependencies
    common::Result<bool> add_dependency(const std::string& dependent,
                                const std::string& dependency);

    // Perform checks
    common::Result<health_check_result> check(const std::string& name);
    std::unordered_map<std::string, health_check_result> check_all();
    
    // Get status
    health_status get_overall_status() const;
    
    // Recovery handlers
    void register_recovery_handler(const std::string& check_name,
                                  std::function<bool()> handler);
};
```

#### `health_check`
Abstract base class for health checks.

```cpp
class health_check {
public:
    virtual std::string get_name() const = 0;
    virtual health_check_type get_type() const = 0;
    virtual health_check_result check() = 0;
    virtual std::chrono::milliseconds get_timeout() const;
    virtual bool is_critical() const;
};
```

#### Health Check Builder
```cpp
health_check_builder builder;
auto check = builder
    .with_name("database_check")
    .with_type(health_check_type::readiness)
    .with_check([]() { 
        // Check database connection
        return health_check_result::healthy("Database connected");
    })
    .with_timeout(5s)
    .critical(true)
    .build();
```

---

## Distributed Tracing

### Distributed Tracer
**Header:** `sources/monitoring/tracing/distributed_tracer.h`

#### `distributed_tracer`
Manages distributed traces across services.

```cpp
class distributed_tracer {
public:
    // Start spans
    common::Result<std::shared_ptr<trace_span>> start_span(
        const std::string& operation_name,
        const std::string& service_name = "monitoring_system");

    common::Result<std::shared_ptr<trace_span>> start_child_span(
        const trace_span& parent,
        const std::string& operation_name);

    // Context propagation
    trace_context extract_context(const trace_span& span) const;

    template<typename Carrier>
    void inject_context(const trace_context& context, Carrier& carrier);

    template<typename Carrier>
    common::Result<trace_context> extract_context_from_carrier(const Carrier& carrier);

    // Finish span
    common::Result<bool> finish_span(std::shared_ptr<trace_span> span);
};
```

#### `trace_span`
Represents a unit of work in distributed tracing.

```cpp
struct trace_span {
    std::string trace_id;
    std::string span_id;
    std::string parent_span_id;
    std::string operation_name;
    std::string service_name;
    
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    
    std::unordered_map<std::string, std::string> tags;
    std::unordered_map<std::string, std::string> baggage;
    
    enum class status_code { unset, ok, error };
    status_code status;
    std::string status_message;
};
```

#### Scoped Span Macro
```cpp
// Automatically creates and finishes a span
TRACE_SPAN("database_query");

// Create child span
TRACE_CHILD_SPAN(parent_span, "nested_operation");
```

---

## Storage Backends

### Storage Backend Interface
**Header:** `sources/monitoring/storage/storage_backends.h`

#### `storage_backend`
Abstract base class for all storage implementations.

```cpp
class storage_backend {
public:
    virtual common::Result<bool> initialize() = 0;
    virtual common::Result<bool> write(const std::string& key, const std::string& value) = 0;
    virtual common::Result<std::string> read(const std::string& key) = 0;
    virtual common::Result<bool> remove(const std::string& key) = 0;
    virtual common::Result<std::vector<std::string>> list(const std::string& prefix = "") = 0;
    virtual common::Result<bool> flush() = 0;
    virtual storage_backend_type get_type() const = 0;
};
```

### Available Backends

#### File Storage Backends
- `file_storage_backend` - JSON file storage
- `binary_storage_backend` - Binary file storage
- `csv_storage_backend` - CSV file storage

#### Database Backends
- `sqlite_storage_backend` - SQLite database
- `postgres_storage_backend` - PostgreSQL database
- `mysql_storage_backend` - MySQL database

#### Cloud Storage Backends
- `s3_storage_backend` - Amazon S3
- `gcs_storage_backend` - Google Cloud Storage
- `azure_storage_backend` - Azure Blob Storage

#### Memory Backend
- `memory_buffer_backend` - In-memory storage

### Storage Factory
```cpp
auto storage = storage_factory::create(storage_backend_type::sqlite, config);
```

---

## Stream Processing

### Stream Aggregator
**Header:** `sources/monitoring/stream/stream_aggregator.h`

#### `stream_aggregator`
Performs real-time aggregation on metric streams.

```cpp
class stream_aggregator {
public:
    // Configure aggregation
    common::Result<bool> add_aggregation(const std::string& metric_name,
                                 aggregation_type type,
                                 std::chrono::seconds window);

    // Process values
    common::Result<bool> add_value(const std::string& metric_name, double value);

    // Get results
    common::Result<aggregation_result> get_aggregation(const std::string& metric_name);

    // Windowing
    common::Result<bool> set_window_size(std::chrono::seconds size);
    common::Result<bool> set_sliding_interval(std::chrono::seconds interval);
};
```

#### Aggregation Types
```cpp
enum class aggregation_type {
    sum,
    average,
    min,
    max,
    count,
    percentile_50,
    percentile_95,
    percentile_99,
    stddev,
    rate
};
```

### Buffering Strategies
**Header:** `sources/monitoring/stream/buffering_strategies.h`

#### `buffering_strategy`
Base class for different buffering strategies.

```cpp
class buffering_strategy {
public:
    virtual common::Result<bool> add(const metric_data& data) = 0;
    virtual common::Result<std::vector<metric_data>> get_batch() = 0;
    virtual bool should_flush() const = 0;
    virtual common::Result<bool> flush() = 0;
};
```

Available Strategies:
- `time_based_buffer` - Flush after time interval
- `size_based_buffer` - Flush after size threshold
- `adaptive_buffer` - Dynamic buffering based on load

---

## Reliability Features

### Circuit Breaker
**Header:** `sources/monitoring/reliability/circuit_breaker.h`

#### `circuit_breaker<T>`
Prevents cascading failures by breaking circuits to failing services.

```cpp
template<typename T>
class circuit_breaker {
public:
    circuit_breaker(std::string name, circuit_breaker_config config = {});
    
    // Execute with circuit breaker protection
    common::Result<T> execute(std::function<common::Result<T>()> operation,
                     std::function<common::Result<T>()> fallback = nullptr);
    
    // Manual control
    void open();
    void close();
    void half_open();
    
    // Get state
    circuit_state get_state() const;
    circuit_breaker_metrics get_metrics() const;
};
```

#### Circuit Breaker Configuration
```cpp
struct circuit_breaker_config {
    std::size_t failure_threshold = 5;
    double failure_ratio = 0.5;
    std::chrono::milliseconds timeout{5000};
    std::chrono::milliseconds reset_timeout{60000};
    std::size_t success_threshold = 3;
};
```

### Retry Policy
**Header:** `sources/monitoring/reliability/retry_policy.h`

#### `retry_policy<T>`
Implements retry logic with various strategies.

```cpp
template<typename T>
class retry_policy {
public:
    retry_policy(retry_config config = {});
    
    // Execute with retry
    common::Result<T> execute(std::function<common::Result<T>()> operation);

    // Execute async with retry
    std::future<common::Result<T>> execute_async(std::function<common::Result<T>()> operation);
};
```

#### Retry Strategies
```cpp
enum class retry_strategy {
    fixed_delay,        // Fixed delay between retries
    exponential_backoff,// Exponential increase in delay
    linear_backoff,     // Linear increase in delay
    fibonacci_backoff,  // Fibonacci sequence delays
    random_jitter      // Random delay with jitter
};
```

### Error Boundaries
**Header:** `sources/monitoring/reliability/error_boundaries.h`

#### `error_boundary`
Isolates errors to prevent system-wide failures.

```cpp
class error_boundary {
public:
    error_boundary(const std::string& name,
                  error_boundary_config config = {});
    
    // Execute within boundary
    template<typename T>
    common::Result<T> execute(std::function<common::Result<T>()> operation);
    
    // Set error handler
    void set_error_handler(std::function<void(const error_info&)> handler);
    
    // Get statistics
    error_boundary_stats get_stats() const;
};
```

---

## OpenTelemetry Integration

### OpenTelemetry Adapter
**Header:** `sources/monitoring/adapters/opentelemetry_adapter.h`

#### `opentelemetry_adapter`
Bridges monitoring system with OpenTelemetry.

```cpp
class opentelemetry_adapter {
public:
    opentelemetry_adapter(const otel_config& config = {});
    
    // Convert to OpenTelemetry formats
    common::Result<otel_span> convert_span(const trace_span& span);
    common::Result<otel_metric> convert_metric(const metric_data& metric);
    common::Result<otel_log> convert_log(const log_entry& log);

    // Export to OpenTelemetry
    common::Result<bool> export_traces(const std::vector<trace_span>& spans);
    common::Result<bool> export_metrics(const std::vector<metric_data>& metrics);
    common::Result<bool> export_logs(const std::vector<log_entry>& logs);
};
```

### Trace Exporters
**Header:** `sources/monitoring/exporters/trace_exporters.h`

Available Exporters:
- `jaeger_exporter` - Export to Jaeger
- `zipkin_exporter` - Export to Zipkin
- `otlp_exporter` - OTLP protocol exporter
- `console_exporter` - Console output for debugging

### Metric Exporters
**Header:** `sources/monitoring/exporters/metric_exporters.h`

Available Exporters:
- `prometheus_exporter` - Prometheus format
- `statsd_exporter` - StatsD protocol
- `influxdb_exporter` - InfluxDB line protocol
- `graphite_exporter` - Graphite plaintext protocol

---

## Usage Examples

### Health Check Setup
```cpp
// Create health monitor
health_monitor monitor;

// Register health checks
monitor.register_check("database", 
    health_check_builder()
        .with_name("database_check")
        .with_type(health_check_type::readiness)
        .with_check([]() { return check_database(); })
        .build()
);

// Add dependencies
monitor.add_dependency("api", "database");

// Start monitoring
monitor.start();
```

### Distributed Tracing
```cpp
// Start a trace
distributed_tracer& tracer = global_tracer();
auto span = tracer.start_span("process_request");

// Add tags
span.value()->tags["user_id"] = "12345";
span.value()->tags["endpoint"] = "/api/users";

// Create child span
auto child_span = tracer.start_child_span(*span.value(), "database_query");

// Finish spans
tracer.finish_span(child_span.value());
tracer.finish_span(span.value());
```

### Circuit Breaker Usage
```cpp
circuit_breaker_config config;
config.failure_threshold = 5;
config.reset_timeout = 30s;

circuit_breaker<std::string> breaker("external_api", config);

auto result = breaker.execute(
    []() { return call_external_api(); },
    []() -> common::Result<std::string> { return common::ok(std::string("fallback_value")); }
);
```

---

## See Also

- [API_REFERENCE.md](API_REFERENCE.md) - API Reference index
- [API_REFERENCE_CORE.md](API_REFERENCE_CORE.md) - Core components and performance monitor
- [API_REFERENCE_COLLECTORS.md](API_REFERENCE_COLLECTORS.md) - Collector class APIs
- [FEATURES_ALERTS_TRACING.md](FEATURES_ALERTS_TRACING.md) - Feature-level documentation for alerts, tracing, and exporters
