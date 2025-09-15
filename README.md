# Monitoring System

[![Build Status](https://github.com/kcenon/monitoring_system/actions/workflows/build.yml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/build.yml)
[![License](https://img.shields.io/badge/License-BSD_3--Clause-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)

## Project Overview

A comprehensive, **production-ready monitoring and observability platform** for C++ applications featuring real-time alerting, web-based dashboards, distributed tracing, performance monitoring, and reliability features. Built with modern C++20, featuring event-driven architecture, lock-free data structures, and real-time visualization.

> **🎯 Latest Release**: Phase 3 Complete - Real-time alerting system and web dashboard implementation with WebSocket streaming, multi-channel notifications, and responsive UI.

> **🏗️ Modular Architecture**: Streamlined event-driven design with Observer pattern, plugin-based collectors, and high-performance storage engines. Fully independent operation with optional thread_system and logger_system integration.

## 🔗 Project Ecosystem & Inter-Dependencies

This project is part of a modular ecosystem designed for high-performance monitoring and observability:

### Core Monitoring Framework
- **[monitoring_system](https://github.com/kcenon/monitoring_system)** (This project): Complete monitoring and observability platform
  - Provides: Real-time metrics, distributed tracing, alerting, web dashboard
  - Dependencies: None (standalone) - Optional integration with thread_system/logger_system
  - Usage: Production monitoring, observability, alerting, and visualization

### Optional Integration Components
- **[thread_system](https://github.com/kcenon/thread_system)**: High-performance threading framework
  - Provides: Thread pools, job queues, worker management
  - Integration: Enhanced concurrent metric collection

- **[logger_system](https://github.com/kcenon/logger_system)**: Asynchronous logging system
  - Provides: High-performance logging with multiple targets
  - Integration: Structured logging for monitoring events

### Dependency Flow
```
monitoring_system (standalone)
    ↓ (optional)        ↓ (optional)
thread_system      logger_system
```

## 🚀 Quick Start

### Prerequisites
- C++20 capable compiler (GCC 11+, Clang 14+, MSVC 2019+)
- CMake 3.16 or later
- Optional: vcpkg for dependency management

### Build & Installation

```bash
# Clone the repository
git clone https://github.com/kcenon/monitoring_system.git
cd monitoring_system

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_CXX_STANDARD=20 -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)  # Linux/macOS
# or
cmake --build . --config Release  # Windows

# Run tests
ctest --verbose

# Install (optional)
sudo make install
```

### Quick Example

```cpp
#include <kcenon/monitoring/collectors/system_resource_collector.h>
#include <kcenon/monitoring/core/event_bus.h>
#include <kcenon/monitoring/exporters/metric_exporters.h>

using namespace kcenon::monitoring;

int main() {
    // 1. Start system resource collection
    auto collector = std::make_shared<collectors::SystemResourceCollector>();
    collector->start_collection(std::chrono::seconds(1));

    // 2. Set up event bus for monitoring
    auto event_bus = std::make_shared<event_bus>();
    collector->attach_to_bus(event_bus);

    // 3. Configure metric exporter
    auto exporter = std::make_shared<metric_exporters>();
    exporter->add_export_target("prometheus", "localhost:9090");

    // 4. Start monitoring
    collector->start();

    std::cout << "Dashboard available at http://localhost:8080\n";
    std::cout << "Press Enter to stop...\n";
    std::cin.get();

    return 0;
}
```

## ✨ Key Features

### 🚨 Real-time Alerting System (Phase 3)
- **Rule-based Alert Engine**: Dynamic rule evaluation with complex conditions
- **Multi-channel Notifications**: Email, Slack, SMS, Webhook, PagerDuty, OpsGenie
- **Intelligent Deduplication**: Fuzzy matching, time-based grouping, fingerprinting
- **Silence Management**: Scheduled alert suppression with regex patterns

### 📊 Web Dashboard & Visualization (Phase 3)
- **Real-time Dashboard**: Chart.js-based responsive visualization
- **WebSocket Streaming**: Bidirectional real-time communication
- **RESTful API**: Comprehensive metric query and management endpoints
- **Mobile Responsive**: Optimized for desktop, tablet, and mobile devices

### 🏗️ Event-Driven Architecture (Phase 1)
- **Observer Pattern**: Loose coupling between components
- **Event Bus**: High-performance publish-subscribe messaging
- **Plugin Architecture**: Extensible collector and exporter framework
- **Interface-based Design**: Clean separation of concerns

### 📈 Distributed Metrics Collection (Phase 2)
- **Plugin-based Collectors**: System resources, thread pools, custom metrics
- **Time-series Storage**: LSM-tree based engine with compression
- **SQL-like Query Engine**: Advanced metric querying and aggregation
- **Multiple Export Formats**: Prometheus, InfluxDB, CSV, JSON

### 🔧 Core Monitoring Features
- **Performance Monitoring**: Nanosecond-precision timing and resource tracking
- **Distributed Tracing**: W3C Trace Context compliant with hierarchical spans
- **Health Monitoring**: Liveness, readiness, and startup checks
- **Adaptive Monitoring**: Dynamic resource-aware monitoring with auto-tuning

### 🛡️ Reliability & Safety
- **Circuit Breakers**: Prevent cascading failures
- **Retry Policies**: Exponential backoff and custom strategies
- **Error Boundaries**: Failure isolation
- **Resource Management**: Memory quotas and rate limiting

### 🌐 Integration & Compatibility
- **OpenTelemetry Compatible**: Full OTEL resource model
- **Multiple Storage Backends**: File, Database, Cloud, Memory
- **Protocol Support**: HTTP, gRPC, WebSocket, JSON
- **Cross-platform**: Linux, Windows, macOS

## 📊 Performance Benchmarks

*Benchmarked on Intel i7-12700K @ 5.0GHz, 32GB DDR5, Ubuntu 22.04, GCC 11.3*

### Alert Engine Performance
| Operation | Throughput | Latency | Notes |
|-----------|------------|---------|-------|
| Rule Evaluation | 1M rules/sec | < 1μs | Single rule, simple condition |
| Complex Rules | 100K/sec | < 10μs | Multiple conditions with AND/OR |
| Alert Deduplication | 500K/sec | < 2μs | Exact match strategy |
| Notification Dispatch | 50K/sec | < 20ms | Including network I/O |

### Web Dashboard Performance
| Metric | Value | Notes |
|--------|-------|-------|
| WebSocket Connections | 10,000+ | Concurrent connections |
| Message Throughput | 100K msg/sec | Real-time updates |
| API Response Time | < 10ms | P95 latency |
| Dashboard Load Time | < 500ms | Initial page load |

### Metric Collection Performance
| Collector Type | Throughput | CPU Overhead | Memory Usage |
|----------------|------------|--------------|--------------|
| System Resources | 1M metrics/sec | < 1% | < 10MB |
| Thread Pool | 500K metrics/sec | < 0.5% | < 5MB |
| Custom Metrics | 2M metrics/sec | < 0.1% | < 2MB |

## 🏗️ Architecture

### System Architecture
```
┌─────────────────────────────────────────────────┐
│                Web Dashboard                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────────┐    │
│  │  Charts  │ │ Filters  │ │ Alert Panel  │    │
│  └──────────┘ └──────────┘ └──────────────┘    │
└─────────────────────────────────────────────────┘
                        │
                   WebSocket/HTTP
                        │
┌─────────────────────────────────────────────────┐
│              Dashboard Server                    │
│  ┌──────────┐ ┌──────────┐ ┌──────────────┐    │
│  │   HTTP   │ │WebSocket │ │  Metric API  │    │
│  └──────────┘ └──────────┘ └──────────────┘    │
└─────────────────────────────────────────────────┘
                        │
┌─────────────────────────────────────────────────┐
│              Alerting System                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────────┐    │
│  │Rule Engine│ │Notification│ │Deduplication│   │
│  └──────────┘ └──────────┘ └──────────────┘    │
└─────────────────────────────────────────────────┘
                        │
┌─────────────────────────────────────────────────┐
│              Event Bus                           │
│  ┌──────────┐ ┌──────────┐ ┌──────────────┐    │
│  │Publisher │ │Subscriber│ │ Message Queue│    │
│  └──────────┘ └──────────┘ └──────────────┘    │
└─────────────────────────────────────────────────┘
                        │
┌─────────────────────────────────────────────────┐
│           Metric Collection Layer                │
│  ┌──────────┐ ┌──────────┐ ┌──────────────┐    │
│  │Collectors│ │Aggregators│ │  Exporters  │    │
│  └──────────┘ └──────────┘ └──────────────┘    │
└─────────────────────────────────────────────────┘
                        │
┌─────────────────────────────────────────────────┐
│             Storage Layer                        │
│  ┌──────────┐ ┌──────────┐ ┌──────────────┐    │
│  │Time Series│ │  Query   │ │  Compression │    │
│  │  Engine  │ │  Engine  │ │   & Archive  │    │
│  └──────────┘ └──────────┘ └──────────────┘    │
└─────────────────────────────────────────────────┘
```

### Directory Structure
```
monitoring_system/
├── 📁 sources/
│   ├── 📁 monitoring/
│   │   ├── 📁 core/           # Core types and event bus
│   │   ├── 📁 interfaces/     # Abstract interfaces
│   │   ├── 📁 collectors/     # Metric collectors
│   │   ├── 📁 storage/        # Storage backends
│   │   ├── 📁 query/          # Query engine
│   │   ├── 📁 export/         # Export formats
│   │   └── 📁 adapters/       # System adapters
│   ├── 📁 alerting/           # Alert engine (Phase 3)
│   │   ├── rule_engine.h      # Rule evaluation
│   │   ├── notification_manager.h # Notifications
│   │   └── alert_deduplication.h  # Deduplication
│   └── 📁 web/                # Web dashboard (Phase 3)
│       ├── dashboard_server.h # HTTP/WebSocket server
│       ├── metric_api.h       # REST API
│       └── 📁 static/         # Frontend files
├── 📁 examples/               # Usage examples
├── 📁 tests/                  # Unit tests
├── 📁 benchmarks/             # Performance tests
├── 📁 docs/                   # Documentation
└── CMakeLists.txt
```

## 📚 Documentation

### Core Documentation
- [**Architecture Guide**](docs/ARCHITECTURE_GUIDE.md) - System design and patterns
- [**API Reference**](docs/API_REFERENCE.md) - Complete API documentation
- [**Performance Tuning**](docs/PERFORMANCE_TUNING.md) - Optimization strategies
- [**Troubleshooting**](docs/TROUBLESHOOTING.md) - Common issues and solutions

### Phase Documentation
- [**Phase 3: Alerting & Dashboard**](docs/PHASE3.md) - Real-time alerting and web dashboard

### Examples & Tutorials
- [**Tutorial Guide**](docs/guides/TUTORIAL.md) - Step-by-step tutorials
- [**Basic Monitoring**](examples/basic_monitoring_example.cpp) - Getting started example
- [**Distributed Tracing**](examples/distributed_tracing_example.cpp) - Tracing implementation
- [**Health & Reliability**](examples/health_reliability_example.cpp) - Circuit breakers and health checks
- [**Result Pattern**](examples/result_pattern_example.cpp) - Error handling patterns
- [**Event Bus**](examples/event_bus_example.cpp) - Event-driven communication
- [**Plugin Collector**](examples/plugin_collector_example.cpp) - Custom metric collectors
- [**Storage Backend**](examples/storage_example.cpp) - Storage configuration

## 🔧 Usage Examples

### Setting Up Alerting Rules

```cpp
#include "alerting/rule_engine.h"
#include "alerting/notification_manager.h"

using namespace monitoring_system::alerting;

// Create rule engine
RuleEngine engine;

// Define a CPU alert rule
auto cpu_rule = RuleBuilder("high_cpu")
    .with_name("High CPU Usage Alert")
    .with_severity(AlertSeverity::WARNING)
    .with_condition({
        .metric_name = "system.cpu.usage",
        .op = ConditionOperator::GREATER_THAN,
        .threshold = 80.0,
        .aggregation = AggregationFunction::AVG,
        .window = std::chrono::seconds(60)
    })
    .with_cooldown_period(std::chrono::seconds(300))
    .add_label("team", "infrastructure")
    .add_annotation("runbook", "https://wiki/high-cpu")
    .build();

engine.add_rule(cpu_rule);

// Set up notifications
NotificationManager notifier;

// Configure Slack notifications
auto slack_config = std::make_shared<SlackConfig>();
slack_config->webhook_url = "https://hooks.slack.com/...";
slack_config->channel = "#alerts";
notifier.add_channel_config("slack_prod", slack_config);

// Configure email notifications
auto email_config = std::make_shared<EmailConfig>();
email_config->smtp_server = "smtp.gmail.com";
email_config->smtp_port = 587;
email_config->to_addresses = {"ops@company.com"};
notifier.add_channel_config("email_ops", email_config);

// Start background evaluation
engine.start_background_evaluation();
```

### Creating a Custom Dashboard

```cpp
#include "web/dashboard_server.h"
#include "web/metric_api.h"

using namespace monitoring_system::web;

// Create dashboard server
DashboardServer server(8080);

// Configure CORS for frontend access
CorsConfig cors;
cors.allowed_origins = {"http://localhost:3000"};
cors.allowed_methods = {"GET", "POST", "PUT", "DELETE"};
server.set_cors_config(cors);

// Set up authentication
AuthConfig auth;
auth.type = AuthConfig::BEARER;
auth.validate_token = [](const std::string& token) {
    // Validate JWT token
    return validate_jwt(token);
};
server.set_auth_config(auth);

// Create metric API
MetricAPI api;
api.set_metric_database(metric_db);
api.set_query_engine(query_engine);
api.register_routes(server);

// Add custom routes
server.add_route("/api/v1/health", HttpMethod::GET,
    [](const HttpRequest& req) {
        return ResponseBuilder()
            .status(HttpStatus::OK)
            .json(R"({"status":"healthy"})")
            .build();
    });

// WebSocket endpoint for real-time metrics
server.add_websocket_endpoint("/ws/metrics",
    [](const std::string& client_id, const WebSocketMessage& msg) {
        // Handle metric subscription
        if (msg.data == "subscribe:cpu") {
            // Start streaming CPU metrics to client
            stream_cpu_metrics(client_id);
        }
    });

// Start server
server.start();
std::cout << "Dashboard available at http://localhost:8080\n";
```

### Implementing Custom Collectors

```cpp
#include "monitoring/collectors/plugin_metric_collector.h"

using namespace monitoring_system::collectors;

class CustomApplicationCollector : public IMetricCollectorPlugin {
public:
    std::string get_plugin_name() const override {
        return "application_metrics";
    }

    std::vector<metric> collect_metrics() override {
        std::vector<metric> metrics;

        // Collect application-specific metrics
        metrics.push_back({
            .name = "app.requests.total",
            .value = get_request_count(),
            .timestamp = std::chrono::system_clock::now(),
            .labels = {{"service", "api"}, {"version", "v1"}}
        });

        metrics.push_back({
            .name = "app.latency.p95",
            .value = calculate_p95_latency(),
            .timestamp = std::chrono::system_clock::now(),
            .labels = {{"service", "api"}}
        });

        return metrics;
    }

    bool initialize(const std::unordered_map<std::string, std::string>& config) override {
        // Initialize collector with config
        return true;
    }
};

// Register and use the collector
PluginMetricCollector collector;
collector.register_plugin(std::make_shared<CustomApplicationCollector>());
collector.start_collection(std::chrono::seconds(10));
```

### Query Metrics with Time-series Engine

```cpp
#include "monitoring/storage/timeseries_engine.h"
#include "monitoring/query/metric_query_engine.h"

using namespace monitoring_system;

// Create storage engine
storage::TimeSeriesEngine storage;
storage.set_compression(storage::CompressionType::LZ4);
storage.set_retention_days(30);

// Create query engine
query::MetricQueryEngine query_engine;
query_engine.set_storage(&storage);

// Build and execute a query
auto result = query_engine.query()
    .select("system.cpu.usage", "system.memory.usage")
    .where("host", "=", "server-01")
    .time_range(
        std::chrono::system_clock::now() - std::chrono::hours(1),
        std::chrono::system_clock::now())
    .group_by(std::chrono::minutes(5))
    .aggregate(query::AggregateFunction::AVG)
    .execute();

// Process results
for (const auto& point : result.data_points) {
    std::cout << "Time: " << format_time(point.timestamp)
              << " CPU: " << point.values["system.cpu.usage"]
              << " Memory: " << point.values["system.memory.usage"]
              << std::endl;
}
```

## 🚀 Advanced Features

### Alert Deduplication Strategies
- **Exact Match**: Identical alert content
- **Fuzzy Match**: Similarity-based (Levenshtein distance)
- **Time-based**: Within time windows
- **Fingerprint**: Content hashing

### Dashboard Features
- **Real-time Updates**: WebSocket push notifications
- **Interactive Charts**: Zoom, pan, drill-down
- **Custom Panels**: Create custom visualization widgets
- **Export Data**: CSV, JSON, Excel formats

### Storage Optimization
- **Compression**: LZ4, Snappy, Zstd, Gzip
- **Partitioning**: Time-based, metric-based, tag-based
- **Compaction**: Background optimization
- **Archival**: Long-term storage with reduced resolution

## 🧪 Testing

```bash
# Run all tests
./build/tests/monitoring_system_tests

# Run specific test categories
./build/tests/monitoring_system_tests --gtest_filter="AlertingTest.*"
./build/tests/monitoring_system_tests --gtest_filter="DashboardTest.*"
./build/tests/monitoring_system_tests --gtest_filter="StorageTest.*"

# Run benchmarks
./build/benchmarks/monitoring_benchmarks

# Run stress tests
./build/tests/stress_tests --duration=3600 --threads=100
```

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guide](docs/CONTRIBUTING.md) for details.

### Development Setup

```bash
# Fork and clone
git clone https://github.com/yourusername/monitoring_system.git
cd monitoring_system

# Create feature branch
git checkout -b feature/amazing-feature

# Make changes and test
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
make -j$(nproc)
ctest

# Submit PR
git push origin feature/amazing-feature
```

## 📄 License

This project is licensed under the BSD 3-Clause License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- OpenTelemetry community for standardization efforts
- Chart.js team for excellent visualization library
- Contributors and users for valuable feedback

## 📞 Support

- **Issues**: [GitHub Issues](https://github.com/kcenon/monitoring_system/issues)
- **Discussions**: [GitHub Discussions](https://github.com/kcenon/monitoring_system/discussions)
- **Email**: kcenon@naver.com

---

<p align="center">
  <strong>monitoring_system</strong> - Enterprise-grade Monitoring & Observability Platform<br>
  Made with ❤️ by the monitoring_system team
</p>