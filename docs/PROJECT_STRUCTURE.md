# Monitoring System - Project Structure

**Version**: 1.0
**Last Updated**: 2025-11-15

---

## Table of Contents

- [Overview](#overview)
- [Directory Structure](#directory-structure)
- [Core Modules](#core-modules)
- [File Descriptions](#file-descriptions)
- [Build Artifacts](#build-artifacts)
- [Module Dependencies](#module-dependencies)

---

## Overview

The monitoring system follows a modular, interface-based architecture with clear separation of concerns. This document provides a comprehensive guide to the project structure and file organization.

---

## Directory Structure

```
monitoring_system/
├── 📁 include/kcenon/monitoring/   # Public headers (API surface)
│   ├── 📁 core/                    # Core monitoring components
│   │   ├── performance_monitor.h   # Performance metrics collection
│   │   ├── result_types.h          # Error handling types
│   │   ├── di_container.h          # Dependency injection container
│   │   └── thread_context.h        # Thread-local context tracking
│   ├── 📁 interfaces/              # Abstract interfaces
│   │   ├── monitorable_interface.h # Monitoring abstraction
│   │   ├── storage_interface.h     # Storage backend abstraction
│   │   ├── tracer_interface.h      # Distributed tracing abstraction
│   │   └── health_check_interface.h # Health check abstraction
│   ├── 📁 tracing/                 # Distributed tracing components
│   │   ├── distributed_tracer.h    # Trace management and coordination
│   │   ├── span.h                  # Span operations and lifecycle
│   │   ├── trace_context.h         # Context propagation mechanisms
│   │   └── trace_exporter.h        # Trace export and batching
│   ├── 📁 health/                  # Health monitoring components
│   │   ├── health_monitor.h        # Health validation framework
│   │   ├── health_check.h          # Health check definitions
│   │   ├── circuit_breaker.h       # Circuit breaker pattern
│   │   └── reliability_patterns.h  # Retry and fallback patterns
│   ├── 📁 storage/                 # Storage backend implementations
│   │   ├── memory_storage.h        # In-memory storage backend
│   │   ├── file_storage.h          # File-based persistent storage
│   │   └── time_series_storage.h   # Time-series optimized storage
│   └── 📁 config/                  # Configuration management
│       ├── monitoring_config.h     # Configuration structures
│       └── config_validator.h      # Configuration validation
├── 📁 src/                         # Implementation files
│   ├── 📁 core/                    # Core implementations
│   │   ├── performance_monitor.cpp # Performance monitor implementation
│   │   ├── result_types.cpp        # Result type implementations
│   │   ├── di_container.cpp        # DI container implementation
│   │   └── thread_context.cpp      # Thread context implementation
│   ├── 📁 tracing/                 # Tracing implementations
│   │   ├── distributed_tracer.cpp  # Tracer implementation
│   │   ├── span.cpp                # Span implementation
│   │   ├── trace_context.cpp       # Context implementation
│   │   └── trace_exporter.cpp      # Exporter implementation
│   ├── 📁 health/                  # Health implementations
│   │   ├── health_monitor.cpp      # Health monitor implementation
│   │   ├── health_check.cpp        # Health check implementation
│   │   ├── circuit_breaker.cpp     # Circuit breaker implementation
│   │   └── reliability_patterns.cpp # Reliability pattern implementations
│   ├── 📁 storage/                 # Storage implementations
│   │   ├── memory_storage.cpp      # Memory storage implementation
│   │   ├── file_storage.cpp        # File storage implementation
│   │   └── time_series_storage.cpp # Time-series storage implementation
│   └── 📁 config/                  # Configuration implementations
│       ├── monitoring_config.cpp   # Config structure implementation
│       └── config_validator.cpp    # Validator implementation
├── 📁 examples/                    # Example applications
│   ├── 📁 basic_monitoring_example/   # Basic monitoring usage
│   │   ├── main.cpp                # Entry point
│   │   ├── README.md               # Example documentation
│   │   └── CMakeLists.txt          # Build configuration
│   ├── 📁 distributed_tracing_example/ # Tracing across services
│   │   ├── main.cpp                # Entry point
│   │   ├── README.md               # Example documentation
│   │   └── CMakeLists.txt          # Build configuration
│   ├── 📁 health_reliability_example/ # Health checks and reliability
│   │   ├── main.cpp                # Entry point
│   │   ├── README.md               # Example documentation
│   │   └── CMakeLists.txt          # Build configuration
│   └── 📁 integration_examples/    # Ecosystem integration
│       ├── with_thread_system.cpp  # Thread system integration
│       ├── with_logger_system.cpp  # Logger system integration
│       ├── README.md               # Integration guide
│       └── CMakeLists.txt          # Build configuration
├── 📁 tests/                       # All test suites
│   ├── 📁 unit/                    # Unit tests
│   │   ├── test_result_types.cpp   # Result type tests
│   │   ├── test_di_container.cpp   # DI container tests
│   │   ├── test_performance_monitor.cpp # Performance tests
│   │   ├── test_tracer.cpp         # Tracer tests
│   │   ├── test_health_monitor.cpp # Health monitor tests
│   │   └── test_storage.cpp        # Storage backend tests
│   ├── 📁 integration/             # Integration tests
│   │   ├── test_monitoring_integration.cpp # Full integration
│   │   ├── test_thread_system_integration.cpp # Thread integration
│   │   └── test_logger_integration.cpp # Logger integration
│   ├── 📁 benchmarks/              # Performance benchmarks
│   │   ├── bench_metrics.cpp       # Metrics benchmarks
│   │   ├── bench_tracing.cpp       # Tracing benchmarks
│   │   ├── bench_health.cpp        # Health check benchmarks
│   │   └── bench_storage.cpp       # Storage benchmarks
│   └── CMakeLists.txt              # Test build configuration
├── 📁 docs/                        # Documentation
│   ├── 📁 guides/                  # User guides
│   │   ├── USER_GUIDE.md           # Comprehensive user guide
│   │   ├── INTEGRATION.md          # Integration guide
│   │   ├── BEST_PRACTICES.md       # Best practices
│   │   ├── TROUBLESHOOTING.md      # Troubleshooting guide
│   │   ├── FAQ.md                  # Frequently asked questions
│   │   └── MIGRATION_GUIDE.md      # Migration between versions
│   ├── 📁 advanced/                # Advanced topics
│   │   ├── CUSTOM_STORAGE.md       # Custom storage backends
│   │   ├── CUSTOM_METRICS.md       # Custom metrics
│   │   └── PERFORMANCE_TUNING.md   # Performance optimization
│   ├── 📁 performance/             # Performance documentation
│   │   ├── BASELINE.md             # Performance baselines
│   │   └── BENCHMARKS.md           # Detailed benchmarks
│   ├── 📁 contributing/            # Contribution guidelines
│   │   ├── CONTRIBUTING.md         # How to contribute
│   │   ├── CODE_STYLE.md           # Code style guide
│   │   └── DEVELOPMENT_SETUP.md    # Development environment setup
│   ├── 01-ARCHITECTURE.md          # Architecture overview
│   ├── 02-API_REFERENCE.md         # Complete API reference
│   ├── FEATURES.md                 # Detailed feature documentation
│   ├── BENCHMARKS.md               # Performance benchmarks
│   ├── PROJECT_STRUCTURE.md        # This file
│   ├── PRODUCTION_QUALITY.md       # Production quality metrics
│   ├── CHANGELOG.md                # Version history
│   └── README.md                   # Documentation index
├── 📁 cmake/                       # CMake modules
│   ├── CompilerWarnings.cmake      # Compiler warning flags
│   ├── Sanitizers.cmake            # Sanitizer configuration
│   ├── StaticAnalysis.cmake        # Static analysis tools
│   └── Dependencies.cmake          # Dependency management
├── 📁 .github/                     # GitHub configuration
│   ├── 📁 workflows/               # CI/CD workflows
│   │   ├── ci.yml                  # Main CI pipeline
│   │   ├── coverage.yml            # Code coverage
│   │   ├── static-analysis.yml     # Static analysis
│   │   └── build-doxygen.yaml      # Documentation build
│   └── 📁 ISSUE_TEMPLATE/          # Issue templates
│       ├── bug_report.md           # Bug report template
│       └── feature_request.md      # Feature request template
├── 📄 CMakeLists.txt               # Root build configuration
├── 📄 vcpkg.json                   # Dependency manifest
├── 📄 .clang-format                # Code formatting rules
├── 📄 .clang-tidy                  # Static analysis rules
├── 📄 .gitignore                   # Git ignore patterns
├── 📄 LICENSE                      # BSD 3-Clause license
├── 📄 README.md                    # Main documentation
├── 📄 README_KO.md                 # Korean documentation
└── 📄 BASELINE.md                  # Performance baselines
```

---

## Core Modules

### Core Module (`include/kcenon/monitoring/core/`)

**Purpose**: Fundamental monitoring capabilities and infrastructure

**Key Components**:

| File | Purpose | Key Classes/Functions | Dependencies |
|------|---------|----------------------|--------------|
| `performance_monitor.h` | Performance metrics collection | `performance_monitor`, `metrics_snapshot` | result_types.h |
| `result_types.h` | Error handling types | `result<T>`, `monitoring_error` | None |
| `di_container.h` | Dependency injection | `di_container`, service registration | result_types.h |
| `thread_context.h` | Thread-local context | `thread_context`, context propagation | None |

### Interfaces Module (`include/kcenon/monitoring/interfaces/`)

**Purpose**: Abstract interfaces for extensibility and testability

**Key Interfaces**:

| File | Purpose | Key Methods | Implementations |
|------|---------|-------------|-----------------|
| `monitorable_interface.h` | Monitoring capability | `configure()`, `start()`, `stop()`, `collect_now()` | performance_monitor |
| `storage_interface.h` | Storage backend | `store()`, `retrieve()`, `flush()` | memory_storage, file_storage, time_series_storage |
| `tracer_interface.h` | Distributed tracing | `start_span()`, `finish_span()`, `export_traces()` | distributed_tracer |
| `health_check_interface.h` | Health validation | `check()`, `get_status()` | functional_health_check, custom checks |

### Tracing Module (`include/kcenon/monitoring/tracing/`)

**Purpose**: Distributed request tracing and context propagation

**Key Components**:

| File | Purpose | Key Classes/Functions | Thread-Safe |
|------|---------|----------------------|-------------|
| `distributed_tracer.h` | Trace coordination | `distributed_tracer`, `global_tracer()` | ✅ Yes |
| `span.h` | Span lifecycle | `span`, tag management | ✅ Yes |
| `trace_context.h` | Context propagation | `trace_context`, `get_current_context()` | ✅ Yes (thread-local) |
| `trace_exporter.h` | Trace export | `trace_exporter`, batch processing | ✅ Yes |

### Health Module (`include/kcenon/monitoring/health/`)

**Purpose**: Health monitoring and reliability patterns

**Key Components**:

| File | Purpose | Key Classes/Functions | Use Case |
|------|---------|----------------------|----------|
| `health_monitor.h` | Health validation | `health_monitor`, check registration | Service health |
| `health_check.h` | Health check definitions | `health_check_result`, status types | Custom checks |
| `circuit_breaker.h` | Circuit breaker pattern | `circuit_breaker`, state management | Fault tolerance |
| `reliability_patterns.h` | Retry/fallback | `retry_policy`, `error_boundary` | Resilience |

### Storage Module (`include/kcenon/monitoring/storage/`)

**Purpose**: Metric and trace storage backends

**Key Components**:

| File | Purpose | Performance | Persistence | Best For |
|------|---------|-------------|-------------|----------|
| `memory_storage.h` | In-memory storage | 8.5M ops/sec | No | Real-time, short retention |
| `file_storage.h` | File-based storage | 2.1M ops/sec | Yes | Long retention, auditing |
| `time_series_storage.h` | Time-series optimized | 1.8M ops/sec | Yes | Historical analysis, compression |

### Config Module (`include/kcenon/monitoring/config/`)

**Purpose**: Configuration management and validation

**Key Components**:

| File | Purpose | Key Structures | Validation |
|------|---------|----------------|------------|
| `monitoring_config.h` | Config structures | `monitoring_config`, `storage_config` | Required |
| `config_validator.h` | Config validation | `validate_config()`, error checking | Comprehensive |

---

## File Descriptions

### Core Implementation Files

#### `src/core/performance_monitor.cpp`

**Purpose**: Real-time performance metrics collection

**Key Features**:
- Atomic counter operations (10M+ ops/sec)
- Gauge tracking
- Histogram recording with configurable buckets
- Timer utilities with RAII
- Thread-safe metric collection

**Public API**:
```cpp
class performance_monitor {
    auto enable_collection(bool enabled) -> void;
    auto collect() -> result<metrics_snapshot>;
    auto increment_counter(const std::string& name) -> void;
    auto set_gauge(const std::string& name, double value) -> void;
    auto record_histogram(const std::string& name, double value) -> void;
    auto start_timer(const std::string& name) -> scoped_timer;
};
```

#### `src/core/result_types.cpp`

**Purpose**: Error handling infrastructure

**Key Features**:
- Type-safe error handling without exceptions
- Composable operations (map, and_then)
- Rich error context
- Integration with monitoring error codes

**Public API**:
```cpp
template<typename T>
class result {
    auto has_value() const -> bool;
    auto value() const -> const T&;
    auto get_error() const -> const monitoring_error&;
    template<typename F> auto map(F&& func) -> result<...>;
    template<typename F> auto and_then(F&& func) -> ...;
};
```

#### `src/core/di_container.cpp`

**Purpose**: Dependency injection and lifecycle management

**Key Features**:
- Singleton registration
- Transient registration
- Factory registration
- Automatic dependency resolution
- Thread-safe service access

**Public API**:
```cpp
class di_container {
    template<typename Interface, typename Implementation>
    auto register_singleton() -> result_void;

    template<typename Interface>
    auto resolve() -> result<std::shared_ptr<Interface>>;
};
```

### Tracing Implementation Files

#### `src/tracing/distributed_tracer.cpp`

**Purpose**: Distributed trace management

**Key Features**:
- Span lifecycle management (2.5M spans/sec)
- Context propagation (<50ns overhead)
- Trace export and batching
- Thread-safe operations

**Implementation Details**:
- Lock-free span creation using atomic operations
- Thread-local context storage
- Batch export optimization (optimal batch size: 100-500)

#### `src/tracing/span.cpp`

**Purpose**: Individual span operations

**Key Features**:
- Tag management
- Parent-child relationships
- Timing information
- Metadata attachment

### Health Implementation Files

#### `src/health/health_monitor.cpp`

**Purpose**: Health validation framework

**Key Features**:
- Health check registration
- Periodic health validation (configurable intervals)
- Dependency health tracking
- Aggregate health status

**Implementation Details**:
- Check execution: 500K checks/sec
- Automatic retry on transient failures
- Health status caching for performance

#### `src/health/circuit_breaker.cpp`

**Purpose**: Circuit breaker pattern implementation

**Key Features**:
- State management (Closed, Open, Half-Open)
- Failure threshold tracking
- Automatic recovery testing
- Statistics collection

**Performance**:
- Closed state: 12M ops/sec
- Open state: 25M ops/sec (fast fail)
- Half-open state: 8M ops/sec

### Storage Implementation Files

#### `src/storage/memory_storage.cpp`

**Purpose**: In-memory storage backend

**Implementation Details**:
- Hash map for fast lookups
- LRU eviction for memory management
- Configurable retention
- Lock-free reads where possible

**Performance**: 8.5M write ops/sec, 12M read ops/sec

#### `src/storage/time_series_storage.cpp`

**Purpose**: Time-series optimized storage

**Implementation Details**:
- Delta encoding for compression (up to 90%)
- Downsampling for historical data
- Efficient range queries
- Retention policy enforcement

**Performance**: 1.8M write ops/sec with compression

---

## Build Artifacts

### Build Directory Structure

```
build/
├── 📁 lib/                         # Libraries
│   └── libmonitoring_system.a      # Static library
├── 📁 bin/                         # Executables
│   ├── basic_monitoring_example    # Example binary
│   ├── distributed_tracing_example # Example binary
│   └── health_reliability_example  # Example binary
├── 📁 tests/                       # Test executables
│   ├── monitoring_system_tests     # Unit tests
│   ├── integration_tests           # Integration tests
│   └── benchmarks                  # Benchmark suite
└── 📁 docs/                        # Generated documentation
    └── 📁 html/                    # Doxygen HTML output
        └── index.html              # Documentation entry point
```

### CMake Targets

| Target | Type | Output | Purpose |
|--------|------|--------|---------|
| `monitoring_system` | Library | `libmonitoring_system.a` | Main library |
| `monitoring_system_tests` | Executable | `monitoring_system_tests` | Unit tests |
| `integration_tests` | Executable | `integration_tests` | Integration tests |
| `benchmarks` | Executable | `benchmarks` | Performance tests |
| `basic_monitoring_example` | Executable | `basic_monitoring_example` | Example app |
| `docs` | Custom | `docs/html/` | Documentation |

---

## Module Dependencies

### Internal Dependencies

```
┌─────────────────────────────────────────────────────────────┐
│                     monitoring_system                       │
└─────────────────────────────────────────────────────────────┘
                           │
           ┌───────────────┼───────────────┐
           │               │               │
           ▼               ▼               ▼
    ┌──────────┐    ┌──────────┐    ┌──────────┐
    │   core   │    │ tracing  │    │  health  │
    └────┬─────┘    └────┬─────┘    └────┬─────┘
         │               │               │
         │          ┌────┴────┐          │
         │          │         │          │
         ▼          ▼         ▼          ▼
    ┌────────────────────────────────────────┐
    │           interfaces                   │
    └────────────────────────────────────────┘
                     │
                     ▼
            ┌─────────────────┐
            │    storage      │
            └─────────────────┘
```

### Module Dependency Matrix

| Module | Depends On | Used By |
|--------|-----------|---------|
| **config** | None | core, tracing, health, storage |
| **interfaces** | config | core, tracing, health, storage |
| **core** | interfaces | tracing, health |
| **tracing** | core, interfaces | health |
| **health** | core, interfaces | N/A |
| **storage** | interfaces | core, tracing, health |

### External Dependencies

| Dependency | Version | Purpose | Required |
|------------|---------|---------|----------|
| **common_system** | Latest | Core interfaces (IMonitor, ILogger, Result<T>) | Yes |
| **thread_system** | Latest | Threading primitives, monitoring_interface | Yes |
| **logger_system** | Latest | Logging capabilities | No (optional) |
| **Google Test** | 1.12+ | Unit testing framework | No (test only) |
| **Google Benchmark** | 1.7+ | Performance benchmarking | No (benchmark only) |
| **Catch2** | 3.0+ | Testing framework (migrating to) | No (test only) |

### Compilation Order

1. **config** - No dependencies
2. **interfaces** - Depends on config
3. **core** - Depends on interfaces
4. **storage** - Depends on interfaces
5. **tracing** - Depends on core, interfaces
6. **health** - Depends on core, interfaces

**Total Build Time**: ~12 seconds (Release mode, Apple M1)

---

## Test Organization

### Unit Tests (`tests/unit/`)

| Test File | Tests | Coverage | Purpose |
|-----------|-------|----------|---------|
| `test_result_types.cpp` | 13 | Result<T> pattern | Error handling validation |
| `test_di_container.cpp` | 9 | DI container | Service registration/resolution |
| `test_performance_monitor.cpp` | 8 | Performance monitor | Metrics collection |
| `test_tracer.cpp` | 5 | Distributed tracer | Span lifecycle |
| `test_health_monitor.cpp` | 4 | Health monitor | Health checks |
| `test_storage.cpp` | 6 | Storage backends | Data persistence |

**Total**: 37 tests, 100% pass rate

### Integration Tests (`tests/integration/`)

| Test File | Tests | Purpose |
|-----------|-------|---------|
| `test_monitoring_integration.cpp` | Full stack | End-to-end monitoring |
| `test_thread_system_integration.cpp` | Thread integration | Thread system compatibility |
| `test_logger_integration.cpp` | Logger integration | Logging integration |

### Benchmark Tests (`tests/benchmarks/`)

| Benchmark File | Benchmarks | Purpose |
|----------------|------------|---------|
| `bench_metrics.cpp` | Counter, gauge, histogram | Metrics performance |
| `bench_tracing.cpp` | Span creation, export | Tracing performance |
| `bench_health.cpp` | Health checks, circuit breaker | Health monitoring performance |
| `bench_storage.cpp` | Storage backends | Storage performance |

---

## See Also

- [Architecture Guide](01-ARCHITECTURE.md) - System design and patterns
- [API Reference](02-API_REFERENCE.md) - Complete API documentation
- [Features](FEATURES.md) - Detailed feature documentation
- [Benchmarks](BENCHMARKS.md) - Performance metrics
- [User Guide](guides/USER_GUIDE.md) - Usage examples
