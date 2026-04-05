# Getting Started with monitoring_system

A step-by-step guide to using `monitoring_system` -- from installation through
metric collectors, alert pipelines, distributed tracing, OTLP export, and plugin
development.

## Prerequisites

| Requirement | Minimum | Recommended |
|-------------|---------|-------------|
| C++ standard | C++20 | C++20 |
| CMake | 3.20 | 3.28+ |
| GCC | 13 | 13+ |
| Clang | 17 | 17+ |
| Apple Clang | 14 | 15+ |
| MSVC | 2022 (17.0) | 2022 (17.8+) |

**Required dependencies**:
- [common_system](https://github.com/kcenon/common_system) (Tier 0)
- [thread_system](https://github.com/kcenon/thread_system) (Tier 1)

**Optional**: logger_system, network_system (for HTTP transport), gRPC (for OTLP
gRPC transport).

## Installation

### Option A -- CMake FetchContent (recommended)

```cmake
include(FetchContent)

FetchContent_Declare(
    monitoring_system
    GIT_REPOSITORY https://github.com/kcenon/monitoring_system.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(monitoring_system)

target_link_libraries(your_target PRIVATE monitoring_system)
```

### Option B -- Clone and build locally

```bash
# Clone alongside sibling dependencies
git clone https://github.com/kcenon/common_system.git
git clone https://github.com/kcenon/thread_system.git
git clone https://github.com/kcenon/monitoring_system.git
cd monitoring_system

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Option C -- Add as a Git submodule

```bash
git submodule add https://github.com/kcenon/monitoring_system.git external/monitoring_system
```

Then in your `CMakeLists.txt`:

```cmake
add_subdirectory(external/monitoring_system)
target_link_libraries(your_target PRIVATE monitoring_system)
```

## First Collector

Create a custom metric collector using the CRTP base class.

```cpp
#include <kcenon/monitoring/collectors/collector_base.h>
#include <iostream>

class my_collector : public kcenon::monitoring::collector_base<my_collector> {
public:
    static constexpr const char* collector_name = "my_collector";

    bool do_initialize(const kcenon::monitoring::config_map& config) {
        // Collector-specific initialization
        return true;
    }

    std::vector<kcenon::monitoring::metric> do_collect() {
        // Collect your application-specific metrics
        kcenon::monitoring::metric m;
        m.name = "app_requests_total";
        m.type = kcenon::monitoring::metric_type::counter;
        m.value = get_request_count();
        return {m};
    }

    bool is_available() const {
        return true;
    }

    std::vector<std::string> do_get_metric_types() const {
        return {"app_requests_total"};
    }

    void do_add_statistics(kcenon::monitoring::stats_map& stats) const {
        stats["last_request_count"] = get_request_count();
    }
};
```

### Using the Collector Factory

```cpp
#include <kcenon/monitoring/factory/metric_factory.h>

auto& factory = kcenon::monitoring::metric_factory::instance();

// Register your collector
factory.register_collector<my_collector>("my_collector");

// Create from the factory with configuration
kcenon::monitoring::config_map config = {{"enabled", "true"}};
auto result = factory.create("my_collector", config);
if (result) {
    auto& collector = result.collector;
    // Use the collector
}
```

### Built-in Collectors

The factory provides many built-in collectors:

| Collector | Metrics |
|-----------|---------|
| `system_resource_collector` | CPU, memory, disk usage |
| `process_metrics_collector` | Process CPU/memory |
| `network_metrics_collector` | Network I/O statistics |
| `temperature_collector` | Hardware temperature sensors |
| `battery_collector` | Battery level and state |
| `gpu_collector` | GPU utilization |
| `uptime_collector` | System uptime |

## Alert Pipeline

Set up metric-driven alerting with thresholds, aggregation, and notifications.

### Define Alert Rules

```cpp
#include <kcenon/monitoring/alert/alert_rule.h>
#include <kcenon/monitoring/alert/alert_triggers.h>

// Create a rule
kcenon::monitoring::alert_rule rule("high_cpu");
rule.set_severity(kcenon::monitoring::alert_severity::critical)
    .set_summary("CPU usage is high")
    .set_description("CPU usage exceeded 80%")
    .add_label("team", "infrastructure")
    .set_for_duration(std::chrono::minutes(5));

// Set a threshold trigger
rule.set_trigger(kcenon::monitoring::threshold_trigger::above(80.0));
```

### Trigger Types

```cpp
// Threshold: value > 80
auto trigger = threshold_trigger::above(80.0);

// Threshold: value < 10
auto trigger = threshold_trigger::below(10.0);

// Custom comparison
auto trigger = std::make_shared<threshold_trigger>(
    5.0, comparison_operator::greater_or_equal);
```

### Alert Aggregation

```cpp
#include <kcenon/monitoring/alert/alert_pipeline.h>

kcenon::monitoring::alert_aggregator_config agg_config;
agg_config.group_by_labels = {"service", "environment"};
agg_config.group_wait = std::chrono::seconds(30);
agg_config.group_interval = std::chrono::minutes(5);

kcenon::monitoring::alert_aggregator aggregator(agg_config);

aggregator.add_alert(cpu_alert);
aggregator.add_alert(memory_alert);

// Get grouped alerts ready for notification
auto groups = aggregator.get_ready_groups();
for (auto& group : groups) {
    notify_team(group);
    aggregator.mark_sent(group.group_key);
}
```

## Distributed Tracing

W3C-style tracing with trace_id, span_id, and parent propagation.

```cpp
#include <kcenon/monitoring/tracing/trace_context.h>
#include <kcenon/monitoring/tracing/distributed_tracer.h>

// Create a root trace
auto root = kcenon::monitoring::trace_context::create_root("handle_request");

// Create child spans for sub-operations
auto db_span = root.create_child("query_database");
// ... perform database query ...

auto cache_span = root.create_child("check_cache");
// ... check cache ...
```

### Distributed Tracer

```cpp
kcenon::monitoring::distributed_tracer tracer;

// Start a span
auto span = tracer.start_span("process_order", root_context);

// Add attributes
span.set_attribute("order_id", "12345");
span.set_attribute("customer", "Alice");

// End span
span.end();

// Export all spans
tracer.export_spans();
```

## OTLP Export

Export traces to OpenTelemetry-compatible backends (Jaeger, Grafana Tempo, etc.).

### gRPC Transport

```cpp
#include <kcenon/monitoring/exporters/otlp_grpc_exporter.h>

kcenon::monitoring::otlp_grpc_config config;
config.endpoint = "localhost:4317";
config.service_name = "my_service";
config.service_version = "1.0.0";
config.max_batch_size = 512;
config.timeout = std::chrono::milliseconds(10000);
config.use_tls = false;

// Custom resource attributes
config.resource_attributes = {
    {"deployment.environment", "production"},
    {"host.name", "server-01"}
};

auto exporter = std::make_shared<kcenon::monitoring::otlp_grpc_exporter>(config);
exporter->start();

// Export spans
exporter->export_spans(collected_spans);

exporter->stop();
```

### HTTP Transport

```cpp
#include <kcenon/monitoring/exporters/http_transport.h>

// Alternative HTTP-based OTLP export (does not require gRPC)
// Requires MONITORING_WITH_NETWORK_SYSTEM=ON
```

## Plugin Development

Create custom collector plugins for dynamic loading.

```cpp
#include <kcenon/monitoring/plugins/collector_plugin.h>
#include <kcenon/monitoring/plugins/plugin_api.h>

class my_plugin : public kcenon::monitoring::collector_plugin {
public:
    std::string name() const override { return "my_plugin"; }
    std::string version() const override { return "1.0.0"; }

    bool initialize(const kcenon::monitoring::config_map& config) override {
        // Plugin initialization
        return true;
    }

    std::vector<kcenon::monitoring::metric> collect() override {
        // Collect custom metrics
        return {};
    }
};
```

### Register and Load Plugins

```cpp
#include <kcenon/monitoring/plugins/collector_registry.h>
#include <kcenon/monitoring/plugins/plugin_loader.h>

// Static registration
kcenon::monitoring::collector_registry::instance()
    .register_plugin<my_plugin>();

// Dynamic loading from shared library
kcenon::monitoring::plugin_loader loader;
loader.load("path/to/my_plugin.so");
```

## Health Monitoring

```cpp
#include <kcenon/monitoring/health/health_monitor.h>

kcenon::monitoring::health_monitor monitor({
    .check_interval = std::chrono::milliseconds(5000),
    .enable_auto_recovery = true,
    .max_consecutive_failures = 3
});

auto db_check = kcenon::monitoring::health_check_builder()
    .with_name("database")
    .with_type(kcenon::monitoring::health_check_type::readiness)
    .with_check([] { return check_db_connection(); })
    .critical(true)
    .build();

monitor.register_check("database", db_check);
monitor.start();

// Query health status
auto status = monitor.overall_status();
auto stats = monitor.get_stats();
```

## Next Steps

| Topic | Resource |
|-------|----------|
| Full API surface | [API_REFERENCE.md](API_REFERENCE.md) |
| API cheat sheet | [API_QUICK_REFERENCE.md](API_QUICK_REFERENCE.md) |
| Plugin architecture | [plugin_architecture.md](plugin_architecture.md) |
| Plugin development guide | [plugin_development_guide.md](plugin_development_guide.md) |
| Plugin API reference | [plugin_api_reference.md](plugin_api_reference.md) |
| Architecture | [ARCHITECTURE.md](ARCHITECTURE.md) |
| Benchmarks | [BENCHMARKS.md](BENCHMARKS.md) |
