---
doc_id: "MON-FEAT-CORE-001"
doc_title: "Monitoring System - Core Features"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "monitoring_system"
category: "FEAT"
---

# Monitoring System - Core Features

> **SSOT**: This document is the single source of truth for **Monitoring System - Core Features** (performance monitoring and metric types).

**Version**: 0.4.0.0
**Last Updated**: 2026-02-08

## Overview

This document describes the core monitoring capabilities of the monitoring system, including performance metrics collection, metric types, health monitoring, error handling, dependency injection, storage backends, reliability patterns, the plugin system, adaptive monitoring, and SIMD-accelerated aggregation.

For collector implementations, see [FEATURES_COLLECTORS.md](FEATURES_COLLECTORS.md).
For alert pipeline, distributed tracing, and exporters, see [FEATURES_ALERTS_TRACING.md](FEATURES_ALERTS_TRACING.md).

---

## Table of Contents

- [Core Capabilities](#core-capabilities)
- [Performance Monitoring](#performance-monitoring)
- [Health Monitoring](#health-monitoring)
- [Error Handling](#error-handling)
- [Dependency Injection](#dependency-injection)
- [Storage Backends](#storage-backends)
- [Reliability Patterns](#reliability-patterns)
- [Advanced Features](#advanced-features)
- [Load Average History Tracking](#load-average-history-tracking)
- [Plugin System](#plugin-system)
- [Adaptive Monitoring](#adaptive-monitoring)
- [SIMD Optimization](#simd-optimization)

---

## Core Capabilities

### Real-Time Monitoring

The monitoring system provides comprehensive real-time observability for high-performance applications.

**Key Features**:
- **Performance Metrics**: Atomic counters, gauges, histograms with 10M+ ops/sec throughput
- **Distributed Tracing**: Request flow tracking with span creation (2.5M spans/sec)
- **Health Monitoring**: Service health checks and dependency validation (500K checks/sec)
- **Thread-Safe Operations**: Lock-free atomic operations for minimal overhead
- **Configurable Storage**: Memory and file backends with time-series compression

**Use Cases**:
- Microservices observability
- High-frequency trading systems
- Real-time application monitoring
- Performance bottleneck identification
- System health validation

---

## Performance Monitoring

### Metrics Collection

The performance monitor provides real-time metrics collection with minimal overhead.

**Supported Metric Types**:

| Metric Type | Description | Throughput | Use Case |
|-------------|-------------|------------|----------|
| **Counters** | Monotonically increasing values | 10M ops/sec | Request counts, event totals |
| **Gauges** | Point-in-time values | 8M ops/sec | Memory usage, connection counts |
| **Histograms** | Distribution of values | 5M ops/sec | Request latencies, response sizes |
| **Timers** | Duration measurements | 6M ops/sec | Operation timing, SLA tracking |

**Example Usage**:
```cpp
#include <kcenon/monitoring/core/performance_monitor.h>

using namespace monitoring_system;

performance_monitor monitor("my_service");

// Counter: Track request count
monitor.increment_counter("requests_total");
monitor.increment_counter("requests_by_endpoint:/api/users");

// Gauge: Track current connections
monitor.set_gauge("active_connections", 42);

// Histogram: Track response sizes
monitor.record_histogram("response_size_bytes", 1024);

// Timer: Automatic duration tracking
{
    auto timer = monitor.start_timer("request_processing");
    // ... processing logic ...
    // Timer automatically records duration when destroyed
}

// Collect metrics snapshot
auto snapshot = monitor.collect();
if (snapshot) {
    std::cout << "CPU Usage: " << snapshot.value().get_metric("cpu_usage") << "%\n";
    std::cout << "Memory Usage: " << snapshot.value().get_metric("memory_usage") << " MB\n";
}
```

### Profiling Capabilities

The profiler provides detailed timing and resource usage analysis.

**Features**:
- High-resolution timing (nanosecond precision)
- Statistical analysis (mean, median, P95, P99)
- Hot path identification
- Resource usage tracking

**Example**:
```cpp
auto& profiler = monitor.get_profiler();

// Record operation timing
auto start = std::chrono::steady_clock::now();
// ... operation ...
auto end = std::chrono::steady_clock::now();

auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
profiler.record_sample("operation_name", duration, true);

// Get statistics
auto stats = profiler.get_statistics("operation_name");
std::cout << "Mean: " << stats.mean_duration.count() << " ns\n";
std::cout << "P95: " << stats.p95_duration.count() << " ns\n";
std::cout << "P99: " << stats.p99_duration.count() << " ns\n";
```

### Adaptive Sampling

Intelligent sampling strategies for high-throughput scenarios.

**Sampling Strategies**:
- **Always**: Record every event (development/debugging)
- **Probabilistic**: Sample at configured rate (production)
- **Rate-based**: Limit samples per second
- **Adaptive**: Dynamically adjust based on load

**Configuration**:
```cpp
monitoring_config config;
config.sampling_rate = 0.1;  // 10% sampling
config.max_samples_per_second = 1000;
config.adaptive_sampling = true;

auto monitor = create_monitor(config);
```

---

## Health Monitoring

### Health Check System

Comprehensive health validation framework for services and dependencies.

**Health Check Types**:

| Type | Purpose | Check Frequency | Example |
|------|---------|-----------------|---------|
| **System** | System resource validation | High (1-5s) | Memory, CPU, disk usage |
| **Dependency** | External service health | Medium (10-30s) | Database, cache, APIs |
| **Custom** | Application-specific checks | Configurable | Business logic validation |

**Example**:
```cpp
#include <monitoring/health/health_monitor.h>

health_monitor health_monitor;

// Register system health check
health_monitor.register_check(
    std::make_unique<functional_health_check>(
        "system_resources",
        health_check_type::system,
        []() {
            auto memory_usage = get_memory_usage_percent();
            if (memory_usage < 80.0) {
                return health_check_result::healthy("Memory usage normal");
            } else if (memory_usage < 90.0) {
                return health_check_result::degraded("High memory usage");
            } else {
                return health_check_result::unhealthy("Critical memory usage");
            }
        }
    )
);

// Register dependency health check
health_monitor.register_check(
    std::make_unique<functional_health_check>(
        "database_connection",
        health_check_type::dependency,
        []() {
            bool connected = check_database_connection();
            return connected ?
                health_check_result::healthy("Database connected") :
                health_check_result::unhealthy("Database unreachable");
        }
    )
);

// Check overall health
auto health_result = health_monitor.check_health();

// Get specific check status
auto db_status = health_monitor.get_check_status("database_connection");
```

### Health Status Levels

**Status Hierarchy**:
- **Healthy**: All checks passing, system operating normally
- **Degraded**: Some non-critical issues detected, system operational
- **Unhealthy**: Critical issues detected, system may be failing

**Response Mapping**:
```cpp
health_status status = health_result.status;

switch (status) {
    case health_status::healthy:
        // HTTP 200 OK
        break;
    case health_status::degraded:
        // HTTP 200 OK (with warning)
        break;
    case health_status::unhealthy:
        // HTTP 503 Service Unavailable
        break;
}
```

---

## Error Handling

### Result Pattern

Comprehensive error handling using the `Result<T>` pattern for type-safe operations.

**Result Type Features**:
- Type-safe error handling
- No exceptions required
- Composable operations
- Rich error context

**Example**:
```cpp
#include <kcenon/monitoring/core/result_types.h>

using namespace monitoring_system;

// Function returning Result<T>
result<std::string> fetch_user_data(int user_id) {
    if (user_id <= 0) {
        return make_error<std::string>(
            monitoring_error_code::invalid_argument,
            "Invalid user ID"
        );
    }

    // ... fetch logic ...

    return make_success(std::string("user_data"));
}

// Error handling with explicit checking
auto result = fetch_user_data(123);
if (result) {
    std::cout << "User data: " << result.value() << "\n";
} else {
    std::cerr << "Error: " << result.error().message << "\n";
    std::cerr << "Code: " << static_cast<int>(result.error().code) << "\n";
}

// Chaining operations
auto processed = result
    .map([](const std::string& data) {
        return data + "_processed";
    })
    .and_then([](const std::string& data) {
        return make_success(data.length());
    });

if (processed) {
    std::cout << "Length: " << processed.value() << "\n";
}
```

### Error Code Organization

Monitoring system uses error codes in the range **-300 to -399**.

**Error Categories**:

| Range | Category | Examples |
|-------|----------|----------|
| -300 to -309 | Configuration | Invalid config, missing parameters |
| -310 to -319 | Metrics collection | Collection failure, invalid metric |
| -320 to -329 | Tracing | Span creation failure, invalid context |
| -330 to -339 | Health monitoring | Check failure, invalid health state |
| -340 to -349 | Storage | Write failure, storage full |
| -350 to -359 | Analysis | Analysis failure, insufficient data |

**Example Error Codes**:
```cpp
enum class monitoring_error_code {
    // Configuration (-300 to -309)
    invalid_configuration = -300,
    missing_required_parameter = -301,

    // Metrics collection (-310 to -319)
    collection_failed = -310,
    invalid_metric_type = -311,

    // Tracing (-320 to -329)
    span_creation_failed = -320,
    invalid_trace_context = -321,

    // Health monitoring (-330 to -339)
    health_check_failed = -330,
    dependency_unavailable = -331,

    // Storage (-340 to -349)
    storage_write_failed = -340,
    storage_full = -341,

    // Analysis (-350 to -359)
    analysis_failed = -350,
    insufficient_data = -351
};
```

---

## Dependency Injection

### DI Container

Complete dependency injection container with service registration and lifecycle management.

**Features**:
- Singleton registration
- Transient registration
- Factory registration
- Automatic dependency resolution
- Lifecycle management

**Example**:
```cpp
#include <kcenon/monitoring/core/di_container.h>

using namespace monitoring_system;

di_container container;

// Register singleton
container.register_singleton<ILogger, ConsoleLogger>();
container.register_singleton<IMonitor, PerformanceMonitor>();

// Register transient (new instance each time)
container.register_transient<IDatabase, PostgresDatabase>();

// Register with factory
container.register_factory<ICache>([&]() {
    auto config = container.resolve<IConfig>().value();
    return std::make_shared<RedisCache>(config);
});

// Resolve dependencies
auto logger = container.resolve<ILogger>();
if (logger) {
    logger.value()->info("DI container initialized");
}

auto monitor = container.resolve<IMonitor>();
if (monitor) {
    monitor.value()->enable_collection(true);
}
```

---

## Storage Backends

### Memory Storage

In-memory storage backend for high-performance scenarios.

**Features**:
- Zero I/O overhead
- Configurable retention
- Automatic cleanup
- Thread-safe access

**Example**:
```cpp
#include <kcenon/monitoring/storage/memory_storage.h>

auto storage = std::make_unique<memory_storage>(memory_storage_config{
    .max_entries = 10000,
    .retention_period = std::chrono::hours(1),
    .cleanup_interval = std::chrono::minutes(10)
});

performance_monitor monitor("service", std::move(storage));
```

### File Storage

File-based persistent storage for long-term retention.

**Features**:
- Durable storage
- Configurable rotation
- Compression support
- Efficient querying

**Example**:
```cpp
#include <kcenon/monitoring/storage/file_storage.h>

auto storage = std::make_unique<file_storage>(file_storage_config{
    .base_path = "/var/log/metrics",
    .rotation = rotation_policy::daily,
    .compression = true,
    .max_file_size = 100_MB
});
```

### Time-Series Storage

Optimized storage for time-series metric data.

**Features**:
- Efficient compression (up to 90%)
- Fast aggregation queries
- Downsampling support
- Retention policies

**Example**:
```cpp
#include <kcenon/monitoring/storage/time_series_storage.h>

auto storage = std::make_unique<time_series_storage>(time_series_config{
    .database_path = "metrics.tsdb",
    .compression_ratio = 10,
    .retention_days = 30,
    .downsampling = {
        {std::chrono::hours(1), std::chrono::days(7)},
        {std::chrono::hours(24), std::chrono::days(30)}
    }
});
```

---

## Reliability Patterns

### Circuit Breaker

Automatic failure detection and recovery mechanism.

**States**:
- **Closed**: Normal operation, requests pass through
- **Open**: Failure threshold exceeded, requests fail fast
- **Half-Open**: Testing recovery, limited requests allowed

**Example**:
```cpp
#include <kcenon/monitoring/health/circuit_breaker.h>

circuit_breaker db_breaker("database_connection", circuit_breaker_config{
    .failure_threshold = 5,
    .timeout = std::chrono::seconds(30),
    .half_open_max_calls = 3
});

// Protected operation
auto result = db_breaker.execute([&]() -> result<std::string> {
    return fetch_from_database();
});

if (!result) {
    std::cerr << "Operation failed: " << result.error().message << "\n";

    // Check circuit breaker state
    auto state = db_breaker.get_state();
    if (state == circuit_breaker_state::open) {
        // Circuit is open, fail fast
        return use_fallback_data();
    }
}
```

### Retry Policies

Configurable retry mechanisms for transient failures.

**Retry Strategies**:
- **Fixed delay**: Constant interval between retries
- **Exponential backoff**: Increasing delay between retries
- **Jittered backoff**: Exponential with random jitter

**Example**:
```cpp
#include <kcenon/monitoring/health/reliability_patterns.h>

retry_policy policy{
    .max_attempts = 3,
    .strategy = retry_strategy::exponential_backoff,
    .initial_delay = std::chrono::milliseconds(100),
    .max_delay = std::chrono::seconds(10),
    .jitter = true
};

auto result = execute_with_retry(policy, [&]() {
    return call_external_service();
});
```

### Error Boundaries

Isolate failures and prevent cascading errors.

**Example**:
```cpp
error_boundary boundary("user_service");

auto result = boundary.execute([&]() {
    // Protected operation
    return process_user_request();
});

if (!result) {
    // Error isolated
    boundary.record_failure();
    return use_cached_response();
}
```

---

## Advanced Features

### Thread Context Tracking

Track request context and metadata across threads.

**Features**:
- Thread-local storage
- Automatic context propagation
- Metadata attachment
- Correlation ID tracking

**Example**:
```cpp
#include <kcenon/monitoring/core/thread_context.h>

// Set context
thread_context ctx;
ctx.correlation_id = "req-12345";
ctx.metadata["user_id"] = "67890";
ctx.metadata["session_id"] = "session-abc";

set_thread_context(ctx);

// Context available in current thread
auto current = get_thread_context();
std::cout << "Correlation ID: " << current.correlation_id << "\n";
```

### Event-Driven Architecture

Asynchronous event processing with minimal blocking.

**Example**:
```cpp
event_bus bus;

// Subscribe to events
bus.subscribe("metric_collected", [](const event& e) {
    std::cout << "Metric: " << e.data["metric_name"] << "\n";
});

// Publish events
event e;
e.type = "metric_collected";
e.data["metric_name"] = "requests_total";
e.data["value"] = "12345";

bus.publish(e);
```

### Custom Metrics

Create domain-specific monitoring capabilities.

**Example**:
```cpp
// Custom metric type
class business_metric : public metric_interface {
public:
    void record_sale(double amount) {
        monitor_.increment_counter("sales_count");
        monitor_.record_histogram("sale_amount", amount);
    }

    void record_conversion(bool success) {
        monitor_.increment_counter("conversions_total");
        if (success) {
            monitor_.increment_counter("conversions_successful");
        }
    }

private:
    performance_monitor& monitor_;
};
```

---

## Load Average History Tracking

### Overview

The load average history tracking feature provides time-series storage of system load averages for trend analysis, anomaly detection, and capacity planning. It extends `system_resource_collector` with in-memory ring buffer storage that automatically tracks load_1m, load_5m, and load_15m values with timestamps.

**Features**:
- Thread-safe ring buffer with configurable size (default: 1000 samples)
- Timestamp stored with each sample
- Statistics calculation (min, max, avg, stddev, p95, p99)
- Duration-based and time-range queries
- Memory-bounded with no unbounded growth

**Metrics Available in Statistics**:

| Statistic | Description | Notes |
|-----------|-------------|-------|
| **min_value** | Minimum load value in the period | Per load average type (1m, 5m, 15m) |
| **max_value** | Maximum load value in the period | Per load average type |
| **avg** | Average load value | Per load average type |
| **stddev** | Standard deviation | Measures volatility |
| **p95** | 95th percentile | For capacity planning |
| **p99** | 99th percentile | For outlier detection |
| **sample_count** | Number of samples | In the query period |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/system_resource_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
system_resource_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"load_history_max_samples", "1000"},
    {"enable_load_history", "true"}
};
collector.initialize(config);

// Collect metrics (automatically stores load averages)
collector.collect();
collector.collect();
collector.collect();

// Get load history for the last hour
auto samples = collector.get_load_history(std::chrono::hours(1));
for (const auto& sample : samples) {
    std::cout << "Load 1m: " << sample.load_1m
              << ", 5m: " << sample.load_5m
              << ", 15m: " << sample.load_15m << std::endl;
}

// Get statistics for the last 30 minutes
auto stats = collector.get_load_statistics(std::chrono::minutes(30));
std::cout << "Load 1m avg: " << stats.load_1m_stats.avg << std::endl;
std::cout << "Load 1m p95: " << stats.load_1m_stats.p95 << std::endl;
std::cout << "Load 1m stddev: " << stats.load_1m_stats.stddev << std::endl;
```

### Using time_series_buffer Directly

For custom metric history tracking, you can use the generic `time_series_buffer<T>` template:

```cpp
#include <kcenon/monitoring/utils/time_series_buffer.h>

using namespace kcenon::monitoring;

// Configure buffer
time_series_buffer_config config;
config.max_samples = 500;

// Create buffer for custom metric
time_series_buffer<double> cpu_history(config);

// Add samples
cpu_history.add_sample(45.2);
cpu_history.add_sample(52.1);
cpu_history.add_sample(48.7);

// Get statistics
auto stats = cpu_history.get_statistics();
std::cout << "CPU avg: " << stats.avg << std::endl;
std::cout << "CPU min: " << stats.min_value << std::endl;
std::cout << "CPU max: " << stats.max_value << std::endl;

// Get samples from the last 5 minutes
auto recent = cpu_history.get_samples(std::chrono::minutes(5));
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `load_history_max_samples` | 1000 | Maximum samples to store in ring buffer |
| `enable_load_history` | true | Enable/disable load history tracking |

### Thread Safety

Both `time_series_buffer<T>` and `load_average_history` are fully thread-safe:
- Mutex-protected read/write operations
- Safe for concurrent collection and query
- Statistics calculation is performed on snapshot data

### Memory Footprint

The memory footprint is bounded and predictable:
- `time_series_buffer<T>`: ~48 bytes per sample (timestamp + value + overhead)
- `load_average_history`: ~72 bytes per sample (timestamp + 3 doubles + overhead)
- Default 1000 samples: ~72KB for load average history

---

## Plugin System

### Overview

The plugin system provides a dynamic, extensible architecture for loading metric collectors at runtime. Plugins are compiled as shared libraries (`.so`/`.dylib`/`.dll`) and loaded via a cross-platform loader with API version compatibility checking. This enables optional features (container monitoring, hardware sensors) to be loaded only when needed, reducing binary size and collection overhead for deployments that do not require them.

**Architecture Components**:

| Component | Header | Purpose |
|-----------|--------|---------|
| **collector_plugin** | `plugins/collector_plugin.h` | Pure virtual interface for all collector plugins |
| **plugin_api** | `plugins/plugin_api.h` | C ABI interface for cross-compiler compatibility |
| **plugin_loader** | `plugins/plugin_loader.h` | Dynamic shared library loading and symbol resolution |
| **collector_registry** | `plugins/collector_registry.h` | Singleton registry for plugin lifecycle management |

### Plugin Categories

Plugins are organized by category via the `plugin_category` enum:

| Category | Description | Example Plugins |
|----------|-------------|-----------------|
| `system` | System integration (threads, loggers, containers) | Container plugin |
| `hardware` | Hardware sensors (GPU, temperature, battery, power) | Hardware plugin |
| `platform` | Platform-specific (VM, uptime, interrupts) | - |
| `network` | Network metrics (connectivity, bandwidth) | - |
| `process` | Process-level metrics (resources, performance) | - |
| `custom` | User-defined plugins | Any custom implementation |

### Available Plugin Types

#### Container Plugin

The container plugin (`plugins/container/container_plugin.h`) provides monitoring for Docker, Kubernetes, and cgroup-based container runtimes.

**Supported Runtimes**: Docker, containerd, Podman, CRI-O (with auto-detection)

**Metrics**: Container CPU/memory/network/I/O, running container count, Kubernetes pod/deployment metrics, cgroup CPU time and memory usage/limits.

```cpp
#include <kcenon/monitoring/plugins/container/container_plugin.h>

using namespace kcenon::monitoring::plugins;

// Create with custom configuration
container_plugin_config config;
config.enable_docker = true;
config.enable_kubernetes = false;
config.docker_socket = "/var/run/docker.sock";
config.collect_network_metrics = true;

auto plugin = container_plugin::create(config);

// Check container environment before loading
if (container_plugin::is_running_in_container()) {
    registry.register_plugin(std::move(plugin));
}

// Detect runtime automatically
auto runtime = container_plugin::detect_runtime();
```

#### Hardware Plugin

The hardware plugin (`plugins/hardware/hardware_plugin.h`) provides battery, power consumption, temperature, and GPU monitoring for desktop/laptop environments.

**Metrics**: Battery level/health/cycles, power consumption (watts/RAPL), CPU/GPU/motherboard temperatures, GPU utilization/VRAM/clocks/fan speed.

```cpp
#include <kcenon/monitoring/plugins/hardware/hardware_plugin.h>

using namespace kcenon::monitoring::plugins;

// Create with custom configuration
hardware_plugin_config config;
config.enable_battery = true;
config.enable_temperature = true;
config.enable_gpu = true;
config.gpu_collect_utilization = true;
config.gpu_collect_memory = true;

auto plugin = hardware_plugin::create(config);

// Check individual sensor availability
if (plugin->is_battery_available()) {
    // Battery metrics will be collected
}
if (plugin->is_gpu_available()) {
    // GPU metrics will be collected
}
```

### Plugin Loading and Registration Flow

The plugin lifecycle follows this sequence:

1. **Load**: `dynamic_plugin_loader` opens the shared library and resolves `create_plugin`, `destroy_plugin`, and `get_plugin_info` symbols
2. **Verify**: API version compatibility is checked via `plugin_api_metadata.api_version` against `PLUGIN_API_VERSION`
3. **Create**: The plugin instance is created via the resolved `create_plugin` function
4. **Register**: The plugin is added to `collector_registry` which manages ownership
5. **Initialize**: `initialize()` is called with configuration parameters
6. **Collect**: `collect()` is called periodically based on `interval()`
7. **Shutdown**: `shutdown()` is called before plugin destruction and library unloading

```cpp
// Load and register a plugin from a shared library
auto& registry = collector_registry::instance();
bool loaded = registry.load_plugin("/path/to/libmy_plugin.so");

if (!loaded) {
    std::cerr << "Error: " << registry.get_plugin_loader_error() << std::endl;
}

// Access a registered plugin
if (auto* plugin = registry.get_plugin("my_collector")) {
    auto metrics = plugin->collect();
}

// List plugins by category
auto hw_plugins = registry.get_plugins_by_category(plugin_category::hardware);

// Factory-based lazy registration
registry.register_factory<my_collector_plugin>("my_collector");

// Initialize all plugins at once
registry.initialize_all(config);

// Shutdown all plugins
registry.shutdown_all();
```

### Implementing a Custom Plugin

Plugins must export three C-linkage functions. The `IMPLEMENT_PLUGIN` macro simplifies this:

```cpp
#include <kcenon/monitoring/plugins/plugin_api.h>
#include <kcenon/monitoring/plugins/collector_plugin.h>

class my_sensor_plugin : public kcenon::monitoring::collector_plugin {
public:
    auto name() const -> std::string_view override { return "my_sensor"; }

    auto collect() -> std::vector<metric> override {
        // Collect metrics from custom hardware/source
        return { /* ... */ };
    }

    auto interval() const -> std::chrono::milliseconds override {
        return std::chrono::seconds(5);
    }

    auto is_available() const -> bool override {
        return true; // Check hardware availability
    }

    auto get_metric_types() const -> std::vector<std::string> override {
        return {"gauge"};
    }
};

// Export required C ABI functions
IMPLEMENT_PLUGIN(
    my_sensor_plugin,    // Plugin class
    "my_sensor",         // Plugin name
    "1.0.0",             // Version
    "My Sensor Plugin",  // Description
    "Author Name",       // Author
    "hardware"           // Category
)
```

> [!TIP]
> For detailed plugin development instructions, see [Plugin Development Guide](plugin_development_guide.md). For a complete API reference, see [Plugin API Reference](plugin_api_reference.md). For architectural details, see [Plugin Architecture](plugin_architecture.md).

---

## Adaptive Monitoring

### Overview

The adaptive monitoring system automatically adjusts collection intervals, sampling rates, and metric granularity based on current system resource utilization. This ensures that monitoring overhead remains minimal during high-load situations while providing detailed data during idle periods.

**Key Features**:
- Automatic load level detection (idle, low, moderate, high, critical)
- Per-level configurable collection intervals and sampling rates
- Three adaptation strategies: conservative, balanced, aggressive
- Hysteresis and cooldown mechanisms to prevent oscillation
- Exponential smoothing for stable load estimation
- Thread-safe operation with RAII scope management

### Adaptation Strategies

| Strategy | Behavior | Use Case |
|----------|----------|----------|
| `conservative` | Reduces effective load by 20% to maintain system stability | Production servers with strict SLAs |
| `balanced` | No adjustment; uses raw load metrics directly | General-purpose monitoring |
| `aggressive` | Increases effective load by 20% to preserve monitoring detail | Development and debugging environments |

### Load Levels and Defaults

| Load Level | CPU Threshold | Default Interval | Default Sampling Rate |
|------------|--------------|------------------|-----------------------|
| `idle` | < 20% | 100ms | 100% |
| `low` | 20-40% | 250ms | 80% |
| `moderate` | 40-60% | 500ms | 50% |
| `high` | 60-80% | 1000ms | 20% |
| `critical` | > 80% | 5000ms | 10% |

### Basic Usage

```cpp
#include <kcenon/monitoring/adaptive/adaptive_monitor.h>

using namespace kcenon::monitoring;

// Configure adaptive behavior
adaptive_config config;
config.strategy = adaptation_strategy::balanced;
config.idle_interval = std::chrono::milliseconds(100);
config.critical_interval = std::chrono::milliseconds(5000);
config.smoothing_factor = 0.7;
config.enable_hysteresis = true;
config.hysteresis_margin = 5.0;

// Register a collector with the global adaptive monitor
auto& monitor = global_adaptive_monitor();
monitor.register_collector("my_service", my_collector, config);
monitor.start();

// Check adaptation statistics
auto stats = monitor.get_collector_stats("my_service");
if (stats.is_ok()) {
    auto s = stats.value();
    std::cout << "Current load: " << static_cast<int>(s.current_load_level) << std::endl;
    std::cout << "Sampling rate: " << s.current_sampling_rate << std::endl;
    std::cout << "Total adaptations: " << s.total_adaptations << std::endl;
}

// Stop when done
monitor.stop();
```

### RAII Scope Management

The `adaptive_scope` class provides automatic registration and cleanup:

```cpp
#include <kcenon/monitoring/adaptive/adaptive_monitor.h>

using namespace kcenon::monitoring;

{
    // Automatically registers collector with the global adaptive monitor
    adaptive_scope scope("my_service", my_collector, config);

    if (scope.is_registered()) {
        // Collector is active and will be adaptively monitored
    }
    // Collector is automatically unregistered when scope exits
}
```

### Hysteresis and Cooldown

To prevent rapid oscillation at threshold boundaries:

- **Hysteresis**: The load must exceed the threshold by `hysteresis_margin` (default: 5%) to trigger a level change
- **Cooldown**: A minimum `cooldown_period` (default: 1000ms) must elapse between level changes

```cpp
adaptive_config config;
config.enable_hysteresis = true;
config.hysteresis_margin = 5.0;      // 5% margin above/below thresholds
config.enable_cooldown = true;
config.cooldown_period = std::chrono::milliseconds(1000);
```

---

## SIMD Optimization

### Overview

The SIMD aggregator provides high-performance statistical operations on metric data using SIMD (Single Instruction Multiple Data) instructions. It automatically detects available instruction sets at runtime and falls back to scalar operations when SIMD is not available or the dataset is too small to benefit.

**Supported Instruction Sets**:

| Instruction Set | Platform | Vector Width (doubles) |
|-----------------|----------|------------------------|
| AVX2 | x86_64 | 4 |
| SSE2 | x86_64 | 2 |
| NEON | ARM64 (aarch64) | 2 |
| Scalar fallback | All platforms | 1 |

### Statistical Operations

| Operation | Method | Description |
|-----------|--------|-------------|
| Sum | `sum(data)` | Sum of all elements |
| Mean | `mean(data)` | Arithmetic mean |
| Min | `min(data)` | Minimum value |
| Max | `max(data)` | Maximum value |
| Variance | `variance(data)` | Sample variance |
| Summary | `compute_summary(data)` | Full statistical summary (count, sum, mean, variance, std_dev, min, max) |

### Basic Usage

```cpp
#include <kcenon/monitoring/optimization/simd_aggregator.h>

using namespace kcenon::monitoring;

// Create with default configuration (SIMD enabled, auto-detect)
auto aggregator = make_simd_aggregator();

// Or with custom configuration
simd_config config;
config.enable_simd = true;
config.vector_size = 8;
config.alignment = 32;
config.use_fma = true;

auto aggregator = make_simd_aggregator(config);

// Perform operations
std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};

auto sum_result = aggregator->sum(data);
if (sum_result.is_ok()) {
    std::cout << "Sum: " << sum_result.value() << std::endl;  // 36.0
}

auto mean_result = aggregator->mean(data);
if (mean_result.is_ok()) {
    std::cout << "Mean: " << mean_result.value() << std::endl;  // 4.5
}

// Full statistical summary
auto summary = aggregator->compute_summary(data);
if (summary.is_ok()) {
    auto s = summary.value();
    std::cout << "Count: " << s.count << std::endl;
    std::cout << "Sum: " << s.sum << std::endl;
    std::cout << "Mean: " << s.mean << std::endl;
    std::cout << "Std Dev: " << s.std_dev << std::endl;
    std::cout << "Min: " << s.min_val << std::endl;
    std::cout << "Max: " << s.max_val << std::endl;
}
```

### Runtime Capability Detection

```cpp
auto aggregator = make_simd_aggregator();

// Query available SIMD instruction sets
const auto& caps = aggregator->get_capabilities();
std::cout << "SSE2: " << caps.sse2_available << std::endl;
std::cout << "AVX2: " << caps.avx2_available << std::endl;
std::cout << "NEON: " << caps.neon_available << std::endl;

// Self-test to verify SIMD correctness
auto test_result = aggregator->test_simd();
if (test_result.is_ok() && test_result.value()) {
    std::cout << "SIMD self-test passed" << std::endl;
}
```

### Performance Statistics

The aggregator tracks how often SIMD vs. scalar paths are used:

```cpp
auto aggregator = make_simd_aggregator();

// Perform several operations...
aggregator->sum(data);
aggregator->mean(data);
aggregator->min(data);

// Check utilization
const auto& stats = aggregator->get_statistics();
std::cout << "Total operations: " << stats.total_operations << std::endl;
std::cout << "SIMD operations: " << stats.simd_operations << std::endl;
std::cout << "Scalar operations: " << stats.scalar_operations << std::endl;
std::cout << "SIMD utilization: " << stats.get_simd_utilization() << "%" << std::endl;
std::cout << "Elements processed: " << stats.total_elements_processed << std::endl;

// Reset statistics
aggregator->reset_statistics();
```

### Performance Characteristics

- SIMD paths are automatically selected for datasets larger than `2 * vector_size` elements
- Smaller datasets use scalar paths to avoid SIMD setup overhead
- AVX2 processes 4 doubles per cycle; SSE2 and NEON process 2 doubles per cycle
- Automatic handling of remainder elements that do not fill a full SIMD vector
- All operations return `Result<T>` with proper error handling for empty inputs

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enable_simd` | true | Enable/disable SIMD acceleration |
| `vector_size` | 8 | SIMD vector width for processing |
| `alignment` | 32 | Memory alignment for SIMD operations (bytes) |
| `use_fma` | true | Use fused multiply-add if available |

> [!NOTE]
> The SIMD aggregator automatically falls back to scalar operations on platforms without supported SIMD instruction sets, or when SIMD is explicitly disabled via configuration. No code changes are required for cross-platform compatibility.

---

## See Also

- [FEATURES.md](FEATURES.md) - Features index page
- [FEATURES_COLLECTORS.md](FEATURES_COLLECTORS.md) - Collector implementations
- [FEATURES_ALERTS_TRACING.md](FEATURES_ALERTS_TRACING.md) - Alert pipeline, distributed tracing, exporters
- [API Reference](API_REFERENCE.md) - Complete API documentation
- [Architecture Guide](ARCHITECTURE.md) - System design and patterns
- [Quick Start Guide](guides/QUICK_START.md) - Usage examples and best practices
- [Benchmarks](BENCHMARKS.md) - Performance metrics and comparisons
