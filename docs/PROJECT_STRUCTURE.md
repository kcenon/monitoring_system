# Monitoring System - Project Structure

**Version**: 0.3.0
**Last Updated**: 2026-01-22

---

## Table of Contents

- [Overview](#overview)
- [Directory Structure](#directory-structure)
- [Core Modules](#core-modules)
- [Collector Modules](#collector-modules)
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
├── include/kcenon/monitoring/   # Public headers (API surface)
│   ├── adapters/                # System integration adapters
│   │   ├── common_monitor_adapter.h
│   │   ├── common_system_adapter.h
│   │   ├── logger_system_adapter.h
│   │   ├── monitor_adapter.h
│   │   ├── performance_monitor_adapter.h
│   │   └── thread_system_adapter.h
│   ├── adaptive/                # Adaptive monitoring
│   │   └── adaptive_monitor.h
│   ├── collectors/              # Metric collectors (core + optional plugins)
│   │   ├── collector_base.h          # CRTP base class for collectors
│   │   │
│   │   │   # Core Collectors (6) - Always included
│   │   ├── system_resource_collector.h # Unified CPU, memory, disk metrics
│   │   ├── network_metrics_collector.h # Socket buffer + TCP state (consolidated)
│   │   ├── process_metrics_collector.h # FD, inode, context switch (consolidated)
│   │   ├── platform_metrics_collector.h # Linux/macOS/Windows via Strategy pattern
│   │   ├── thread_system_collector.h # Thread pool metrics
│   │   ├── logger_system_collector.h # Logger system integration
│   │   │
│   │   │   # Utility Collectors
│   │   ├── plugin_metric_collector.h # Plugin system interface
│   │   ├── uptime_collector.h        # System uptime
│   │   ├── vm_collector.h            # Virtualization metrics
│   │   ├── interrupt_collector.h     # Interrupt statistics
│   │   ├── security_collector.h      # Security events
│   │   │
│   │   │   # Hardware Collectors (optional plugin)
│   │   ├── battery_collector.h       # Battery status (plugin)
│   │   ├── power_collector.h         # Power consumption (plugin)
│   │   ├── temperature_collector.h   # Hardware temperature (plugin)
│   │   ├── gpu_collector.h           # GPU metrics (plugin)
│   │   │
│   │   │   # Container Collectors (optional plugin)
│   │   ├── container_collector.h     # Docker/container metrics (plugin)
│   │   └── smart_collector.h         # SMART disk health (plugin)
│   │
│   ├── plugins/                 # Optional plugin system (Issue #389)
│   │   ├── hardware/            # Hardware monitoring plugin
│   │   │   └── hardware_plugin.h     # Battery, power, temp, GPU
│   │   └── container/           # Container monitoring plugin
│   │       └── container_plugin.h    # Docker, Kubernetes, cgroups
│   ├── concepts/                # C++20 Concepts
│   │   └── monitoring_concepts.h     # Type constraints
│   ├── context/                 # Context management
│   │   └── thread_context.h          # Thread-local context
│   ├── core/                    # Core monitoring components
│   │   ├── central_collector.h       # Central metric collection
│   │   ├── error_codes.h             # Error code definitions
│   │   ├── event_bus.h               # Event publishing
│   │   ├── event_types.h             # Event type definitions
│   │   ├── performance_monitor.h     # Performance metrics
│   │   ├── performance_types.h       # Performance type definitions
│   │   ├── result_types.h            # Result<T> pattern
│   │   ├── safe_event_dispatcher.h   # Thread-safe events
│   │   └── thread_local_buffer.h     # Thread-local buffering
│   ├── di/                      # Dependency injection
│   │   └── service_registration.h
│   ├── exporters/               # Data exporters
│   │   ├── http_transport.h          # HTTP transport
│   │   ├── metric_exporters.h        # Metric export formats
│   │   ├── opentelemetry_adapter.h   # OpenTelemetry integration
│   │   └── trace_exporters.h         # Trace exporters
│   ├── health/                  # Health monitoring
│   │   └── health_monitor.h          # Health check framework
│   ├── interfaces/              # Abstract interfaces
│   │   ├── event_bus_interface.h     # Event bus abstraction
│   │   ├── metric_collector_interface.h # Collector abstraction
│   │   ├── metric_types_adapter.h    # Metric type adapters
│   │   ├── monitorable_interface.h   # Monitoring abstraction
│   │   ├── monitoring_core.h         # Core monitoring interface
│   │   └── observer_interface.h      # Observer pattern
│   ├── reliability/             # Reliability patterns
│   │   ├── circuit_breaker.h         # Circuit breaker pattern
│   │   ├── error_boundary.h          # Error isolation
│   │   ├── fault_tolerance_manager.h # Fault tolerance
│   │   └── retry_policy.h            # Retry mechanisms
│   ├── storage/                 # Storage backends
│   │   └── storage_backends.h        # Storage interface
│   ├── tracing/                 # Distributed tracing
│   │   ├── distributed_tracer.h      # Trace management
│   │   └── trace_context.h           # Context propagation
│   ├── utils/                   # Utility components
│   │   ├── metric_types.h            # Metric type definitions
│   │   ├── time_series.h             # Time series utilities
│   │   └── time_series_buffer.h      # Time series buffer
│   ├── compatibility.h          # Backward compatibility
│   └── forward.h                # Forward declarations
├── src/                         # Implementation files
│   ├── collectors/              # Core collector implementations
│   │   ├── system_resource_collector.cpp  # Unified system metrics
│   │   └── vm_collector.cpp               # Virtualization metrics
│   ├── context/                 # Context implementations
│   │   └── thread_context.cpp
│   ├── core/                    # Core implementations
│   │   ├── central_collector.cpp
│   │   ├── performance_monitor.cpp
│   │   └── thread_local_buffer.cpp
│   ├── impl/                    # Feature implementations
│   │   ├── adaptive_monitor.cpp
│   │   ├── battery_collector.cpp
│   │   ├── container_collector.cpp
│   │   ├── context_switch_collector.cpp
│   │   ├── inode_collector.cpp
│   │   ├── interrupt_collector.cpp
│   │   ├── socket_buffer_collector.cpp
│   │   ├── tcp_state_collector.cpp
│   │   ├── tracing/
│   │   │   └── distributed_tracer.cpp
│   │   └── uptime_collector.cpp
│   ├── platform/                # Platform-specific code
│   │   ├── cgroup_metrics.cpp        # cgroups support
│   │   ├── docker_metrics.cpp        # Docker integration
│   │   ├── fd_collector.cpp
│   │   ├── gpu_collector.cpp
│   │   ├── linux_*.cpp               # Linux implementations
│   │   ├── macos_*.cpp               # macOS implementations
│   │   ├── power_collector.cpp
│   │   ├── security_collector.cpp
│   │   ├── smart_metrics.cpp
│   │   ├── temperature_collector.cpp
│   │   └── windows_*.cpp             # Windows implementations
│   ├── plugins/                 # Optional plugin implementations
│   │   ├── hardware/            # Hardware plugin (battery, power, temp, GPU)
│   │   │   └── hardware_plugin.cpp
│   │   └── container/           # Container plugin (Docker, K8s)
│   │       └── container_plugin.cpp
│   └── utils/                   # Utility implementations (@internal)
│       ├── buffer_manager.h          # Buffer lifecycle management
│       ├── buffering_strategy.h      # Configurable buffering strategies
│       ├── metric_storage.h          # Metric persistence
│       ├── ring_buffer.h             # Lock-free ring buffer
│       └── time_series.h             # Time-series data storage
├── tests/                       # Test suites (48 test files)
│   ├── test_adapter_functionality.cpp
│   ├── test_adaptive_monitoring.cpp
│   ├── test_battery_collector.cpp
│   ├── test_buffering_strategies.cpp
│   ├── test_container_collector.cpp
│   ├── test_context_switch_collector.cpp
│   ├── test_cross_system_integration.cpp
│   ├── test_data_consistency.cpp
│   ├── test_di_container.cpp
│   ├── test_distributed_tracing.cpp
│   ├── test_error_boundaries.cpp
│   ├── test_event_bus.cpp
│   ├── test_fault_tolerance.cpp
│   ├── test_fd_collector.cpp
│   ├── test_gpu_collector.cpp
│   ├── test_health_monitoring.cpp
│   ├── test_inode_collector.cpp
│   ├── test_integration_e2e.cpp
│   ├── test_interfaces_compile.cpp
│   ├── test_interrupt_collector.cpp
│   ├── test_lock_free_collector.cpp
│   ├── test_metric_exporters.cpp
│   ├── test_metric_storage.cpp
│   ├── test_monitorable_interface.cpp
│   ├── test_opentelemetry_adapter.cpp
│   ├── test_optimization.cpp
│   ├── test_performance_monitoring.cpp
│   ├── test_power_collector.cpp
│   ├── test_resource_management.cpp
│   ├── test_result_types.cpp
│   ├── test_security_collector.cpp
│   ├── test_service_registration.cpp
│   ├── test_smart_collector.cpp
│   ├── test_socket_buffer_collector.cpp
│   ├── test_storage_backends.cpp
│   ├── test_stream_aggregation.cpp
│   ├── test_stress_performance.cpp
│   ├── test_system_resource_collector.cpp
│   ├── test_tcp_state_collector.cpp
│   ├── test_temperature_collector.cpp
│   ├── test_thread_context.cpp
│   ├── test_thread_context_simple.cpp
│   ├── test_time_series_buffer.cpp
│   ├── test_timer_metrics.cpp
│   ├── test_trace_exporters.cpp
│   ├── test_uptime_collector.cpp
│   ├── test_vm_collector.cpp
│   └── thread_safety_tests.cpp
├── examples/                    # Example applications
│   ├── basic_monitoring_example.cpp
│   ├── bidirectional_di_example.cpp
│   ├── custom_metric_types_example.cpp
│   ├── distributed_tracing_example.cpp
│   ├── event_bus_example.cpp
│   ├── facade_adapter_poc.cpp
│   ├── health_reliability_example.cpp
│   ├── logger_di_integration_example.cpp
│   ├── monitor_factory_pattern_example.cpp
│   ├── plugin_collector_example.cpp
│   ├── result_pattern_example.cpp
│   ├── storage_example.cpp
│   └── CMakeLists.txt
├── benchmarks/                  # Performance benchmarks
│   ├── adaptive_monitor_bench.cpp
│   ├── collector_overhead_bench.cpp
│   ├── event_bus_bench.cpp
│   ├── main_bench.cpp
│   ├── metric_collection_bench.cpp
│   ├── BASELINE.md
│   ├── CMakeLists.txt
│   └── README.md
├── integration_tests/           # Integration tests
│   └── README.md
├── docs/                        # Documentation
│   ├── advanced/                # Advanced topics
│   ├── contributing/            # Contribution guidelines
│   ├── guides/                  # User guides
│   ├── integration/             # Integration guides
│   ├── performance/             # Performance documentation
│   ├── API_REFERENCE.md         # API documentation
│   ├── API_REFERENCE.kr.md      # API documentation (Korean)
│   ├── ARCHITECTURE.md          # Architecture overview
│   ├── ARCHITECTURE.kr.md
│   ├── BENCHMARKS.md            # Benchmark results
│   ├── BENCHMARKS.kr.md
│   ├── CHANGELOG.md             # Version history
│   ├── CHANGELOG.kr.md
│   ├── FEATURES.md              # Feature documentation
│   ├── FEATURES.kr.md
│   ├── KNOWN_ISSUES.md          # Known issues
│   ├── PRODUCTION_QUALITY.md    # Production quality metrics
│   ├── PRODUCTION_QUALITY.kr.md
│   ├── PROJECT_STRUCTURE.md     # This file
│   ├── PROJECT_STRUCTURE.kr.md
│   └── README.md                # Documentation index
├── cmake/                       # CMake modules
│   ├── monitoring_system-config.cmake.in
│   ├── MonitoringCompatibility.cmake
│   └── MonitoringLegacyOptions.cmake
├── .github/                     # GitHub configuration
│   ├── workflows/               # CI/CD workflows
│   └── ISSUE_TEMPLATE/          # Issue templates
├── CMakeLists.txt               # Root build configuration
├── vcpkg.json                   # Dependency manifest
├── .clang-format                # Code formatting rules
├── .clang-tidy                  # Static analysis rules
├── .gitignore                   # Git ignore patterns
├── LICENSE                      # BSD 3-Clause license
├── README.md                    # Main documentation
└── README.kr.md                 # Korean documentation
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
| `central_collector.h` | Central metric collection | `central_collector` | interfaces |
| `thread_local_buffer.h` | Thread-local buffering | `thread_local_buffer` | None |
| `event_bus.h` | Event publishing system | `event_bus` | event_types.h |

### Interfaces Module (`include/kcenon/monitoring/interfaces/`)

**Purpose**: Abstract interfaces for extensibility and testability

**Key Interfaces**:

| File | Purpose | Key Methods | Implementations |
|------|---------|-------------|-----------------|
| `monitorable_interface.h` | Monitoring capability | `configure()`, `start()`, `stop()`, `collect_now()` | performance_monitor |
| `metric_collector_interface.h` | Collector abstraction | `collect()`, `get_name()`, `is_healthy()` | All collectors |
| `event_bus_interface.h` | Event publishing | `publish()`, `subscribe()` | event_bus |
| `observer_interface.h` | Observer pattern | `on_update()` | Various observers |

### Concepts Module (`include/kcenon/monitoring/concepts/`)

**Purpose**: C++20 Concepts for compile-time type validation

**Key Concepts**:

| Concept | Purpose | Requirements |
|---------|---------|--------------|
| `EventType` | Event type constraint | Class type, copy-constructible |
| `EventHandler` | Event handler constraint | Invocable with const E&, returns void |
| `EventFilter` | Event filter constraint | Invocable with const E&, returns bool |
| `Validatable` | Configuration validation | Has `validate()` method |
| `MetricSourceLike` | Metric source | Has `get_current_metrics()`, `get_source_name()`, `is_healthy()` |
| `MetricCollectorLike` | Metric collector | Has `collect_metrics()`, `is_collecting()`, `get_metric_types()` |

---

## Collector Modules

> **Refactored in Issue #389**: Collector count reduced from 20+ to 6 core collectors plus optional plugins.

### Core Collectors (Always Included)

**Purpose**: Essential metric collection for all deployments

| Collector | Purpose | Consolidated From | Platform Support |
|-----------|---------|-------------------|------------------|
| `system_resource_collector` | CPU, memory, disk metrics | cpu_collector, memory_collector | Linux, macOS, Windows |
| `network_metrics_collector` | Network socket & TCP states | socket_buffer_collector, tcp_state_collector | Linux, macOS |
| `process_metrics_collector` | FD, inode, context switches | fd_collector, inode_collector, context_switch_collector | Linux, macOS |
| `platform_metrics_collector` | Platform-specific metrics | linux_metrics, macos_metrics, windows_metrics | All (Strategy pattern) |
| `thread_system_collector` | Thread pool metrics | - | All |
| `logger_system_collector` | Logger system integration | - | All |

### Utility Collectors

| Collector | Purpose | Platform Support |
|-----------|---------|------------------|
| `uptime_collector` | System uptime | Linux, macOS, Windows |
| `vm_collector` | Virtualization metrics | Linux, macOS |
| `interrupt_collector` | Interrupt statistics | Linux, macOS |
| `security_collector` | Security event monitoring | Linux |

### Optional Plugins (`include/kcenon/monitoring/plugins/`)

**Purpose**: Optional collectors for specialized environments

#### Hardware Plugin (`-DMONITORING_BUILD_HARDWARE_PLUGIN=ON`)

| Collector | Purpose | Platform Support |
|-----------|---------|------------------|
| `battery_collector` | Battery status monitoring | Linux, macOS, Windows |
| `power_collector` | Power consumption (RAPL) | Linux, macOS |
| `temperature_collector` | Hardware temperature | Linux, macOS |
| `gpu_collector` | GPU metrics (NVIDIA, AMD, Intel, Apple) | Linux, macOS |

#### Container Plugin (`-DMONITORING_BUILD_CONTAINER_PLUGIN=ON`)

| Collector | Purpose | Platform Support |
|-----------|---------|------------------|
| `container_collector` | Docker/container metrics | Linux (cgroups v1/v2) |
| `smart_collector` | SMART disk health | Linux, macOS |

---

## Build Artifacts

### Build Directory Structure

```
build/
├── lib/                         # Libraries
│   ├── libmonitoring_system.a   # Core static library
│   ├── libmonitoring_hardware_plugin.a  # Optional: Hardware plugin
│   └── libmonitoring_container_plugin.a # Optional: Container plugin
├── bin/                         # Executables
│   ├── basic_monitoring_example
│   ├── distributed_tracing_example
│   └── health_reliability_example
├── tests/                       # Test executables
│   ├── monitoring_system_tests  # All tests
│   └── monitoring_container_plugin_test  # Container plugin tests
├── benchmarks/                  # Benchmark executables
│   └── monitoring_benchmarks
└── docs/                        # Generated documentation
    └── html/                    # Doxygen HTML output
```

### CMake Targets

| Target | Type | Output | Purpose |
|--------|------|--------|---------|
| `monitoring_system` | Library | `libmonitoring_system.a` | Core library |
| `monitoring_hardware_plugin` | Library | `libmonitoring_hardware_plugin.a` | Optional hardware plugin |
| `monitoring_container_plugin` | Library | `libmonitoring_container_plugin.a` | Optional container plugin |
| `monitoring_system_tests` | Executable | `monitoring_system_tests` | Unit tests |
| `monitoring_benchmarks` | Executable | `monitoring_benchmarks` | Performance tests |
| `*_example` | Executable | Example binaries | Example apps |

### CMake Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `MONITORING_BUILD_HARDWARE_PLUGIN` | OFF | Build hardware monitoring plugin (battery, power, temp, GPU) |
| `MONITORING_BUILD_CONTAINER_PLUGIN` | OFF | Build container monitoring plugin (Docker, K8s, cgroups) |

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
    │   core   │    │collectors│    │  health  │
    └────┬─────┘    └────┬─────┘    └────┬─────┘
         │               │               │
         │          ┌────┴────┐          │
         │          │         │          │
         ▼          ▼         ▼          ▼
    ┌────────────────────────────────────────┐
    │           interfaces/concepts          │
    └────────────────────────────────────────┘
                     │
                     ▼
            ┌─────────────────┐
            │   adapters      │
            └─────────────────┘
```

### External Dependencies

| Dependency | Version | Purpose | Required |
|------------|---------|---------|----------|
| **common_system** | Latest | Core interfaces (IMonitor, ILogger, Result<T>) | Yes |
| **thread_system** | Latest | Threading primitives | Yes |
| **logger_system** | Latest | Logging capabilities | No (optional) |
| **Google Test** | 1.12+ | Unit testing framework | No (test only) |
| **Google Benchmark** | 1.7+ | Performance benchmarking | No (benchmark only) |

---

## Test Organization

### Unit Tests (`tests/`)

| Category | Test Files | Purpose |
|----------|------------|---------|
| Core | `test_result_types`, `test_di_container`, `test_event_bus` | Core functionality |
| Collectors | `test_*_collector` (15 files) | Collector validation |
| Integration | `test_cross_system_integration`, `test_integration_e2e` | System integration |
| Reliability | `test_fault_tolerance`, `test_error_boundaries` | Reliability patterns |
| Performance | `test_stress_performance`, `test_optimization` | Performance validation |
| Thread Safety | `thread_safety_tests` | Concurrency testing |

**Total**: 48 test files

---

## See Also

- [Architecture Guide](ARCHITECTURE.md) - System design and patterns
- [API Reference](API_REFERENCE.md) - Complete API documentation
- [Features](FEATURES.md) - Detailed feature documentation
- [Benchmarks](BENCHMARKS.md) - Performance metrics
