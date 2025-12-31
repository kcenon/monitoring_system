Architecture Overview
=====================

> **Language:** **English** | [한국어](ARCHITECTURE_KO.md)

Purpose
- monitoring_system provides metrics collection, storage, analysis, health checks, and event‑driven observability for C++20 services.
- It is interface‑first and usable standalone or integrated with the thread_system and logger_system projects.

Key Modules
- Core
  - result_types, error_codes: Result pattern and error taxonomy
  - event_bus, event_types: In‑process pub/sub for metrics and system events
- Interfaces
  - monitoring_interface, metric_collector_interface, storage_backend, metrics_analyzer
- Platform Abstraction Layer (Issue #291)
  - metrics_provider: Abstract interface for platform-specific metrics collection
  - linux_metrics_provider: Linux implementation using /proc and /sys filesystems
  - macos_metrics_provider: macOS implementation using IOKit and system APIs
  - windows_metrics_provider: Windows implementation using WMI and system APIs
  - Provides: battery, temperature, uptime, context switches, FD stats, inode stats,
    TCP states, socket buffers, interrupts, power info, GPU info, security info
- Utilities
  - buffer_manager, ring_buffer, time_series, aggregation_processor
- Reliability & Tracing
  - retry_policy, circuit_breaker, fault_tolerance_manager, distributed_tracer
- Adapters (optional)
  - thread_system_adapter, logger_system_adapter (graceful fallback when not present)

Integration Topology
```
thread_system ──(metrics)──► monitoring_system ◄──(metrics)── logger_system
        │                                      │
        └──── application components ──────────┘
```

Data Flow
1) Collectors gather metrics (pull or push) and emit metric_collection_event via event_bus.
2) Storage backends persist snapshots; analyzers compute trends and alerts.
3) Health checks summarize component status into health_check_event.

Thread System Integration
- When thread_system is available, thread_system_adapter discovers a monitorable provider via service_container and converts selected thread pool metrics into monitoring metrics.
- Without thread_system, the adapter returns empty sets and remains no‑op, keeping builds green.

Design Principles
- Interface segregation: decouple producers, transport, and sinks.
- Back‑pressure ready: buffering utilities and bounded queues.
- Result pattern: explicit error reporting (no exceptions across module boundaries).

Build & Options
- C++20, CMake. Optional flags: USE_THREAD_SYSTEM, BUILD_WITH_LOGGER_SYSTEM.
- On macOS/Linux/Windows with vcpkg (optional) for third‑party libraries.


---

*Last Updated: 2025-12-31*
