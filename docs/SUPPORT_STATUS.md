---
doc_id: "MON-QUAL-006"
doc_title: "Monitoring System - Component Support Status"
doc_version: "1.0.0"
doc_date: "2026-05-21"
doc_status: "Released"
project: "monitoring_system"
category: "QUAL"
---

# Monitoring System — Component Support Status

> **SSOT**: This document is the single source of truth for the **support level**
> of every collector, plugin, and exporter in monitoring_system. It is the audit
> deliverable for [issue #689](https://github.com/kcenon/monitoring_system/issues/689)
> (part of [common_system#684](https://github.com/kcenon/common_system/issues/684)).

## Purpose

The README describes monitoring_system as a modern observability platform with
collectors, tracing, alerting, plugins, and exporters. Downstream consumers need
to distinguish components that are production-ready from examples, placeholders,
and not-yet-implemented headers. This table classifies each component by a
**code-verified** support level so feature claims are auditable.

## Support Levels

| Level | Meaning |
|-------|---------|
| `production` | Header + source implementation present, exercised by tests, safe to integrate. |
| `experimental` | Public API exists and works, but behaviour is environment-dependent (e.g. requires an optional dependency; falls back to a non-transmitting stub otherwise). Use with awareness. |
| `test-only` | Declared in headers but **no implementation** is compiled or linked. Present only as an interface sketch; not usable at runtime. |
| `remove` | Recommended for removal. *(None at this time.)* |

## Audit Method

Classification is based on the **code as SSOT**, not on documentation claims:

1. Marker sweep — `grep -rni -E "TODO|FIXME|HACK|stub|placeholder|not implemented" include src tests benchmarks`.
2. Implementation check — whether a `.cpp` (or inline header definition) actually
   defines the component's member functions, and whether the build (`cmake/sources.cmake`,
   `file(GLOB_RECURSE ... src/*.cpp)`) links it.
3. Test check — whether a dedicated test exists under `tests/`.
4. Factory check — whether `include/kcenon/monitoring/factory/builtin_collectors.h`
   registers the component.

Most markers found in `tests/` (69) and `benchmarks/` (7) are harmless test
scaffolding (mock objects, fixture comments) and do **not** indicate production gaps.
The marker counts that matter are in `include/` and `src/`.

## Collectors

`include/kcenon/monitoring/collectors/` — 17 collector headers + `collector_base.h`.

| Component | Kind | Support | Evidence | Notes |
|-----------|------|---------|----------|-------|
| `collector_base` | collector | `production` | CRTP base, header-only template; consumed by collectors. | Infrastructure, not a runtime collector. |
| `battery_collector` | collector | `production` | `src/collectors/battery_collector.cpp`; `tests/collectors/test_battery_collector.cpp`; registered in `builtin_collectors.h`. | |
| `container_collector` | collector | `production` | `src/collectors/container_collector.cpp`; test present; registered. | Linux cgroup v1/v2 with graceful degradation. |
| `gpu_collector` | collector | `production` | `src/platform/gpu_collector.cpp`; test present. | Windows path is a documented platform stub (no GPU data on Windows). |
| `interrupt_collector` | collector | `production` | `src/collectors/interrupt_collector.cpp`; test present; registered. | Windows path not implemented (documented platform limitation). |
| `network_metrics_collector` | collector | `production` | `src/collectors/network_metrics_collector.cpp`; test present; registered. | Windows path is a documented platform stub. |
| `platform_metrics_collector` | collector | `production` | `src/collectors/platform_metrics_collector.cpp`; test present; registered. | |
| `power_collector` | collector | `production` | `src/platform/power_collector.cpp`; test present. | |
| `process_metrics_collector` | collector | `production` | `src/collectors/process_metrics_collector.cpp`; test present; registered. | |
| `security_collector` | collector | `production` | `src/platform/security_collector.cpp`; test present; registered. | Windows path is a documented platform stub. |
| `smart_collector` | collector | `production` | `src/platform/smart_metrics.cpp` defines `smart_collector::*`; test present; registered. | Uses `smartctl`; degrades gracefully when unavailable. |
| `system_resource_collector` | collector | `production` | `src/collectors/system_resource_collector.cpp`; test present; registered. | Windows PDH path has documented safe-stub branches. |
| `temperature_collector` | collector | `production` | `src/platform/temperature_collector.cpp`; test present. | |
| `uptime_collector` | collector | `production` | `src/collectors/uptime_collector.cpp`; test present; registered. | |
| `vm_collector` | collector | `production` | `src/collectors/vm_collector.cpp`; test present; registered. | Windows path stubbed; CPU-steal is a documented placeholder. |
| `logger_system_collector` | collector | `test-only` | Header `logger_system_collector.h` declares ctor/`collect()`/`initialize()`/`get_statistics()`/`get_metric_types()`, but **no `.cpp` defines them**, no header inline body, no file includes it, no test, not in `builtin_collectors.h`. | Interface sketch only. Not compiled into the library. See follow-up issue. |
| `thread_system_collector` | collector | `test-only` | Header declares `collect()` etc.; **no implementation**, no test, not registered. | Interface sketch only. See follow-up issue. |
| `plugin_metric_collector` | collector | `test-only` | Header declares `force_collect()` and management API; **no implementation**, no test, not registered. | Interface sketch only. See follow-up issue. |

### Collector summary

- `production`: 15 (`collector_base` + 14 runtime collectors)
- `test-only`: 3 (`logger_system_collector`, `thread_system_collector`, `plugin_metric_collector`)
- `experimental`: 0
- `remove`: 0

Platform-specific stub branches (Windows GPU/interrupt/PDH, VM steal placeholder)
inside otherwise-production collectors are **documented platform limitations**, not
support-level downgrades — the collectors compile, link, are tested, and degrade
gracefully.

## Plugins

`include/kcenon/monitoring/plugins/` and `src/plugins/`.

| Component | Kind | Support | Evidence | Notes |
|-----------|------|---------|----------|-------|
| `collector_plugin` | plugin interface | `production` | `collector_plugin.h` — abstract interface, implemented by built-in collectors. | |
| `plugin_api` | plugin interface | `production` | `plugin_api.h` — `DECLARE_MONITORING_PLUGIN` macro and C ABI declarations. | |
| `collector_registry` | plugin infra | `production` | `src/plugins/collector_registry.cpp` (349 lines); `tests/plugins/test_collector_registry*.cpp`. | |
| `plugin_loader` | plugin infra | `production` | `src/plugins/plugin_loader.cpp` (335 lines) — real `dlopen`/`LoadLibraryA` dynamic loading. | |
| `container_plugin` | plugin | `production` | `src/plugins/container/container_plugin.cpp` (304 lines); `tests/plugins/test_container_plugin.cpp`. | |
| `hardware_plugin` | plugin | `production` | `src/plugins/hardware/hardware_plugin.cpp` (298 lines). | |

### Plugin summary

- `production`: 6
- No stub/placeholder markers found in `src/plugins/`.

## Exporters

`include/kcenon/monitoring/exporters/` — header-only (no `src/` directory; classes are inline in headers).

| Component | Kind | Support | Evidence | Notes |
|-----------|------|---------|----------|-------|
| `metric_exporters` | exporter | `production` | `prometheus_exporter`, `statsd_exporter`, `otlp_metrics_exporter` fully defined; `tests/integration/test_metric_exporters.cpp`. | StatsD UDP send path is `// simulated` when no transport is wired — see note below. |
| `trace_exporters` | exporter | `production` | Fully defined; `tests/integration/test_trace_exporters.cpp`. | |
| `opentelemetry_adapter` | exporter | `production` | Fully defined; `tests/integration/test_opentelemetry_adapter.cpp`. | |
| `http_transport` | transport | `experimental` | `http_transport.h` ships `stub_http_transport` (returns an error in stub mode) and a real path behind `MONITORING_HAS_NETWORK_SYSTEM`. | Without `network_system`, the factory falls back to the non-transmitting stub. Mark integrations accordingly. |
| `udp_transport` | transport | `experimental` | `udp_transport.h` ships `stub_udp_transport` (testing), `simple`, and a real path. | Stub is the fallback when no real backend is configured. |
| `grpc_transport` | transport | `experimental` | `grpc_transport.h` ships `stub_grpc_transport` (testing) and a real `grpc::GenericStub` path. | `create_grpc_transport()` falls back to the stub when gRPC is unavailable. |
| `otlp_grpc_exporter` | exporter | `experimental` | `otlp_grpc_exporter.h` fully defined, but `export_spans()` runs over a `grpc_transport` that is the stub unless a real gRPC backend is present. | Functional only when paired with a real (non-stub) gRPC transport. |

### Exporter summary

- `production`: 3 (`metric_exporters`, `trace_exporters`, `opentelemetry_adapter`)
- `experimental`: 4 (`http_transport`, `udp_transport`, `grpc_transport`, `otlp_grpc_exporter`)

The `stub_*_transport` classes are intentional **testing seams** named `stub` — they
are part of the supported transport abstraction, not unfinished work. The
`experimental` label reflects that real network transmission depends on the optional
`network_system`/gRPC backends being present.

## Storage Backends (related)

`include/kcenon/monitoring/storage/storage_backends.h` — outside the issue scope but
flagged during the audit for completeness.

| Component | Support | Notes |
|-----------|---------|-------|
| `memory_storage_backend` | `production` | In-memory snapshot store, fully functional. |
| `file_storage_backend` | `experimental` | `flush()` is a stub (`// actual file I/O would go here`); data is held in memory only. |
| `database_storage_backend` | `experimental` | Header comment marks it a stub implementation. |
| `cloud_storage_backend` | `experimental` | Header comment marks it a stub implementation. |

Storage backend gaps are tracked separately; this table records them so the audit is complete.

## Marker Accounting

`grep -rni -E "TODO|FIXME|HACK|stub|placeholder|not implemented"` results:

| Path | Count | Disposition |
|------|-------|-------------|
| `include/` | 51 | All accounted for: `stub_*_transport` classes (intentional testing seams), documented Windows platform stubs in collectors, storage backend stub comments. See tables above. |
| `src/` | 12 | Documented platform limitations (Windows GPU/interrupt/PDH), VM steal placeholder, Docker stub on non-Linux — all graceful-degradation paths in production collectors. |
| `tests/` | 69 | Harmless test scaffolding (mocks, fixtures). Not production gaps. |
| `benchmarks/` | 7 | Harmless benchmark scaffolding. Not production gaps. |

No marker indicates an undocumented production gap. The only true unimplemented
components are the three `test-only` collectors, which carry follow-up issues.

## Follow-up Issues

Components shipped as if production but backed by no implementation:

- `logger_system_collector`, `thread_system_collector`, `plugin_metric_collector` —
  tracked by [issue #690](https://github.com/kcenon/monitoring_system/issues/690)
  (implement or remove the three `test-only` stub collectors).

---

*Audit deliverable for issue [#689](https://github.com/kcenon/monitoring_system/issues/689).*
