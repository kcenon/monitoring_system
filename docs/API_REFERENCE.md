# Monitoring System API Reference

## Table of Contents

1. [Core Components](#core-components)
2. [Monitoring Interfaces](#monitoring-interfaces)
3. [Performance Monitoring](#performance-monitoring)
4. [Health Monitoring](#health-monitoring)
5. [Distributed Tracing](#distributed-tracing)
6. [Storage Backends](#storage-backends)
7. [Stream Processing](#stream-processing)
8. [Reliability Features](#reliability-features)
9. [OpenTelemetry Integration](#opentelemetry-integration)

---

## Core Components

### Result Types
**Header:** `sources/monitoring/core/result_types.h`

#### `result<T>`
A monadic result type for error handling without exceptions.

```cpp
template<typename T>
class result {
public:
    // Check if result contains a value
    bool has_value() const;
    explicit operator bool() const;
    
    // Access the value (throws if error)
    T& value();
    const T& value() const;
    
    // Access the error
    error_info get_error() const;
    
    // Monadic operations
    template<typename F>
    auto and_then(F&& f);
    
    template<typename F>
    auto or_else(F&& f);
};
```

**Usage Example:**
```cpp
result<int> divide(int a, int b) {
    if (b == 0) {
        return make_error<int>(monitoring_error_code::invalid_argument);
    }
    return a / b;
}
```

### Thread Context
**Header:** `sources/monitoring/context/thread_context.h`

#### `thread_context`
Manages thread-local context for correlation and tracing.

```cpp
class thread_context {
public:
    // Get current thread context
    static context_metadata& current();
    
    // Create new context
    static context_metadata& create(const std::string& request_id = "");
    
    // Clear current context
    static void clear();
    
    // Generate IDs
    static std::string generate_request_id();
    static std::string generate_correlation_id();
};
```

### Dependency Injection Container
**Header:** `sources/monitoring/di/di_container.h`

#### `di_container`
Lightweight dependency injection container for managing service instances.

```cpp
class di_container {
public:
    // Register singleton
    template<typename Interface, typename Implementation>
    void register_singleton();
    
    // Register factory
    template<typename Interface>
    void register_factory(std::function<std::shared_ptr<Interface>()> factory);
    
    // Resolve dependency
    template<typename T>
    std::shared_ptr<T> resolve();
};
```

---

## Monitoring Interfaces

### Monitoring Interface
**Header:** `sources/monitoring/interfaces/monitoring_interface.h`

#### `metrics_collector`
Base interface for all metric collectors.

```cpp
class metrics_collector {
public:
    virtual std::string get_name() const = 0;
    virtual bool is_enabled() const = 0;
    virtual result_void set_enabled(bool enable) = 0;
    virtual result_void initialize() = 0;
    virtual result_void cleanup() = 0;
    virtual result<metrics_snapshot> collect() = 0;
};
```

#### `monitorable`
Interface for objects that can be monitored.

```cpp
class monitorable {
public:
    virtual std::string get_name() const = 0;
    virtual result<metrics_snapshot> get_metrics() const = 0;
    virtual result_void reset_metrics() = 0;
    virtual result<std::string> get_status() const = 0;
};
```

---

## Performance Monitoring

### Performance Monitor
**Header:** `sources/monitoring/performance/performance_monitor.h`

#### `performance_monitor`
Monitors system and application performance metrics.

```cpp
class performance_monitor : public metrics_collector {
public:
    explicit performance_monitor(const std::string& name = "performance_monitor");
    
    // Create scoped timer
    scoped_timer time_operation(const std::string& operation_name);
    
    // Get profiler
    performance_profiler& get_profiler();
    
    // Get system monitor
    system_monitor& get_system_monitor();
    
    // Set thresholds
    void set_cpu_threshold(double threshold);
    void set_memory_threshold(double threshold);
    void set_latency_threshold(std::chrono::milliseconds threshold);
};
```

#### `scoped_timer`
RAII timer for measuring operation duration.

```cpp
class scoped_timer {
public:
    scoped_timer(performance_profiler* profiler, const std::string& operation_name);
    ~scoped_timer();
    
    void mark_failed();
    void complete();
    std::chrono::nanoseconds elapsed() const;
};
```

### Adaptive Optimizer
**Header:** `sources/monitoring/performance/adaptive_optimizer.h`

#### `adaptive_optimizer`
Dynamically optimizes monitoring parameters based on system load.

```cpp
class adaptive_optimizer {
public:
    struct optimization_config {
        double cpu_threshold = 80.0;
        double memory_threshold = 90.0;
        double target_overhead = 5.0;
        std::chrono::seconds adaptation_interval{60};
    };
    
    explicit adaptive_optimizer(const optimization_config& config = {});
    
    result<bool> start();
    result<bool> stop();
    result<optimization_decision> analyze_and_optimize();
    result<bool> apply_optimization(const optimization_decision& decision);
};
```

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
    result<bool> register_check(const std::string& name,
                                std::shared_ptr<health_check> check);
    
    // Add dependencies
    result<bool> add_dependency(const std::string& dependent,
                                const std::string& dependency);
    
    // Perform checks
    result<health_check_result> check(const std::string& name);
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
    result<std::shared_ptr<trace_span>> start_span(
        const std::string& operation_name,
        const std::string& service_name = "monitoring_system");
    
    result<std::shared_ptr<trace_span>> start_child_span(
        const trace_span& parent,
        const std::string& operation_name);
    
    // Context propagation
    trace_context extract_context(const trace_span& span) const;
    
    template<typename Carrier>
    void inject_context(const trace_context& context, Carrier& carrier);
    
    template<typename Carrier>
    result<trace_context> extract_context_from_carrier(const Carrier& carrier);
    
    // Finish span
    result<bool> finish_span(std::shared_ptr<trace_span> span);
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
    virtual result<bool> initialize() = 0;
    virtual result<bool> write(const std::string& key, const std::string& value) = 0;
    virtual result<std::string> read(const std::string& key) = 0;
    virtual result<bool> remove(const std::string& key) = 0;
    virtual result<std::vector<std::string>> list(const std::string& prefix = "") = 0;
    virtual result<bool> flush() = 0;
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
    result<bool> add_aggregation(const std::string& metric_name,
                                 aggregation_type type,
                                 std::chrono::seconds window);
    
    // Process values
    result<bool> add_value(const std::string& metric_name, double value);
    
    // Get results
    result<aggregation_result> get_aggregation(const std::string& metric_name);
    
    // Windowing
    result<bool> set_window_size(std::chrono::seconds size);
    result<bool> set_sliding_interval(std::chrono::seconds interval);
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
    virtual result<bool> add(const metric_data& data) = 0;
    virtual result<std::vector<metric_data>> get_batch() = 0;
    virtual bool should_flush() const = 0;
    virtual result<bool> flush() = 0;
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
    result<T> execute(std::function<result<T>()> operation,
                     std::function<result<T>()> fallback = nullptr);
    
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
    result<T> execute(std::function<result<T>()> operation);
    
    // Execute async with retry
    std::future<result<T>> execute_async(std::function<result<T>()> operation);
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
    result<T> execute(std::function<result<T>()> operation);
    
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
    result<otel_span> convert_span(const trace_span& span);
    result<otel_metric> convert_metric(const metric_data& metric);
    result<otel_log> convert_log(const log_entry& log);
    
    // Export to OpenTelemetry
    result<bool> export_traces(const std::vector<trace_span>& spans);
    result<bool> export_metrics(const std::vector<metric_data>& metrics);
    result<bool> export_logs(const std::vector<log_entry>& logs);
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

### Basic Monitoring Setup
```cpp
// Create monitoring instance
monitoring_builder builder;
auto monitoring = builder
    .with_history_size(1000)
    .with_collection_interval(1s)
    .add_collector(std::make_unique<performance_monitor>())
    .with_storage(std::make_unique<sqlite_storage_backend>(config))
    .enable_compression(true)
    .build();

// Start monitoring
monitoring.value()->start();
```

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
    []() { return result<std::string>::success("fallback_value"); }
);
```

---

## Error Codes

Common error codes used throughout the system:

```cpp
enum class monitoring_error_code {
    success = 0,
    unknown_error,
    invalid_argument,
    out_of_range,
    not_found,
    already_exists,
    permission_denied,
    resource_exhausted,
    operation_cancelled,
    operation_timeout,
    not_implemented,
    internal_error,
    unavailable,
    data_loss,
    unauthenticated,
    circuit_breaker_open,
    retry_exhausted,
    validation_failed,
    initialization_failed,
    configuration_error
};
```

---

## Thread Safety

All public interfaces in the monitoring system are thread-safe unless explicitly documented otherwise. Internal synchronization uses:
- `std::mutex` for exclusive access
- `std::shared_mutex` for read-write locks
- `std::atomic` for lock-free operations
- Thread-local storage for context management

---

## Performance Considerations

1. **Metric Collection**: Designed for minimal overhead (<5% CPU)
2. **Storage**: Async writes with batching for efficiency
3. **Tracing**: Sampling support to reduce overhead
4. **Health Checks**: Cached results with configurable TTL
5. **Circuit Breakers**: Lock-free state transitions
6. **Stream Processing**: Zero-copy where possible

---

## Best Practices

1. **Error Handling**: Always check `result<T>` return values
2. **Resource Management**: Use RAII patterns (scoped_timer, scoped_span)
3. **Configuration**: Validate configs before use
4. **Monitoring**: Start with conservative thresholds, tune based on metrics
5. **Tracing**: Use sampling in production to control overhead
6. **Health Checks**: Keep checks lightweight and fast
7. **Storage**: Choose backend based on durability vs performance needs

---

## Migration Guide

### From Direct Metrics to Collectors
```cpp
// Old way
metrics.record("latency", 150.0);

// New way
auto timer = perf_monitor.time_operation("process_request");
// ... operation ...
timer.complete();
```

### From Callbacks to Health Checks
```cpp
// Old way
register_health_callback([]() { return check_health(); });

// New way
monitor.register_check("service",
    std::make_shared<functional_health_check>(
        "service_check",
        health_check_type::liveness,
        []() { return check_health(); }
    )
);
```

---

## Version Compatibility

- C++17 or later required
- Thread support required
- Optional: C++20 for enhanced features (concepts, coroutines)
- Compatible with: GCC 7+, Clang 5+, MSVC 2017+

---

## Further Reading

- [Architecture Guide](ARCHITECTURE_GUIDE.md)
- [Examples](../examples/)
- [Performance Tuning Guide](PERFORMANCE_TUNING.md)
- [Troubleshooting Guide](TROUBLESHOOTING.md)