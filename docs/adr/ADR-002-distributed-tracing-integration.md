---
doc_id: "MON-ADR-002"
doc_title: "ADR-002: Distributed Tracing Integration"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Accepted"
project: "monitoring_system"
category: "ADR"
---

# ADR-002: Distributed Tracing Integration

> **SSOT**: This document is the single source of truth for **ADR-002: Distributed Tracing Integration**.

| Field | Value |
|-------|-------|
| Status | Accepted |
| Date | 2025-05-01 |
| Decision Makers | kcenon ecosystem maintainers |

## Context

The kcenon ecosystem runs distributed workloads where a single operation may span
multiple systems (network_system -> database_system -> pacs_system). Debugging
latency issues or failures across system boundaries requires correlated tracing.

Requirements:
1. Trace a request across ecosystem boundaries with correlated IDs.
2. Support export to industry-standard backends (Jaeger, Zipkin, OTLP).
3. Minimize overhead on the hot path (< 50 ns for context propagation).
4. Work without mandatory external dependencies.

Two integration approaches were evaluated:
- Use OpenTelemetry C++ SDK directly.
- Build a lightweight tracing implementation with export adapters.

## Decision

**Build a lightweight native tracing implementation** with W3C-compatible trace
context and export adapters for Jaeger, Zipkin, and OTLP.

Design:
1. **`trace_context`** — Thread-local storage for active span context (trace_id,
   span_id, parent_span_id). Propagation overhead < 50 ns via `thread_local`.
2. **`distributed_tracer`** — Creates spans with automatic parent linking,
   duration measurement, and attribute attachment.
3. **Export adapters** — `otlp_exporter`, `jaeger_exporter`, `zipkin_exporter`
   serialize spans to the respective wire formats (HTTP/gRPC/UDP).
4. **Context propagation** — W3C `traceparent`/`tracestate` header format for
   interoperability with OpenTelemetry-instrumented services.

```cpp
auto span = tracer.start_span("process_request");
span.set_attribute("request.id", request_id);
// ... operation ...
span.end();  // duration recorded, exported asynchronously
```

## Alternatives Considered

### OpenTelemetry C++ SDK

- **Pros**: Industry standard, feature-complete, community-maintained.
- **Cons**: Heavy dependency (~50+ headers, Abseil, gRPC, protobuf). Increases
  build times significantly. The SDK's memory model and threading assumptions
  may conflict with the ecosystem's own thread_system. Version compatibility
  across 8 repositories adds maintenance burden.

### No Tracing (Logging Only)

- **Pros**: Zero overhead, no additional complexity.
- **Cons**: Correlating log entries across services requires manual log parsing.
  No visualization of request flow, no latency breakdown.

### Distributed Tracing via Logger System

- **Pros**: Reuses existing infrastructure, single log pipeline.
- **Cons**: Conflates two concerns (logging and tracing). Trace spans have
  different lifecycle, storage, and query patterns than log entries.

## Consequences

### Positive

- **Low overhead**: Thread-local context propagation at < 50 ns satisfies the
  performance requirement for hot-path instrumentation.
- **No heavy dependencies**: The native implementation avoids pulling
  OpenTelemetry SDK's transitive dependencies into all ecosystem projects.
- **Interoperable**: W3C trace context format enables correlation with external
  OpenTelemetry-instrumented services.
- **Selective export**: Export adapters are optional — applications only link
  the exporter they need.

### Negative

- **Maintenance burden**: A custom tracing implementation must be maintained
  instead of leveraging the OpenTelemetry community.
- **Feature gap**: The native implementation covers basic tracing but lacks
  advanced features (baggage propagation, sampling strategies, metric exemplars)
  that OpenTelemetry SDK provides.
- **Migration path**: If the ecosystem later adopts OpenTelemetry SDK, the
  native tracing API will need a migration layer or deprecation period.
