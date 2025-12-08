# Monitoring System - Feature Documentation

**Version**: 1.0
**Last Updated**: 2025-11-15

## Overview

This document provides comprehensive details about all features available in the monitoring system. For a quick overview, see the main [README](../README.md).

---

## Table of Contents

- [Core Capabilities](#core-capabilities)
- [Performance Monitoring](#performance-monitoring)
- [Container Metrics Monitoring](#container-metrics-monitoring)
- [SMART Disk Health Monitoring](#smart-disk-health-monitoring)
- [Hardware Temperature Monitoring](#hardware-temperature-monitoring)
- [File Descriptor Usage Monitoring](#file-descriptor-usage-monitoring)
- [Distributed Tracing](#distributed-tracing)
- [Health Monitoring](#health-monitoring)
- [Error Handling](#error-handling)
- [Dependency Injection](#dependency-injection)
- [Storage Backends](#storage-backends)
- [Reliability Patterns](#reliability-patterns)
- [Advanced Features](#advanced-features)

---

## Core Capabilities

### 🎯 Real-Time Monitoring

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

## Container Metrics Monitoring

### Overview

The container collector provides per-container resource usage metrics for containerized environments. It supports Linux cgroups v1/v2 with graceful degradation outside containers.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **CPU Usage** | Container CPU utilization | Percent |
| **Memory Usage** | Current memory consumption | Bytes |
| **Memory Limit** | Container memory limit | Bytes |
| **Network RX** | Bytes received | Bytes |
| **Network TX** | Bytes transmitted | Bytes |
| **Block I/O Read** | Bytes read from disk | Bytes |
| **Block I/O Write** | Bytes written to disk | Bytes |
| **Process Count** | Current number of processes | Count |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/container_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
container_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"collect_network", "true"},
    {"collect_blkio", "true"}
};
collector.initialize(config);

// Collect metrics from all containers
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": ";
    if (std::holds_alternative<double>(metric.value)) {
        std::cout << std::get<double>(metric.value);
    }
    std::cout << std::endl;
}

// Get raw container metrics
auto container_metrics = collector.get_last_metrics();
for (const auto& cm : container_metrics) {
    std::cout << "Container: " << cm.container_id << std::endl;
    std::cout << "  CPU: " << cm.cpu_usage_percent << "%" << std::endl;
    std::cout << "  Memory: " << cm.memory_usage_bytes << " bytes" << std::endl;
}
```

### Cgroup Detection

The collector automatically detects the cgroup version:

```cpp
container_info_collector info_collector;

// Detect cgroup version
auto version = info_collector.detect_cgroup_version();
switch (version) {
    case cgroup_version::v1:
        std::cout << "Using cgroups v1" << std::endl;
        break;
    case cgroup_version::v2:
        std::cout << "Using cgroups v2" << std::endl;
        break;
    case cgroup_version::none:
        std::cout << "No cgroups available" << std::endl;
        break;
}

// Check if running inside a container
if (info_collector.is_containerized()) {
    std::cout << "Running inside a container" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable metric collection |
| `collect_network` | true | Collect network metrics |
| `collect_blkio` | true | Collect block I/O metrics |

---

## SMART Disk Health Monitoring

### Overview

The SMART collector provides S.M.A.R.T. (Self-Monitoring, Analysis and Reporting Technology) disk health monitoring for predictive failure detection and proactive maintenance. It uses `smartctl` from smartmontools for cross-platform support.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **Health Status** | Overall drive health (PASSED/FAILED) | Boolean |
| **Temperature** | Current drive temperature | Celsius |
| **Reallocated Sectors** | Count of reallocated sectors | Count |
| **Power-On Hours** | Total hours of operation | Hours |
| **Power Cycle Count** | Number of power cycles | Count |
| **Pending Sectors** | Sectors pending reallocation | Count |
| **Uncorrectable Errors** | Uncorrectable error count | Count |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/smart_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
smart_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"collect_temperature", "true"},
    {"collect_error_rates", "true"}
};
collector.initialize(config);

// Collect SMART metrics from all disks
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": " << metric.value << std::endl;
}

// Get raw SMART metrics
auto smart_metrics = collector.get_last_metrics();
for (const auto& sm : smart_metrics) {
    std::cout << "Disk: " << sm.device_path << std::endl;
    std::cout << "  Model: " << sm.model_name << std::endl;
    std::cout << "  Health: " << (sm.health_ok ? "PASSED" : "FAILED") << std::endl;
    std::cout << "  Temperature: " << sm.temperature_celsius << "°C" << std::endl;
    std::cout << "  Power-On Hours: " << sm.power_on_hours << std::endl;
}
```

### SMART Availability Check

```cpp
smart_collector collector;
collector.initialize({});

// Check if SMART monitoring is available
if (collector.is_smart_available()) {
    std::cout << "smartctl is available" << std::endl;
    auto metrics = collector.collect();
} else {
    std::cout << "smartctl not found - install smartmontools" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable SMART collection |
| `collect_temperature` | true | Collect temperature metrics |
| `collect_error_rates` | true | Collect read/write error rates |

### Platform Requirements

| Platform | Requirement |
|----------|-------------|
| **Linux** | `smartmontools` package (`apt install smartmontools`) |
| **macOS** | `smartmontools` via Homebrew (`brew install smartmontools`) |
| **Windows** | smartmontools Windows installer |

> [!NOTE]
> The collector gracefully degrades when `smartctl` is not available or a disk doesn't support SMART. No errors are returned; the collector simply returns empty metrics.

---

## Hardware Temperature Monitoring

### Overview

The temperature collector provides hardware temperature monitoring for thermal sensor data from CPU, GPU, and other system components. It supports cross-platform implementations with graceful degradation when sensors are unavailable.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **Temperature** | Current sensor temperature | Celsius |
| **Critical Threshold** | Temperature threshold for critical state | Celsius |
| **Warning Threshold** | Temperature threshold for warning state | Celsius |
| **Is Critical** | Whether temperature exceeds critical threshold | Boolean |
| **Is Warning** | Whether temperature exceeds warning threshold | Boolean |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/temperature_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
temperature_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"collect_thresholds", "true"},
    {"collect_warnings", "true"}
};
collector.initialize(config);

// Collect temperature metrics from all sensors
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": ";
    if (std::holds_alternative<double>(metric.value)) {
        std::cout << std::get<double>(metric.value);
    }
    std::cout << std::endl;
}

// Get raw temperature readings
auto readings = collector.get_last_readings();
for (const auto& reading : readings) {
    std::cout << "Sensor: " << reading.sensor.name << std::endl;
    std::cout << "  Type: " << sensor_type_to_string(reading.sensor.type) << std::endl;
    std::cout << "  Temperature: " << reading.temperature_celsius << "°C" << std::endl;
    if (reading.thresholds_available) {
        std::cout << "  Warning: " << reading.warning_threshold_celsius << "°C" << std::endl;
        std::cout << "  Critical: " << reading.critical_threshold_celsius << "°C" << std::endl;
    }
}
```

### Thermal Availability Check

```cpp
temperature_collector collector;
collector.initialize({});

// Check if thermal monitoring is available
if (collector.is_thermal_available()) {
    std::cout << "Thermal sensors accessible" << std::endl;
    auto metrics = collector.collect();
} else {
    std::cout << "No thermal sensors found" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable temperature collection |
| `collect_thresholds` | true | Collect critical/warning thresholds |
| `collect_warnings` | true | Collect warning/critical status flags |

### Platform Implementation

| Platform | Method | Data Source |
|----------|--------|-------------|
| **Linux** | sysfs | `/sys/class/thermal/thermal_zone*/temp` |
| **macOS** | IOKit SMC | System Management Controller keys |
| **Windows** | WMI | `MSAcpi_ThermalZoneTemperature` class |

> [!NOTE]
> The collector gracefully degrades when thermal sensors are not available or when access is restricted. No errors are returned; the collector simply returns empty metrics.

---

## File Descriptor Usage Monitoring

### Overview

The file descriptor (FD) collector provides detailed FD usage monitoring at both system and process levels. FD exhaustion is a common failure mode in server applications ("too many open files"), and monitoring enables proactive leak detection, capacity planning, and alerting.

**Metrics Collected**:

| Metric | Description | Unit |
|--------|-------------|------|
| **fd_used_system** | Total system FDs in use (Linux only) | Count |
| **fd_max_system** | System FD limit (Linux only) | Count |
| **fd_used_process** | Current process FD count | Count |
| **fd_soft_limit** | Process FD soft limit | Count |
| **fd_hard_limit** | Process FD hard limit | Count |
| **fd_usage_percent** | Percentage of soft limit used | Percent |

### Basic Usage

```cpp
#include <kcenon/monitoring/collectors/fd_collector.h>

using namespace kcenon::monitoring;

// Create and initialize collector
fd_collector collector;
std::unordered_map<std::string, std::string> config = {
    {"enabled", "true"},
    {"warning_threshold", "80.0"},
    {"critical_threshold", "95.0"}
};
collector.initialize(config);

// Collect FD usage metrics
auto metrics = collector.collect();

for (const auto& metric : metrics) {
    std::cout << metric.name << ": ";
    if (std::holds_alternative<double>(metric.value)) {
        std::cout << std::get<double>(metric.value);
    }
    std::cout << std::endl;
}

// Get raw FD metrics
auto fd_data = collector.get_last_metrics();
std::cout << "Process FDs: " << fd_data.fd_used_process << std::endl;
std::cout << "Soft limit: " << fd_data.fd_soft_limit << std::endl;
std::cout << "Usage: " << fd_data.fd_usage_percent << "%" << std::endl;
```

### FD Monitoring Availability Check

```cpp
fd_collector collector;
collector.initialize({});

// Check if FD monitoring is available
if (collector.is_fd_monitoring_available()) {
    std::cout << "FD monitoring is available" << std::endl;
    auto metrics = collector.collect();
} else {
    std::cout << "FD monitoring not available on this platform" << std::endl;
}
```

### Configuration Options

| Option | Default | Description |
|--------|---------|-------------|
| `enabled` | true | Enable/disable FD collection |
| `warning_threshold` | 80.0 | Warning threshold (percentage of soft limit) |
| `critical_threshold` | 95.0 | Critical threshold (percentage of soft limit) |

### Platform Implementation

| Platform | Process FDs | Process Limits | System FDs |
|----------|-------------|----------------|------------|
| **Linux** | `/proc/self/fd/` enumeration | `/proc/self/limits` | `/proc/sys/fs/file-nr` |
| **macOS** | `/dev/fd/` enumeration | `getrlimit(RLIMIT_NOFILE)` | Not available |
| **Windows** | `GetProcessHandleCount()` | Default limits | Not available |

> [!NOTE]
> System-wide FD metrics are only available on Linux. On macOS and Windows, the collector gracefully degrades and returns only process-level metrics.

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

## See Also

- [API Reference](02-API_REFERENCE.md) - Complete API documentation
- [Architecture Guide](01-ARCHITECTURE.md) - System design and patterns
- [User Guide](guides/USER_GUIDE.md) - Usage examples and best practices
- [Benchmarks](BENCHMARKS.md) - Performance metrics and comparisons
