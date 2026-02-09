Architecture Overview
=====================

> **Language:** **English** | [한국어](ARCHITECTURE.kr.md)

**Version**: 0.4.0.0
**Last Updated**: 2026-02-09

## Purpose

monitoring_system provides metrics collection, storage, analysis, health checks, and event-driven observability for C++20 services. It is interface-first and usable standalone or integrated with the thread_system and logger_system projects.

## Table of Contents

1. [System Architecture Diagram](#1-system-architecture-diagram)
2. [Core Pipeline Architecture](#2-core-pipeline-architecture)
3. [Threading Model](#3-threading-model)
4. [Plugin Architecture](#4-plugin-architecture)
5. [Storage Layer](#5-storage-layer)
6. [Adapter & Interface Layer](#6-adapter--interface-layer)
7. [Design Decisions](#7-design-decisions)
8. [Module Reference](#8-module-reference)

---

## 1. System Architecture Diagram

### Module Dependency Graph

The system comprises 15+ modules organized in a layered architecture. Dependencies flow downward; higher layers depend on lower layers but never the reverse.

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Application Layer                            │
│   Examples  •  User Code  •  Test Suite  •  Benchmarks              │
└──────────────────────────────┬──────────────────────────────────────┘
                               │
┌──────────────────────────────▼──────────────────────────────────────┐
│                     Collectors & Plugins                             │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐  │
│  │  Core Collectors  │  │ Hardware Plugin  │  │ Container Plugin │  │
│  │  (6 built-in)     │  │ (battery, GPU,   │  │ (Docker, cgroups,│  │
│  │                   │  │  power, temp)    │  │  SMART)          │  │
│  └────────┬─────────┘  └────────┬─────────┘  └────────┬─────────┘  │
│           └──────────────┬──────┴──────────────────────┘            │
│                          ▼                                          │
│                   collector_base<T>  (CRTP)                         │
└──────────────────────────┬──────────────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────────────┐
│                        Core Module                                   │
│  ┌────────────┐  ┌────────────┐  ┌───────────────┐  ┌───────────┐  │
│  │ central_   │  │ event_bus  │  │ safe_event_   │  │performance│  │
│  │ collector  │  │            │  │ dispatcher    │  │ _monitor  │  │
│  └──────┬─────┘  └──────┬─────┘  └───────┬───────┘  └─────┬─────┘  │
│         │               │                │                │         │
│  ┌──────▼─────┐  ┌──────▼─────┐  ┌───────▼───────┐                 │
│  │thread_local│  │event_types │  │ result_types  │                  │
│  │ _buffer    │  │            │  │ error_codes   │                  │
│  └────────────┘  └────────────┘  └───────────────┘                  │
└──────────────────────────┬──────────────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────────────┐
│                     Interfaces & Concepts                            │
│  ┌──────────────┐ ┌───────────────┐ ┌────────────┐ ┌────────────┐  │
│  │ monitoring_  │ │metric_collector│ │monitorable │ │event_bus_  │  │
│  │ core         │ │_interface     │ │_interface   │ │interface   │  │
│  └──────────────┘ └───────────────┘ └────────────┘ └────────────┘  │
│  ┌──────────────┐ ┌───────────────┐                                 │
│  │ observer_    │ │ monitoring_   │  (C++20 concepts: 14 concepts)  │
│  │ interface    │ │ concepts.h    │                                  │
│  └──────────────┘ └───────────────┘                                 │
└──────────────────────────┬──────────────────────────────────────────┘
                           │
         ┌─────────────────┼─────────────────────┐
         │                 │                     │
┌────────▼───────┐ ┌──────▼───────┐  ┌──────────▼──────────┐
│  Support Layer │ │ Export Layer  │  │  Cross-System       │
│                │ │              │  │  Integration         │
│ • storage/     │ │ • exporters/ │  │                      │
│ • reliability/ │ │   - OTLP     │  │ • adapters/          │
│ • alert/       │ │   - HTTP     │  │   - thread_system    │
│ • tracing/     │ │   - gRPC     │  │   - logger_system    │
│ • health/      │ │   - UDP      │  │   - common_system    │
│ • optimization/│ │              │  │                      │
│ • adaptive/    │ │              │  │ • di/                │
│ • context/     │ │              │  │   - service_registry │
└────────────────┘ └──────────────┘  └─────────────────────┘
```

### Data Flow: Collection to Export

```
[Application Code]
        │
        ▼  record()
┌──────────────────┐   flush()    ┌───────────────────┐
│  thread_local_   │ ──────────►  │  central_         │
│  buffer          │  (per-thread) │  collector        │
│  (lock-free,     │              │  (shared_mutex,   │
│   ~5-10 ns/op)   │              │   LRU eviction)   │
└──────────────────┘              └────────┬──────────┘
                                           │ aggregated profiles
                                           ▼
                                  ┌───────────────────┐
                                  │  event_bus        │
                                  │  (pub/sub,        │
                                  │   priority queue) │
                                  └────────┬──────────┘
                           ┌───────────────┼───────────────┐
                           ▼               ▼               ▼
                    ┌────────────┐  ┌────────────┐  ┌────────────┐
                    │  Storage   │  │  Exporters │  │  Alert     │
                    │  Backend   │  │  (OTLP,    │  │  Pipeline  │
                    │            │  │   HTTP)    │  │            │
                    └────────────┘  └────────────┘  └────────────┘
```

### Event Bus Pub/Sub Topology

```
Publishers:                          Subscribers:
┌──────────────┐                    ┌──────────────────┐
│ Collectors   │──publish()────►    │ Storage backends  │
│ Health checks│                    │ Alert pipeline    │
│ Adapters     │  event_bus         │ Exporters         │
│ User code    │  (priority queue,  │ Health monitor    │
└──────────────┘   worker threads)  │ Adaptive monitor  │
                                    └──────────────────┘

Event Priority: critical > high > normal > low
Back-pressure: configurable threshold (default 8000/10000)
```

---

## 2. Core Pipeline Architecture

### Central Collector Flow

The central pipeline follows a **producer-consumer** pattern with thread-local buffering for lock-free hot-path performance.

```
Thread 1                 Thread 2                 Thread N
    │                        │                        │
    ▼                        ▼                        ▼
┌──────────┐           ┌──────────┐           ┌──────────┐
│ TL Buffer│           │ TL Buffer│           │ TL Buffer│
│ cap: 256 │           │ cap: 256 │           │ cap: 256 │
└────┬─────┘           └────┬─────┘           └────┬─────┘
     │ flush (batch)        │ flush (batch)        │ flush (batch)
     └──────────────────────┼──────────────────────┘
                            ▼
                 ┌─────────────────────┐
                 │  central_collector   │
                 │                     │
                 │  receive_batch()    │
                 │   └─ process_sample │
                 │       └─ aggregate  │
                 │           to profile│
                 │                     │
                 │  shared_mutex:      │
                 │   read: get_profile │
                 │   write: receive    │
                 │                     │
                 │  LRU eviction when  │
                 │  max_profiles hit   │
                 └─────────────────────┘
```

**Key classes:**

| Class | File | Role |
|-------|------|------|
| `thread_local_buffer` | `core/thread_local_buffer.h` | Lock-free per-thread sample recording |
| `central_collector` | `core/central_collector.h` | Batched aggregation with LRU eviction |
| `metric_sample` | `core/thread_local_buffer.h` | Sample data: operation name, duration, success, timestamp |
| `performance_profile` | `core/performance_types.h` | Aggregated profile: count, avg, min, max, p50, p95, p99 |

### Event Bus Event Routing

The `event_bus` provides decoupled communication between monitoring components using a type-erased publish-subscribe mechanism.

```cpp
// Type-safe publish/subscribe via std::type_index + std::any
event_bus_config config;
config.max_queue_size = 10000;
config.worker_thread_count = 2;
config.enable_back_pressure = true;

auto bus = std::make_shared<event_bus>(config);
bus->start();

// Subscribe with priority
auto token = bus->subscribe_event<alert_event>(
    [](const alert_event& e) { handle_alert(e); },
    event_priority::high
);

// Publish (async, queued)
bus->publish_event(alert_event{"CPU threshold exceeded"});
```

**Internal mechanics:**
- Events wrapped in `event_envelope` with type index, priority, timestamp, and unique ID
- Priority queue orders events by priority then by arrival time (FIFO within same priority)
- Worker threads process events in batches of 10 from the priority queue
- Handlers sorted by priority within each event type for deterministic dispatch order

### Safe Event Dispatcher

`safe_event_dispatcher` wraps `event_bus` to add production safety:
- **Exception isolation**: one handler failure does not affect others
- **Circuit breaker**: disables repeatedly failing handlers (configurable threshold)
- **Dead letter queue**: stores failed events for later recovery
- **Error metrics**: tracks total exceptions and per-handler failure counts

### Performance Monitor Orchestration

`performance_monitor` ties the pipeline together as the top-level orchestrator:
- Owns the `central_collector`
- Provides `thread_local_buffer` access per thread
- Exposes `metrics_snapshot` with counter, gauge, histogram, and timer support
- Implements `monitorable_interface` for lifecycle management (`configure`, `start`, `stop`, `collect_now`)

### Error Handling Flow

```
result_types.h                      error_codes.h
┌────────────────────┐             ┌─────────────────────────┐
│ common::Result<T>  │◄────uses────│ monitoring_error_code   │
│ common::VoidResult │             │   invalid_configuration │
│                    │             │   resource_exhausted    │
│ Monadic operations:│             │   already_started       │
│  and_then()        │             │   not_found             │
│  map()             │             │   timeout               │
│  or_else()         │             │   ...                   │
└────────────────────┘             └─────────────────────────┘

// Example: composable error handling without exceptions
auto result = collector.get_profile("db_query")
    .and_then([](auto& profile) { return validate(profile); })
    .map([](auto& valid) { return format_report(valid); });
```

---

## 3. Threading Model

### Thread-Local Buffer Design

Each thread owns a `thread_local_buffer` (capacity: 256 samples by default). Recording a sample is a direct array write with **no locks, no atomics, ~5-10 ns per operation**.

```
Thread-Local Storage (TLS)
┌──────────────────────────────────────┐
│  thread_local_buffer                 │
│  ┌──────────────────────────────┐    │
│  │  buffer_[0..255]  (pre-allocated) │
│  │  write_index_  (plain size_t)│    │
│  │  collector_    (shared_ptr)  │    │
│  └──────────────────────────────┘    │
│                                      │
│  record():      O(1), no lock        │
│  flush():       sends batch to       │
│                 central_collector     │
│                 (acquires write lock) │
│  ~destructor:   auto-flushes         │
└──────────────────────────────────────┘
```

**Design rationale**: Thread-local buffers eliminate contention on the hot path. The only synchronization point is the `flush()` call, which batches samples to amortize lock acquisition cost.

### Thread Context Management

`context/thread_context.h` provides thread-local context propagation:
- **Correlation IDs**: Unique identifiers for request tracing across thread boundaries
- **Context stacking**: Push/pop context for nested operations
- **Automatic cleanup**: RAII-based context scoping

### Lock-Free Queue (`optimization/lockfree_queue.h`)

For high-throughput scenarios, a lock-free MPSC (multi-producer, single-consumer) queue supplements the thread-local buffer approach:

```
Configuration:
  initial_capacity: 1024
  max_capacity:     65536
  allow_overwrite:  false

Implementation:
  - Cache-line aligned head/tail indices (prevent false sharing)
  - Atomic compare-and-swap for push operations
  - Statistics tracking: push/pop attempts, successes, failures, contentions
```

### Safe Event Dispatcher Thread Safety

The `event_bus` uses **fine-grained locking** with three separate mutexes:
- `bus_mutex_`: protects start/stop lifecycle operations
- `queue_mutex_`: protects the priority queue for event enqueue/dequeue
- `handlers_mutex_`: protects subscriber registration/unregistration
- `scoped_lock` used for atomic acquisition of multiple locks (deadlock prevention)

Worker threads wait on a `condition_variable` and process events in configurable batches.

### Concurrency Summary

| Component | Strategy | Contention |
|-----------|----------|------------|
| `thread_local_buffer` | No synchronization (TLS) | Zero |
| `central_collector` | `shared_mutex` (readers/writer) | Low (batched writes) |
| `event_bus` | Fine-grained mutexes + condition_variable | Low (worker threads) |
| `lockfree_queue` | Atomic CAS, cache-line padding | Minimal |
| `performance_monitor` | Atomic counters for metrics | Zero (relaxed ordering) |

---

## 4. Plugin Architecture

### Plugin Loading Lifecycle

The plugin system enables optional collector registration without modifying core code.

```
                    ┌──────────────────────────────┐
                    │      collector_plugin         │
                    │      (interface)              │
                    ├──────────────────────────────┤
                    │  name()           → string   │
                    │  collect()        → metrics  │
                    │  interval()       → duration │
                    │  is_available()   → bool     │
                    │  get_metric_types() → vector │
                    │  get_metadata()   → metadata │
                    └──────────┬───────────────────┘
                               │
              ┌────────────────┼────────────────┐
              ▼                                 ▼
┌──────────────────────┐          ┌──────────────────────┐
│  hardware_plugin     │          │  container_plugin    │
│                      │          │                      │
│  Registers:          │          │  Registers:          │
│  • battery_collector │          │  • container_collector│
│  • power_collector   │          │  • smart_collector   │
│  • temperature_      │          │                      │
│    collector         │          │  Platform:           │
│  • gpu_collector     │          │  • cgroups v1/v2     │
│                      │          │  • Docker API        │
│  Platform:           │          │  • SMART via         │
│  • IOKit (macOS)     │          │    /dev/sd*          │
│  • RAPL (Linux)      │          │                      │
│  • WMI (Windows)     │          │  Build flag:         │
│  • NVML/ROCm (GPU)   │          │  MONITORING_BUILD_   │
│                      │          │  CONTAINER_PLUGIN=ON │
│  Build flag:         │          │                      │
│  MONITORING_BUILD_   │          └──────────────────────┘
│  HARDWARE_PLUGIN=ON  │
└──────────────────────┘
```

### CRTP Collector Base Pattern

All collectors inherit from `collector_base<Derived>` using the Curiously Recurring Template Pattern for zero-cost static polymorphism:

```cpp
template <typename Derived>
class collector_base {
public:
    // Common functionality: initialize, collect, get_name, is_healthy
    bool initialize(const config_map& config) {
        // Parse common config ("enabled")
        return derived().do_initialize(config);  // Delegate to Derived
    }

    std::vector<metric> collect() {
        if (!enabled_) return {};
        auto metrics = derived().do_collect();    // Static dispatch
        ++collection_count_;                      // Atomic counter
        return metrics;
    }
private:
    Derived& derived() { return static_cast<Derived&>(*this); }
};
```

**Benefits**: No virtual function overhead. The compiler inlines `do_collect()` calls directly, achieving the same performance as hand-written code while eliminating boilerplate for initialization, error handling, statistics tracking, and health monitoring.

### Core Collectors (6 built-in)

| Collector | Metrics | Platform |
|-----------|---------|----------|
| `system_resource_collector` | CPU, memory, disk usage | Linux, macOS, Windows |
| `network_metrics_collector` | TCP states, socket buffers | Linux, macOS |
| `process_metrics_collector` | FD count, inodes, context switches | Linux, macOS |
| `platform_metrics_collector` | Platform-specific metrics (Strategy pattern) | All |
| `thread_system_collector` | Thread pool utilization | All |
| `logger_system_collector` | Logger throughput metrics | All |

### Plugin Registry and Loader

- `collector_registry`: Runtime registration of plugin factories
- `plugin_loader`: Dynamic plugin discovery and instantiation
- `plugin_api.h`: Stable C++ API surface for plugin authors

---

## 5. Storage Layer

### Backend Abstraction

All storage backends implement the `snapshot_storage_backend` interface:

```
snapshot_storage_backend (abstract)
├── store(snapshot)      → Result<bool>
├── retrieve(index)      → Result<snapshot>
├── retrieve_range()     → Result<vector<snapshot>>
├── size() / capacity()
├── flush()              → Result<bool>
├── clear()              → Result<bool>
└── get_stats()          → map<string, size_t>
        │
        ├── memory_storage_backend     (in-memory deque, bounded)
        ├── file_storage_backend       (JSON, binary, CSV)
        ├── database_storage_backend   (SQLite, PostgreSQL, MySQL)
        └── cloud_storage_backend      (S3, GCS, Azure Blob)
```

### Supported Backends

| Category | Backend Type | Key Characteristics |
|----------|-------------|---------------------|
| **Memory** | `memory_buffer` | Bounded deque, fastest, volatile |
| **File** | `file_json` | Human-readable, larger footprint |
| | `file_binary` | Compact, fast serialization |
| | `file_csv` | Spreadsheet-compatible |
| **Database** | `database_sqlite` | Embedded, zero-config |
| | `database_postgresql` | Full ACID, scalable |
| | `database_mysql` | Wide ecosystem support |
| **Cloud** | `cloud_s3` | AWS object storage |
| | `cloud_gcs` | Google Cloud Storage |
| | `cloud_azure_blob` | Azure Blob Storage |

### Backend Selection and Configuration

```cpp
storage_config config;
config.type = storage_backend_type::file_json;
config.path = "/var/monitoring/data";
config.max_capacity = 10000;
config.compression = compression_algorithm::zstd;
config.auto_flush = true;
config.flush_interval = std::chrono::seconds(5);

auto backend = storage_backend_factory::create_backend(config);
```

Helper functions simplify common configurations:

```cpp
auto file_store = create_file_storage("/path", storage_backend_type::file_json, 5000);
auto db_store   = create_database_storage(storage_backend_type::database_sqlite, "/path/db", "metrics");
auto cloud_store = create_cloud_storage(storage_backend_type::cloud_s3, "my-bucket");
```

---

## 6. Adapter & Interface Layer

### Core Interfaces

The interface layer defines the contracts that all components implement:

| Interface | File | Contract |
|-----------|------|----------|
| `monitoring_core` | `interfaces/monitoring_core.h` | Central monitoring abstraction with `metrics_snapshot` |
| `metric_collector_interface` | `interfaces/metric_collector_interface.h` | `collect()`, `get_name()`, `is_healthy()` |
| `monitorable_interface` | `interfaces/monitorable_interface.h` | `configure()`, `start()`, `stop()`, `collect_now()` |
| `interface_event_bus` | `interfaces/event_bus_interface.h` | `publish_event()`, `subscribe_event()`, type-safe |
| `observer_interface` | `interfaces/observer_interface.h` | `on_update()` notification |

### Adapter Pattern for Cross-System Integration

```
┌────────────────────┐      ┌────────────────────────┐
│  thread_system     │      │  logger_system         │
│  (Tier 1)          │      │  (Tier 2, optional)    │
└────────┬───────────┘      └────────┬───────────────┘
         │                           │
         ▼                           ▼
┌────────────────────┐      ┌────────────────────────┐
│thread_system_      │      │logger_system_          │
│adapter             │      │adapter                 │
│                    │      │                        │
│Converts thread pool│      │Converts logger         │
│metrics → monitoring│      │metrics → monitoring    │
│metrics             │      │metrics                 │
│                    │      │                        │
│Graceful fallback   │      │Graceful fallback       │
│when not present    │      │when not present        │
└────────┬───────────┘      └────────┬───────────────┘
         │                           │
         └────────────┬──────────────┘
                      ▼
            ┌───────────────────┐
            │ monitoring_system │
            │ (Tier 3)         │
            └───────────────────┘
```

**Available adapters:**

| Adapter | Source System | Purpose |
|---------|-------------|---------|
| `thread_system_adapter` | thread_system | Thread pool metrics → monitoring metrics |
| `logger_system_adapter` | logger_system | Logger throughput → monitoring metrics |
| `common_system_adapter` | common_system | Shared `IMonitor`/`ILogger` interfaces |
| `monitor_adapter` | Base adapter | Abstract adapter for custom integrations |
| `performance_monitor_adapter` | monitoring_system | Performance metrics bridge |

**Integration topology:**

```
thread_system ──(metrics)──► monitoring_system ◄──(metrics)── logger_system
        │                                      │
        └──── application components ──────────┘
```

### Dependency Injection

`di/service_registration.h` provides a DI container with:
- Singleton and transient lifecycles
- Type-safe service registration and resolution
- Bidirectional integration between ecosystem systems

### C++20 Concepts

`concepts/monitoring_concepts.h` defines 14 compile-time constraints:

| Concept | Constrains | Requirements |
|---------|-----------|--------------|
| `MetricValue` | Metric values | Arithmetic type |
| `MetricType` | Metric structures | Class with `.name` and `.value` |
| `MetricSourceLike` | Data sources | `get_current_metrics()`, `get_source_name()`, `is_healthy()` |
| `MetricCollectorLike` | Collectors | `collect_metrics()`, `is_collecting()`, `get_metric_types()` |
| `ObserverLike` | Observers | `on_metrics_updated()` |
| `MonitoringEventType` | Events | Class, copy-constructible |
| `MonitoringEventHandler<E>` | Event handlers | Invocable with `const E&`, returns void |
| `MetricFilterPredicate<M>` | Filters | Invocable with `const M&`, returns bool |
| `MetricTransformer<M>` | Transformers | Invocable with `const M&` |
| `ConfigValidatable` | Configurations | Has `validate()` method |
| `StorageBackendLike` | Storage | `store()`, `is_connected()` |
| `ExporterLike` | Exporters | `export_metrics()`, `is_ready()` |
| `HealthCheckable` | Health checks | `is_healthy()` returns bool |
| `TracingContextLike` | Trace contexts | `get_trace_id()`, `get_span_id()` |

---

## 7. Design Decisions

### Why Event Bus for Decoupled Communication

**Problem**: Tight coupling between collectors, storage, exporters, and alerting would create a dependency web requiring changes in multiple modules for any new feature.

**Decision**: In-process event bus with type-erased publish-subscribe.

**Consequences**:
- Components only depend on event type definitions, not on each other
- New subscribers (storage backends, exporters) can be added without modifying publishers
- Priority-based processing ensures critical events (alerts) are handled first
- Back-pressure prevents memory exhaustion under burst load

### Why Thread-Local Buffering for Performance

**Problem**: Shared-state metric collection creates lock contention proportional to thread count, unacceptable for high-frequency recording (>1M ops/sec).

**Decision**: Each thread owns a `thread_local_buffer` that flushes batches to `central_collector`.

**Consequences**:
- Hot-path recording is ~5-10 ns (direct array write, no synchronization)
- Lock acquisition cost is amortized over 256 samples per flush
- `central_collector` uses `shared_mutex` for read-heavy access pattern
- Trade-off: slight delay between recording and global visibility (up to 256 samples)

### Why CRTP for Collector Base (Zero-Cost Abstraction)

**Problem**: Virtual dispatch overhead (~2-5 ns per call) multiplied by collection frequency (millions/sec) creates measurable overhead.

**Decision**: CRTP (`collector_base<Derived>`) instead of virtual inheritance.

**Consequences**:
- Compiler inlines derived `do_collect()` calls — zero runtime overhead
- Common functionality (init, stats, health) implemented once in base
- Trade-off: slightly more complex template syntax for collector authors
- Each collector defines: `do_initialize()`, `do_collect()`, `is_available()`, `do_get_metric_types()`

### Why Plugin Architecture for Hardware/Container Collectors

**Problem**: Hardware monitoring (battery, GPU, temperature) and container monitoring (Docker, cgroups) require platform-specific dependencies that not all deployments need.

**Decision**: Optional plugins enabled via CMake flags (`MONITORING_BUILD_HARDWARE_PLUGIN`, `MONITORING_BUILD_CONTAINER_PLUGIN`).

**Consequences**:
- Core library remains lightweight (~80 headers, no optional dependencies)
- Plugins link separately (`libmonitoring_hardware_plugin.a`, `libmonitoring_container_plugin.a`)
- Platform availability checked at runtime via `is_available()`
- Server deployments skip hardware plugins; container deployments enable container plugin

### Why Result<T> Pattern Instead of Exceptions

**Problem**: Exceptions create hidden control flow, have performance implications in error paths, and are problematic across module boundaries in C++.

**Decision**: Monadic `common::Result<T>` type from common_system.

**Consequences**:
- Explicit error propagation — no hidden control flow
- Composable with `and_then()`, `map()`, `or_else()`
- No stack unwinding overhead in error paths
- All monitoring_system APIs return `Result<T>` or `VoidResult`
- Error taxonomy defined in `error_codes.h` with `monitoring_error_code` enum

### C++20 Concepts for Compile-Time Validation

**Problem**: Template metaprogramming produces cryptic error messages when constraints are violated.

**Decision**: C++20 concepts in `monitoring_concepts.h` for explicit type constraints.

**Consequences**:
- Clear compiler diagnostics when types don't meet requirements
- Self-documenting: concept definitions describe the expected interface
- Interoperates with common_system concepts when available (`KCENON_HAS_COMMON_SYSTEM`)

---

## 8. Module Reference

### Directory Structure

```
include/kcenon/monitoring/
├── core/                    # Core pipeline (9 headers)
├── interfaces/              # Abstract contracts (6 headers)
├── concepts/                # C++20 concepts (1 header)
├── collectors/              # Metric collectors (18 headers)
├── plugins/                 # Optional plugins (6 headers)
│   ├── hardware/            # Battery, power, temp, GPU
│   └── container/           # Docker, SMART
├── storage/                 # Storage backends (1 header, 5 backends)
├── exporters/               # Data export (7 headers)
├── adapters/                # Cross-system adapters (10 headers)
├── reliability/             # Fault tolerance (7 headers)
├── alert/                   # Alert pipeline (7 headers)
├── tracing/                 # Distributed tracing (2 headers)
├── health/                  # Health monitoring (1 header)
├── optimization/            # Performance optimization (3 headers)
├── adaptive/                # Adaptive monitoring (1 header)
├── context/                 # Thread context (1 header)
├── di/                      # Dependency injection (1 header)
├── factory/                 # Factory patterns (3 headers)
├── config/                  # Feature flags (1 header)
├── utils/                   # Utilities (10 headers)
├── compatibility.h          # Backward compatibility
└── forward.h                # Forward declarations
```

### Build & Options

- **Standard**: C++20 (CMake 3.20+)
- **Dependencies**: common_system (required), thread_system (required), logger_system (optional)
- **Optional flags**: `MONITORING_BUILD_HARDWARE_PLUGIN`, `MONITORING_BUILD_CONTAINER_PLUGIN`, `MONITORING_WITH_GRPC`, `MONITORING_WITH_NETWORK_SYSTEM`
- **Platforms**: Linux, macOS, Windows (cross-platform CI verified)
- **Sanitizers**: ASan, TSan, UBSan support via CMake options

### Design Principles

- **Interface segregation**: Decouple producers, transport, and sinks
- **Back-pressure ready**: Bounded queues and configurable thresholds
- **Result pattern**: Explicit error reporting (no exceptions across module boundaries)
- **Zero-cost abstractions**: CRTP, compile-time concepts, lock-free hot paths
- **Graceful degradation**: Adapters return empty sets when dependencies are absent

---

*Last Updated: 2026-02-09*
