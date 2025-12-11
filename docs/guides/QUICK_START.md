# Monitoring System - Quick Start Guide

> **Version:** 0.1.0
> **Last Updated:** 2025-11-11
> **Time to Complete:** 5 minutes

Get up and running with monitoring_system in 5 minutes. This guide covers installation, basic usage, and first metrics collection.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Installation](#installation)
3. [Your First Monitoring Program](#your-first-monitoring-program)
4. [Basic Metrics](#basic-metrics)
5. [Alerting](#alerting)
6. [Distributed Tracing](#distributed-tracing)
7. [Web Dashboard](#web-dashboard)
8. [Next Steps](#next-steps)

---

## Prerequisites

**Required:**
- C++20 compiler (GCC 11+, Clang 14+, MSVC 2019+)
- CMake 3.16+
- Git

**Optional:**
- common_system (for Result<T> pattern - auto-fetched)
- thread_system (for thread pool monitoring)
- logger_system (for log correlation)

**Platform Support:**
- ✅ Linux (Ubuntu 20.04+, Fedora 34+)
- ✅ macOS (Big Sur 11+)
- ✅ Windows (10/11 with Visual Studio 2019+)

---

## Installation

Choose your preferred installation method:

### Option 1: CMake FetchContent (Recommended)

**CMakeLists.txt:**
```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    monitoring_system
    GIT_REPOSITORY https://github.com/kcenon/monitoring_system.git
    GIT_TAG        v1.0.0  # or main for latest
)

FetchContent_MakeAvailable(monitoring_system)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE monitoring_system::monitoring_system)
```

**Build:**
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

---

### Option 2: Git Submodule

```bash
# Add as submodule
git submodule add https://github.com/kcenon/monitoring_system.git third_party/monitoring_system
git submodule update --init --recursive

# Update CMakeLists.txt
add_subdirectory(third_party/monitoring_system)
target_link_libraries(my_app PRIVATE monitoring_system::monitoring_system)
```

---

### Option 3: System Install

```bash
git clone https://github.com/kcenon/monitoring_system.git
cd monitoring_system
mkdir build && cd build

cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local

cmake --build .
sudo cmake --install .
```

**Use in your project:**
```cmake
find_package(monitoring_system REQUIRED)
target_link_libraries(my_app PRIVATE monitoring_system::monitoring_system)
```

---

## Your First Monitoring Program

Create `main.cpp`:

```cpp
#include <monitoring_system/monitoring_system.hpp>
#include <iostream>
#include <thread>
#include <chrono>

using namespace kcenon::monitoring;

int main() {
    // 1. Create monitoring system
    auto result = monitoring_system::create();
    if (!result.has_value()) {
        std::cerr << "Failed to create monitoring system: "
                  << result.error().message() << "\n";
        return 1;
    }

    auto monitor = std::move(result.value());
    std::cout << "✅ Monitoring system created\n";

    // 2. Record some metrics
    monitor->record_counter("app_started_total", 1.0);
    monitor->record_gauge("app_version", 1.0);

    std::cout << "✅ Metrics recorded\n";

    // 3. Simulate some activity
    for (int i = 0; i < 10; ++i) {
        // Record request
        monitor->record_counter("requests_total", 1.0, {
            {"method", "GET"},
            {"status", "200"}
        });

        // Record latency
        double latency_ms = 10.0 + (rand() % 50);
        monitor->record_histogram("request_duration_ms", latency_ms);

        // Record active connections
        monitor->record_gauge("active_connections", 5.0 + (rand() % 10));

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "✅ Activity simulated\n";

    // 4. Query metrics
    auto metrics = monitor->get_metrics("requests_total");
    if (metrics.has_value()) {
        std::cout << "📊 Total requests: " << metrics.value().value << "\n";
    }

    auto latency = monitor->get_metrics("request_duration_ms");
    if (latency.has_value()) {
        std::cout << "📊 Average latency: " << latency.value().value << " ms\n";
    }

    std::cout << "✅ Monitoring demo complete!\n";
    return 0;
}
```

**Build and Run:**
```bash
g++ -std=c++20 -o my_app main.cpp -lmonitoring_system
./my_app
```

**Expected Output:**
```
✅ Monitoring system created
✅ Metrics recorded
✅ Activity simulated
📊 Total requests: 10
📊 Average latency: 32.5 ms
✅ Monitoring demo complete!
```

---

## Basic Metrics

### Counter - Monotonically Increasing Value

**Use for:** Request counts, error counts, bytes sent

```cpp
// Simple counter
monitor->record_counter("http_requests_total", 1.0);

// Counter with labels
monitor->record_counter("api_requests_total", 1.0, {
    {"method", "POST"},
    {"endpoint", "/api/users"},
    {"status", "201"}
});

// Increment by custom amount
monitor->record_counter("bytes_sent_total", 1024.0);

// Query counter
auto result = monitor->get_metrics("http_requests_total");
if (result.has_value()) {
    std::cout << "Total: " << result.value().value << "\n";
}
```

---

### Gauge - Point-in-Time Value

**Use for:** Memory usage, temperature, queue size, active connections

```cpp
// Memory usage
size_t memory_bytes = get_memory_usage();
monitor->record_gauge("memory_usage_bytes", static_cast<double>(memory_bytes));

// CPU usage
double cpu_percent = get_cpu_usage();
monitor->record_gauge("cpu_usage_percent", cpu_percent);

// Queue size (can go up and down)
monitor->record_gauge("message_queue_size", queue.size());

// Active connections
monitor->record_gauge("active_connections", conn_pool.active_count());
```

---

### Histogram - Distribution of Values

**Use for:** Request latencies, response sizes, processing times

```cpp
// Measure operation duration
auto start = std::chrono::steady_clock::now();

// ... perform operation ...

auto end = std::chrono::steady_clock::now();
auto duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

monitor->record_histogram("operation_duration_ms", duration_ms, {
    {"operation", "database_query"}
});

// Request size
monitor->record_histogram("request_size_bytes", request.size());

// Response time by endpoint
monitor->record_histogram("response_time_ms", response_time, {
    {"endpoint", "/api/users"},
    {"method", "GET"}
});
```

---

### Summary - Percentile Tracking

**Use for:** Response time percentiles (p50, p95, p99)

```cpp
// Track response times
monitor->record_summary("api_response_time_ms", latency);

// Query percentiles
auto p50 = monitor->get_percentile("api_response_time_ms", 0.50);
auto p95 = monitor->get_percentile("api_response_time_ms", 0.95);
auto p99 = monitor->get_percentile("api_response_time_ms", 0.99);

std::cout << "p50: " << p50.value() << " ms\n";
std::cout << "p95: " << p95.value() << " ms\n";
std::cout << "p99: " << p99.value() << " ms\n";
```

---

## Alerting

### Create Alert Rules

```cpp
#include <monitoring_system/alerting/alert_rule.hpp>

// High CPU usage alert
auto cpu_alert = alert_rule::builder()
    .with_name("high_cpu_usage")
    .with_metric("cpu_usage_percent")
    .with_condition(condition_type::greater_than, 80.0)
    .with_duration(std::chrono::minutes(5))  // Must be true for 5 min
    .with_severity(severity_level::warning)
    .with_message("CPU usage above 80% for 5 minutes")
    .build();

monitor->add_alert_rule(cpu_alert);

// High error rate alert
auto error_alert = alert_rule::builder()
    .with_name("high_error_rate")
    .with_metric("http_errors_total")
    .with_labels({{"status", "5xx"}})
    .with_condition(condition_type::rate_increase, 5.0)  // 5x increase
    .with_evaluation_window(std::chrono::minutes(10))
    .with_severity(severity_level::critical)
    .with_message("Error rate increased 5x in last 10 minutes")
    .build();

monitor->add_alert_rule(error_alert);

// Memory leak detection
auto memory_alert = alert_rule::builder()
    .with_name("memory_leak_suspected")
    .with_metric("memory_usage_bytes")
    .with_condition(condition_type::continuous_increase)
    .with_duration(std::chrono::hours(1))
    .with_severity(severity_level::warning)
    .with_message("Memory usage continuously increasing for 1 hour")
    .build();

monitor->add_alert_rule(memory_alert);
```

---

### Configure Notifications

```cpp
#include <monitoring_system/alerting/notification_channel.hpp>

// Console notifications (for testing)
auto console_channel = console_notification_channel::create();
monitor->add_notification_channel("console", console_channel);

// Email notifications
auto email_channel = email_notification_channel::builder()
    .with_smtp_server("smtp.gmail.com", 587)
    .with_credentials("alerts@myapp.com", "password")
    .with_recipients({"team@myapp.com"})
    .with_tls(true)
    .build();

monitor->add_notification_channel("email", email_channel);

// Webhook notifications
auto webhook_channel = webhook_notification_channel::builder()
    .with_url("https://hooks.slack.com/services/YOUR/WEBHOOK/URL")
    .with_method("POST")
    .with_headers({{"Content-Type", "application/json"}})
    .build();

monitor->add_notification_channel("slack", webhook_channel);

// Route alerts by severity
monitor->set_routing_rules({
    {severity_level::critical, {"slack", "email"}},
    {severity_level::warning, {"slack"}},
    {severity_level::info, {"console"}}
});
```

---

## Distributed Tracing

### Basic Tracing

```cpp
#include <monitoring_system/tracing/distributed_tracer.hpp>

auto tracer = monitor->get_tracer();

// Start a trace
auto trace_ctx = tracer->start_trace("user_request");

// Create spans for operations
{
    auto auth_span = tracer->start_span("authenticate_user", trace_ctx);
    auth_span->set_tag("user_id", "12345");

    // ... authentication logic ...

    if (auth_failed) {
        auth_span->set_tag("error", true);
        auth_span->log_event("auth_failed", "Invalid credentials");
    }
}

{
    auto db_span = tracer->start_span("database_query", trace_ctx);
    db_span->set_tag("query_type", "SELECT");
    db_span->set_tag("table", "users");

    // ... database query ...
}

{
    auto response_span = tracer->start_span("generate_response", trace_ctx);

    // ... generate response ...
}

// Finish trace
tracer->finish_trace(trace_ctx);

// Query trace
auto trace = tracer->get_trace(trace_ctx.trace_id());
if (trace.has_value()) {
    std::cout << "Trace duration: " << trace.value().duration_ms() << " ms\n";
    std::cout << "Spans: " << trace.value().span_count() << "\n";
}
```

---

### Tracing Across Services

**Service A (Sender):**
```cpp
// Start trace
auto trace_ctx = tracer->start_trace("cross_service_request");

// Inject context into HTTP headers
auto headers = tracer->inject_context(trace_ctx);
// headers contains: X-Trace-Id, X-Span-Id, etc.

// Make HTTP request
http_client client;
auto response = client.post("http://service-b/api/process",
                            data,
                            headers);
```

**Service B (Receiver):**
```cpp
// Extract context from HTTP headers
auto trace_ctx_result = tracer->extract_context(request.headers());

if (trace_ctx_result.has_value()) {
    auto trace_ctx = trace_ctx_result.value();

    // Continue trace
    auto span = tracer->start_span("process_request", trace_ctx);

    // ... process request ...
}
```

---

## Web Dashboard

### Enable Dashboard

```cpp
#include <monitoring_system/dashboard/web_dashboard.hpp>

// Create monitoring system with dashboard
auto config = monitoring_config::builder()
    .with_dashboard(true)
    .with_dashboard_port(8080)
    .build();

auto monitor = monitoring_system::create(config).value();

// Start dashboard
auto dashboard_result = monitor->start_dashboard();
if (dashboard_result.has_value()) {
    std::cout << "🌐 Dashboard available at http://localhost:8080\n";
}

// Keep application running
std::cout << "Press Enter to stop...\n";
std::cin.get();

// Stop dashboard
monitor->stop_dashboard();
```

**Dashboard Features:**
- 📊 Real-time metrics visualization
- 🔍 Metric search and filtering
- 📈 Time-series graphs
- 🚨 Active alerts
- 🔎 Trace explorer
- ⚙️ System health status

**Access Dashboard:**
```bash
# Open in browser
xdg-open http://localhost:8080      # Linux
open http://localhost:8080           # macOS
start http://localhost:8080          # Windows
```

---

## Next Steps

### 1. Explore Advanced Features

**Custom Collectors:**
```cpp
// Create custom metric collector
class MyCollector : public metric_collector_interface {
public:
    auto collect() -> Result<std::vector<metric_sample>> override {
        // Collect your custom metrics
        return Result<std::vector<metric_sample>>::ok(samples);
    }

    auto name() const -> std::string override {
        return "my_collector";
    }
};

auto collector = std::make_shared<MyCollector>();
monitor->register_collector(collector);
monitor->start_collection(std::chrono::seconds(10));  // Collect every 10s
```

**Storage Configuration:**
```cpp
// Configure persistent storage
auto storage_config = storage_config::builder()
    .with_backend(storage_backend::file)
    .with_path("/var/lib/myapp/metrics")
    .with_compression(compression_type::zstd)
    .with_retention(std::chrono::days(7))
    .build();

monitor->configure_storage(storage_config);
```

---

### 2. Integrate with Other Systems

**Thread System Integration:**
```cpp
#include <monitoring_system/adapters/thread_system_adapter.hpp>

auto config = monitoring_config::builder()
    .with_thread_system_integration()
    .build();

// Automatic thread pool metrics:
// - thread_pool_active_threads
// - thread_pool_queued_tasks
// - thread_pool_task_duration_ms
```

**Logger System Integration:**
```cpp
#include <monitoring_system/adapters/logger_system_adapter.hpp>

auto config = monitoring_config::builder()
    .with_logger_system_integration()
    .with_log_level_metrics(true)
    .build();

// Automatic log metrics:
// - log_messages_total{level="error"}
// - log_messages_total{level="warn"}
```

---

### 3. Production Deployment

**Performance Tuning:**
```cpp
auto config = monitoring_config::builder()
    // Reduce overhead
    .with_sampling_rate(0.01)  // Sample 1% of high-frequency metrics

    // Thread-local buffering
    .with_thread_local_collection(true)

    // Batch recording
    .with_batch_size(100)

    // Retention policy
    .with_retention(std::chrono::hours(24))
    .with_max_samples(1'000'000)

    .build();
```

**Export to External Systems:**
```cpp
// Prometheus exporter
auto prom_exporter = prometheus_exporter::builder()
    .with_port(9090)
    .with_path("/metrics")
    .build();

monitor->add_exporter(prom_exporter);

// OpenTelemetry exporter
auto otlp_exporter = otlp_exporter::builder()
    .with_endpoint("http://otel-collector:4317")
    .build();

monitor->add_trace_exporter(otlp_exporter);
```

---

### 4. Learn More

**Documentation:**
- 📘 **[Architecture Guide](../ARCHITECTURE_GUIDE.md)** - System design and internals
- 📖 **[API Reference](../API_REFERENCE.md)** - Complete API documentation
- ❓ **[FAQ](FAQ.md)** - Frequently asked questions (25+ Q&A)
- ✨ **[Best Practices](BEST_PRACTICES.md)** - Production patterns
- 🐛 **[Troubleshooting](../TROUBLESHOOTING.md)** - Common issues

**Examples:**
- **[Tutorial](TUTORIAL.md)** - Step-by-step tutorial with examples
- **[Samples](../../samples/)** - Example applications

**Community:**
- **GitHub Issues**: [Report bugs](https://github.com/kcenon/monitoring_system/issues)
- **Discussions**: [Ask questions](https://github.com/kcenon/monitoring_system/discussions)
- **Contributing**: [CONTRIBUTING.md](../CONTRIBUTING.md)

---

## Quick Reference

### Metric Types

| Type | Use Case | Example |
|------|----------|---------|
| Counter | Cumulative counts | `requests_total`, `errors_total` |
| Gauge | Point-in-time values | `memory_bytes`, `cpu_percent` |
| Histogram | Distribution | `request_duration_ms`, `response_size_bytes` |
| Summary | Percentiles | `api_latency_ms` (p50, p95, p99) |

### Common Patterns

```cpp
// Record with labels
monitor->record_counter("metric_name", value, {
    {"label1", "value1"},
    {"label2", "value2"}
});

// Query with labels
auto result = monitor->get_metrics("metric_name", {
    {"label1", "value1"}
});

// Time-range query
auto metrics = monitor->query_range(
    "metric_name",
    start_time,
    end_time,
    std::chrono::seconds(60)  // resolution
);

// Trace operation
auto trace_ctx = tracer->start_trace("operation");
auto span = tracer->start_span("sub_operation", trace_ctx);
// ... operation ...
tracer->finish_trace(trace_ctx);
```

---

**Congratulations!** 🎉 You've completed the quick start guide.

You now know how to:
- ✅ Install monitoring_system
- ✅ Record metrics (counter, gauge, histogram, summary)
- ✅ Create alerts and notifications
- ✅ Implement distributed tracing
- ✅ Use the web dashboard

**Happy Monitoring!** 📊

---

**Last Updated:** 2025-11-11
**Version:** 0.1.0
