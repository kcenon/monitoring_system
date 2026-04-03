# monitoring_system

## Overview

Modern C++20 observability platform providing metrics collection, distributed tracing,
health monitoring, alerting, and reliability patterns (circuit breakers, error boundaries).
Interface-driven design with runtime DI and clean separation from ecosystem dependencies.

## Architecture

```
include/kcenon/monitoring/
  core/          - performance_monitor, central_collector, event_bus, error_codes
  interfaces/    - Pure virtual: metric_collector, metric_source, observable, monitorable
  collectors/    - ~16 collectors (system, process, network, battery, GPU, etc.)
  factory/       - metric_factory (singleton), builtin_collectors, collector_adapters
  tracing/       - distributed_tracer, trace_context (W3C-style)
  context/       - Thread-local context propagation (<50ns)
  alert/         - Alert types, triggers, pipeline, notifiers, manager
  health/        - Health monitor, dependency graph, composite health checks
  reliability/   - circuit_breaker, error_boundary, fault_tolerance, retry_policy
  exporters/     - OTLP, Jaeger, Zipkin (HTTP/gRPC/UDP transports)
  plugins/       - Plugin API, collector_plugin, plugin_loader
  storage/       - Storage backends (memory, file, time-series)
  optimization/  - Lock-free queue, memory pool, SIMD aggregator
  platform/      - metrics_provider (Linux/macOS/Windows Strategy pattern)
  concepts/      - C++20 concepts (Validatable, MetricSourceLike, MetricCollectorLike)
```

Key abstractions:
- `interface_metric_collector` / `interface_metric_source` — Pure virtual metric interfaces
- `metric_factory` — Thread-safe singleton with `REGISTER_COLLECTOR(Type)` macro
- `performance_monitor` — Core monitoring implementing `common_system::IMonitor`
- `distributed_tracer` — W3C-style tracing with span hierarchy and export
- `central_collector` — Centralized metric aggregation
- Platform metrics via Strategy pattern (linux/macos/windows/null providers)

## Build & Test

```bash
# Default (requires sibling checkouts of common_system, thread_system)
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

# With tests
cmake --preset debug && cmake --build --preset debug
./build/tests/monitoring_system_tests
```

Key CMake options:
- `MONITORING_BUILD_TESTS` (ON) — Google Test suite
- `MONITORING_BUILD_EXAMPLES` (OFF) — Example programs
- `MONITORING_BUILD_BENCHMARKS` (OFF) — Performance benchmarks
- `MONITORING_WITH_NETWORK_SYSTEM` (OFF) — HTTP transport for exporters
- `MONITORING_WITH_GRPC` (OFF) — gRPC transport for OTLP
- `MONITORING_ENABLE_MODULES` (OFF) — C++20 modules (CMake 3.28+)

Presets: `default`, `debug`, `release`, `full`, `asan`, `tsan`, `ubsan`, `ci`, `vcpkg`

CI: Multi-platform (Ubuntu GCC/Clang, macOS, Windows MSVC), coverage, static analysis,
sanitizers, benchmarks, integration tests, CVE scan, SBOM.

## Key Patterns

- **Collector factory** — `metric_factory::instance()` with `register_collector<T>(name)` and
  `create(name, config)` returning `unique_ptr<collector_interface>`
- **Metrics pipeline** — Observer pattern via `interface_observable`; `central_collector`
  aggregates multiple `interface_metric_source` instances
- **Distributed tracing** — `trace_span` with trace_id/span_id/parent, thread-local context
  propagation (<50ns), export to Jaeger/Zipkin/OTLP
- **Reliability** — `circuit_breaker`, `error_boundary`, `retry_policy`, `graceful_degradation`
- **Plugin architecture** — `collector_plugin`, `plugin_loader`, `collector_registry`
- **SIMD aggregation** — AVX2 (x86_64) / NEON (ARM64) for metric aggregation
- **Result\<T\>** — Error handling from common_system throughout

## Ecosystem Position

**Tier 3** — Observability layer.

```
common_system    (Tier 0) [required] — IMonitor, ILogger, Result<T>
thread_system    (Tier 1) [required] — Thread pool, async operations
logger_system    (Tier 2) [optional] — Logging via runtime DI
network_system   (Tier 4) [optional] — HTTP transport for exporters
```

## Dependencies

**Required ecosystem**: kcenon-common-system, kcenon-thread-system
**Optional ecosystem**: kcenon-logger-system, kcenon-network-system
**Optional external**: gRPC 1.60.0 + protobuf 4.25.1 (OTLP gRPC transport)
**Dev/test**: Google Test 1.17.0, Google Benchmark 1.9.5
**System**: IOKit + CoreFoundation (macOS hardware monitoring)

## Known Constraints

- C++20 required; GCC 13+, Clang 17+, MSVC 2022+, Apple Clang 14+
- Build requires sibling repos: common_system, thread_system must be cloned alongside
- Jaeger/Zipkin protobuf serialization still stub implementations
- Some tests have platform-specific timing sensitivity (macOS sleep adjustments)
- C++20 modules experimental (CMake 3.28+)
- Hardware/container plugins disabled by default; Kubernetes support is stub
- UWP/Xbox unsupported
