# Monitoring System

A comprehensive, **production-ready monitoring system** for C++ applications with distributed tracing, performance monitoring, health checks, and reliability features. Built with modern C++17, featuring explicit error handling, lock-free data structures, and zero-copy operations.

## 🎉 Project Status: **COMPLETE**

All 6 phases and 24 tasks have been successfully completed (100%). The monitoring system is production-ready with comprehensive testing, documentation, and examples.

## 🚀 Quick Start

### Prerequisites

- C++17 or later compiler
- CMake 3.15+
- Thread support

### Build

```bash
git clone <repository-url>
cd monitoring_system
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Run Examples

```bash
# Basic monitoring example
./examples/basic_monitoring_example

# Distributed tracing example
./examples/distributed_tracing_example

# Health monitoring and reliability example
./examples/health_reliability_example

# Result pattern usage example
./examples/result_pattern_example
```

### Run Tests

```bash
# Run all tests
./tests/monitoring_system_tests

# Run specific test suites
./tests/monitoring_system_tests --gtest_filter="*Performance*"
./tests/monitoring_system_tests --gtest_filter="*DistributedTracing*"
```

## ✨ Key Features

### 🏗️ Core Architecture
- **Result Pattern Error Handling**: Explicit error handling without exceptions
- **Dependency Injection**: Lightweight service container with lifetime management
- **Thread Context**: Cross-cutting concerns and context propagation
- **RAII Resource Management**: Automatic cleanup and lifecycle management

### 📊 Monitoring & Observability
- **Performance Monitoring**: Nanosecond-precision timing and system resource monitoring
- **Distributed Tracing**: W3C Trace Context compliant with hierarchical spans and baggage
- **Health Monitoring**: Liveness, readiness, and startup checks with dependency graphs
- **Adaptive Monitoring**: Dynamic resource-aware monitoring with auto-tuning

### 🔧 Reliability & Safety
- **Circuit Breakers**: Prevent cascading failures with configurable thresholds
- **Retry Policies**: Exponential backoff, linear backoff, and custom strategies  
- **Error Boundaries**: Isolate failures to prevent system-wide outages
- **Resource Management**: Memory quotas, rate limiting, and CPU throttling

### 📈 Performance & Storage
- **Lock-Free Data Structures**: High-performance concurrent collections
- **Stream Processing**: Real-time aggregation with windowing and buffering
- **Multiple Storage Backends**: File (JSON/Binary/CSV), Database (SQLite/PostgreSQL/MySQL), Cloud (S3/GCS/Azure), Memory
- **Compression & Encryption**: Configurable data compression and encryption

### 🌐 Integration & Export  
- **OpenTelemetry Compatible**: Full OTEL resource model and semantic conventions
- **Multiple Export Formats**: Jaeger, Zipkin, Prometheus, StatsD, OTLP (gRPC/HTTP)
- **Storage Flexibility**: 10 different storage backend options
- **Protocol Support**: HTTP, gRPC, Thrift, JSON, Protobuf

## 📖 Documentation

### API Reference
- [**API Reference**](docs/API_REFERENCE.md) - Complete API documentation (784 lines)
- [**Architecture Guide**](docs/ARCHITECTURE_GUIDE.md) - System design and patterns (666 lines)
- [**Performance Tuning**](docs/PERFORMANCE_TUNING.md) - Optimization strategies (482 lines)
- [**Troubleshooting**](docs/TROUBLESHOOTING.md) - Common issues and solutions (679 lines)

### Tutorials & Examples
- [**Tutorial**](examples/TUTORIAL.md) - Comprehensive step-by-step guide (467 lines)
- [**Basic Monitoring**](examples/basic_monitoring_example.cpp) - Getting started
- [**Distributed Tracing**](examples/distributed_tracing_example.cpp) - Cross-service tracing
- [**Health & Reliability**](examples/health_reliability_example.cpp) - Circuit breakers and health checks
- [**Result Pattern**](examples/result_pattern_example.cpp) - Error handling patterns

## 🔥 Usage Examples

### Basic Monitoring

```cpp
#include <monitoring/performance/performance_monitor.h>

// Create performance monitor
performance_monitor monitor("my_app");
monitor.initialize();

// Time an operation
{
    auto timer = monitor.time_operation("database_query");
    // ... perform database query ...
} // Duration automatically recorded

// Get metrics
auto metrics = monitor.collect();
```

### Distributed Tracing

```cpp
#include <monitoring/tracing/distributed_tracer.h>

distributed_tracer tracer;

// Start a trace
auto span = tracer.start_span("process_request", "api_service");
if (span) {
    auto s = span.value();
    s->tags["user.id"] = "12345";
    s->tags["http.method"] = "GET";
    
    // Create child span
    auto child = tracer.start_child_span(*s, "database_query");
    
    // Finish spans
    tracer.finish_span(child.value());
    tracer.finish_span(s);
}
```

### Health Monitoring

```cpp
#include <monitoring/health/health_monitor.h>

health_monitor monitor;

// Register health check
monitor.register_check("database",
    health_check_builder()
        .with_name("db_check")
        .with_type(health_check_type::liveness)
        .with_check([]() {
            return database_is_alive() ? 
                health_check_result::healthy("DB OK") :
                health_check_result::unhealthy("DB Down");
        })
        .build()
);

// Check health
auto results = monitor.check_all();
auto status = monitor.get_overall_status();
```

### Circuit Breaker

```cpp
#include <monitoring/reliability/circuit_breaker.h>

circuit_breaker_config config;
config.failure_threshold = 5;
config.reset_timeout = 30s;

circuit_breaker<std::string> breaker("external_api", config);

// Execute with circuit breaker protection
auto result = breaker.execute(
    []() { return call_external_api(); },
    []() { return result<std::string>::success("cached_fallback"); }
);
```

### Error Handling with Result Pattern

```cpp
#include <monitoring/core/result_types.h>

result<int> parse_config(const std::string& value) {
    try {
        int parsed = std::stoi(value);
        return result<int>::success(parsed);
    } catch (...) {
        return make_error<int>(
            monitoring_error_code::invalid_argument,
            "Cannot parse: " + value
        );
    }
}

// Chain operations
auto result = parse_config("42")
    .and_then([](int value) {
        return value > 0 ? 
            result<int>::success(value * 2) :
            make_error<int>(monitoring_error_code::out_of_range, "Must be positive");
    })
    .map([](int value) { return value + 10; });
```

## 🧪 Testing

The project includes comprehensive testing with **high coverage**:

- **Unit Tests**: Individual component testing
- **Integration Tests**: End-to-end cross-component testing  
- **Stress Tests**: High-load, concurrency, and memory leak detection
- **Performance Tests**: Latency and throughput benchmarking

```bash
# Run all tests with details
./tests/monitoring_system_tests --gtest_output=xml:test_results.xml

# Run stress tests specifically  
./tests/monitoring_system_tests --gtest_filter="StressPerformanceTest.*"

# Run integration tests
./tests/monitoring_system_tests --gtest_filter="*Integration*"
```

## 📊 Performance Characteristics

| Component | Target Performance | Overhead |
|-----------|-------------------|----------|
| Metric Collection | < 1μs per metric | < 1% CPU |
| Span Creation | < 500ns per span | < 0.5% CPU |
| Health Checks | < 10ms per check | < 1% CPU |
| Storage Writes | < 5ms batched | < 2% CPU |
| **Total System** | **-** | **< 5% CPU** |

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Application Layer                        │
├─────────────────────────────────────────────────────────────┤
│                    Monitoring System API                     │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │ Metrics  │  │  Health  │  │ Tracing  │  │ Logging  │   │
│  │Collector │  │ Monitor  │  │  System  │  │  System  │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
├─────────────────────────────────────────────────────────────┤
│                     Core Services Layer                      │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │  Stream  │  │ Storage  │  │Reliability│ │ Adaptive │   │
│  │Processing│  │ Backend  │  │ Features  │ │Optimizer │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
├─────────────────────────────────────────────────────────────┤
│                    Infrastructure Layer                      │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │  Thread  │  │    DI    │  │  Result  │  │  Error   │   │
│  │ Context  │  │Container │  │  Types   │  │ Handling │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## 🔧 Configuration

### Basic Configuration

```cpp
// Performance optimized
monitoring_config config;
config.sampling_rate = 0.1;           // 10% sampling
config.collection_interval = 1s;      // Collect every second
config.max_memory_mb = 50;            // Memory limit
config.enable_compression = true;     // Compress stored data
```

### Advanced Configuration

```cpp
// Health monitoring
health_monitor_config health_config;
health_config.check_interval = 30s;
health_config.enable_auto_recovery = true;

// Circuit breaker  
circuit_breaker_config cb_config;
cb_config.failure_threshold = 5;
cb_config.reset_timeout = 60s;
cb_config.success_threshold = 3;

// Storage backend
storage_config storage_cfg;
storage_cfg.type = storage_backend_type::sqlite;
storage_cfg.path = "/var/lib/monitoring/data.db";
storage_cfg.compression = compression_type::zstd;
```

## 🔌 Integrations

### OpenTelemetry

```cpp
#include <monitoring/adapters/opentelemetry_adapter.h>

opentelemetry_adapter adapter;
adapter.export_traces(spans);
adapter.export_metrics(metrics);
```

### Prometheus

```cpp
#include <monitoring/exporters/prometheus_exporter.h>

prometheus_exporter exporter;
exporter.serve_metrics("/metrics", 9090);
```

### Jaeger

```cpp
#include <monitoring/exporters/trace_exporters.h>

jaeger_exporter exporter("http://localhost:14268/api/traces");
exporter.export_spans(spans);
```

## 📦 Components

### Storage Backends
- **File**: JSON, Binary, CSV formats
- **Database**: SQLite, PostgreSQL, MySQL
- **Cloud**: Amazon S3, Google Cloud Storage, Azure Blob
- **Memory**: In-memory buffer for development/testing

### Export Formats  
- **Tracing**: Jaeger (Thrift/gRPC), Zipkin (JSON/Protobuf), OTLP
- **Metrics**: Prometheus (Text/Protobuf), StatsD, OTLP
- **Protocols**: HTTP, gRPC, UDP

### Reliability Features
- **Circuit Breakers**: Prevent cascading failures
- **Retry Policies**: Multiple backoff strategies
- **Error Boundaries**: Fault isolation
- **Resource Management**: Memory, CPU, rate limiting

## 🛠️ Development

### Project Structure

```
monitoring_system/
├── sources/monitoring/           # Source code
│   ├── core/                    # Core infrastructure  
│   ├── interfaces/              # Abstract interfaces
│   ├── performance/             # Performance monitoring
│   ├── tracing/                 # Distributed tracing
│   ├── health/                  # Health monitoring
│   ├── reliability/             # Reliability features
│   ├── storage/                 # Storage backends
│   ├── stream/                  # Stream processing
│   ├── exporters/               # Export formats
│   └── adapters/                # Integration adapters
├── tests/                       # Comprehensive test suite
├── examples/                    # Working examples
├── docs/                        # Documentation
└── CMakeLists.txt              # Build configuration
```

### Building with Custom Options

```bash
# Build with examples and tests
cmake -DMONITORING_BUILD_EXAMPLES=ON -DMONITORING_BUILD_TESTS=ON ..

# Build with sanitizers
cmake -DENABLE_ASAN=ON -DENABLE_TSAN=ON ..

# Build optimized release
cmake -DCMAKE_BUILD_TYPE=Release ..
```

## 📄 License

This project is available under the MIT License. See the [LICENSE](LICENSE) file for details.

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Run the test suite
6. Submit a pull request

## 📞 Support

- **Documentation**: See [docs/](docs/) directory
- **Examples**: See [examples/](examples/) directory  
- **Issues**: Report bugs via GitHub Issues
- **Discussions**: Use GitHub Discussions for questions

## 🎯 Production Deployment

The monitoring system is **production-ready** with:

- ✅ **Comprehensive Testing**: Unit, integration, stress, and performance tests
- ✅ **Complete Documentation**: API reference, architecture guides, tutorials
- ✅ **Working Examples**: Real-world usage patterns
- ✅ **Performance Optimized**: < 5% CPU overhead, lock-free operations
- ✅ **Industry Standards**: OpenTelemetry compatibility, W3C Trace Context
- ✅ **Reliability Features**: Circuit breakers, retries, error boundaries
- ✅ **Flexible Storage**: 10+ storage backend options
- ✅ **Export Compatibility**: All major observability platforms

### Deployment Checklist

- [ ] Configure appropriate sampling rates for your load
- [ ] Set up storage backend (recommend SQLite for single instance, PostgreSQL for distributed)
- [ ] Configure health checks for your services
- [ ] Set circuit breaker thresholds based on your SLAs
- [ ] Enable compression for storage efficiency
- [ ] Set up export to your observability platform (Jaeger, Prometheus, etc.)
- [ ] Monitor the monitoring system overhead (should be < 5%)

---

**Ready to monitor your applications?** Start with the [Tutorial](examples/TUTORIAL.md) and explore the [examples](examples/) directory! 🚀