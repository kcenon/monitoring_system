# Distributed Tracing Deep Dive Guide

> **Version**: 0.1.0
> **Module**: `kcenon::monitoring::tracing`, `kcenon::monitoring::exporters`
> **Audience**: Developers implementing distributed tracing across services

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Core Concepts](#2-core-concepts)
3. [Trace Context and Propagation](#3-trace-context-and-propagation)
4. [Creating and Managing Spans](#4-creating-and-managing-spans)
5. [Backend Integration](#5-backend-integration)
6. [Cross-Service Tracing](#6-cross-service-tracing)
7. [OpenTelemetry Compatibility](#7-opentelemetry-compatibility)
8. [Transport Layer](#8-transport-layer)
9. [Production Examples](#9-production-examples)
10. [Performance Considerations](#10-performance-considerations)
11. [API Reference](#11-api-reference)

---

## 1. Architecture Overview

The distributed tracing framework tracks requests as they flow across services and components. It provides span-based instrumentation with export capabilities to Jaeger, Zipkin, and OTLP backends.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                        Application Code                                  │
│                                                                          │
│   TRACE_SPAN("operation")    tracer.start_span()    span_builder        │
│         │                          │                       │             │
└─────────┼──────────────────────────┼───────────────────────┼─────────────┘
          │                          │                       │
          ▼                          ▼                       ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                      distributed_tracer                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌─────────────┐ │
│  │ Span Mgmt    │  │   Context    │  │  Thread-     │  │   Export    │ │
│  │ start/finish │  │ Propagation  │  │  Local       │  │   Queue     │ │
│  │ parent/child │  │ inject/      │  │  Storage     │  │   (batch)   │ │
│  │              │  │ extract      │  │              │  │             │ │
│  └──────────────┘  └──────────────┘  └──────────────┘  └──────┬──────┘ │
└───────────────────────────────────────────────────────────────┼────────┘
                                                                │
          ┌─────────────────────────────────────────────────────┤
          │                                                     │
          ▼                                                     ▼
┌──────────────────┐  ┌──────────────────┐  ┌──────────────────────────┐
│ trace_context    │  │ thread_context   │  │ trace_exporter_interface │
│ ┌──────────────┐ │  │ ┌──────────────┐ │  │ ┌──────────────────────┐ │
│ │ W3C          │ │  │ │ request_id   │ │  │ │  jaeger_exporter     │ │
│ │ traceparent  │ │  │ │ correlation  │ │  │ ├──────────────────────┤ │
│ │ tracestate   │ │  │ │ span_id      │ │  │ │  zipkin_exporter     │ │
│ │ baggage      │ │  │ │ trace_id     │ │  │ ├──────────────────────┤ │
│ └──────────────┘ │  │ │ tags         │ │  │ │  otlp_exporter       │ │
└──────────────────┘  │ └──────────────┘ │  │ └──────────────────────┘ │
                      └──────────────────┘  └──────────────────────────┘
                                                        │
                                              ┌─────────┴─────────┐
                                              │                   │
                                              ▼                   ▼
                                     ┌──────────────┐   ┌──────────────┐
                                     │http_transport │   │grpc_transport│
                                     └──────────────┘   └──────────────┘
```

### Key Components

| Component | Header | Purpose |
|-----------|--------|---------|
| Tracer | `distributed_tracer.h` | Central span management and lifecycle |
| Context | `trace_context.h` (public) | Lightweight context for propagation |
| Thread context | `thread_context.h` | Thread-local correlation and IDs |
| Exporters | `trace_exporters.h` | Jaeger, Zipkin, OTLP backends |
| OTel adapter | `opentelemetry_adapter.h` | OpenTelemetry format conversion |
| HTTP transport | `http_transport.h` | HTTP client abstraction |
| gRPC transport | `grpc_transport.h` | gRPC client abstraction |

### Include Structure

```cpp
// Core tracing
#include <kcenon/monitoring/tracing/distributed_tracer.h>  // Main tracer + span types
#include <kcenon/monitoring/tracing/trace_context.h>        // Lightweight context

// Thread context
#include <kcenon/monitoring/context/thread_context.h>       // Thread-local storage

// Exporters
#include <kcenon/monitoring/exporters/trace_exporters.h>    // All backend exporters
#include <kcenon/monitoring/exporters/opentelemetry_adapter.h> // OTel compatibility
#include <kcenon/monitoring/exporters/http_transport.h>     // HTTP transport layer
#include <kcenon/monitoring/exporters/grpc_transport.h>     // gRPC transport layer
```

---

## 2. Core Concepts

### Trace Span

A `trace_span` represents a single unit of work within a distributed trace:

```cpp
struct trace_span {
    std::string trace_id;           // Shared across all spans in a trace
    std::string span_id;            // Unique identifier for this span
    std::string parent_span_id;     // Empty for root spans
    std::string operation_name;     // e.g., "HTTP GET /api/users"
    std::string service_name;       // e.g., "user-service"

    // Timing
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    std::chrono::microseconds duration{0};

    // Metadata
    std::unordered_map<std::string, std::string> tags;     // Indexed attributes
    std::unordered_map<std::string, std::string> baggage;  // Propagated context

    // Status
    enum class status_code { unset, ok, error };
    status_code status{status_code::unset};
    std::string status_message;
};
```

### Trace Hierarchy

```
Trace (trace_id = "abc123")
│
├── Root Span: "HTTP GET /api/orders"          (span_id = "s1", parent = "")
│   ├── Child: "authenticate_request"          (span_id = "s2", parent = "s1")
│   ├── Child: "query_database"                (span_id = "s3", parent = "s1")
│   │   └── Child: "execute_sql"               (span_id = "s4", parent = "s3")
│   └── Child: "serialize_response"            (span_id = "s5", parent = "s1")
```

All spans within the same trace share the same `trace_id`. Parent-child relationships are established through `parent_span_id`.

### Span Lifecycle

```
                 start_span()
                      │
                      ▼
              ┌───────────────┐
              │   Recording   │  tags, baggage, events can be added
              │  (in-flight)  │
              └───────┬───────┘
                      │ finish_span()
                      ▼
              ┌───────────────┐
              │   Finished    │  duration calculated, exported
              │  (immutable)  │
              └───────────────┘
```

### Status Codes

| Code | Meaning | When to Use |
|------|---------|-------------|
| `unset` | Default | Span completed, no explicit status set |
| `ok` | Success | Operation completed successfully |
| `error` | Failure | Operation encountered an error |

---

## 3. Trace Context and Propagation

### W3C Trace Context Format

The framework implements W3C Trace Context for cross-service propagation:

```
traceparent: 00-{trace_id}-{span_id}-{trace_flags}
             │       │          │          │
             │       │          │          └── Sampling flags ("01" = sampled)
             │       │          └── Current span ID (16 hex chars)
             │       └── Trace ID (32 hex chars)
             └── Version (always "00")

Example: 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01
```

### trace_context Structure

```cpp
struct trace_context {
    std::string trace_id;      // 32-character hex trace ID
    std::string span_id;       // 16-character hex span ID
    std::string trace_flags;   // Sampling and other flags
    std::string trace_state;   // Vendor-specific trace state
    std::unordered_map<std::string, std::string> baggage;  // Propagated key-values
};
```

### Serialization

```cpp
// Serialize to W3C traceparent header
trace_context ctx;
ctx.trace_id = "4bf92f3577b34da6a3ce929d0e0e4736";
ctx.span_id = "00f067aa0ba902b7";
ctx.trace_flags = "01";

std::string header = ctx.to_w3c_traceparent();
// "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01"
```

### Parsing

```cpp
// Parse from W3C traceparent header
auto result = trace_context::from_w3c_traceparent(
    "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01"
);

if (result.is_ok()) {
    auto ctx = result.value();
    // ctx.trace_id == "4bf92f3577b34da6a3ce929d0e0e4736"
    // ctx.span_id  == "00f067aa0ba902b7"
    // ctx.trace_flags == "01"
}
```

### Context Injection and Extraction

The `distributed_tracer` provides template methods for injecting into and extracting from arbitrary carriers (any type supporting `operator[]` and `find()`):

**Injection** — Add trace context to outgoing request headers:

```cpp
distributed_tracer tracer;

// Start a span
auto span_result = tracer.start_span("outgoing_request", "order-service");
auto span = span_result.value();

// Extract context from span
auto ctx = tracer.extract_context(*span);

// Inject into HTTP headers (or any map-like carrier)
std::unordered_map<std::string, std::string> headers;
tracer.inject_context(ctx, headers);

// headers now contains:
// "traceparent"    → "00-{trace_id}-{span_id}-{flags}"
// "tracestate"     → vendor-specific state (if set)
// "baggage-{key}"  → baggage values (if any)
```

**Extraction** — Read trace context from incoming request headers:

```cpp
// Incoming HTTP request headers
std::unordered_map<std::string, std::string> headers = {
    {"traceparent", "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01"},
    {"tracestate", "vendor=value"},
    {"baggage-user_id", "12345"}
};

// Extract context from carrier
auto ctx_result = tracer.extract_context_from_carrier(headers);
if (ctx_result.is_ok()) {
    auto ctx = ctx_result.value();
    // ctx.trace_id == "4bf92f3577b34da6a3ce929d0e0e4736"
    // ctx.baggage["user_id"] == "12345"

    // Create a span continuing the trace
    auto span = tracer.start_span_from_context(ctx, "handle_request");
}
```

### Baggage Propagation

Baggage items propagate across all services in the trace:

```cpp
// Service A: Set baggage
auto span = tracer.start_span("process_order").value();
span->baggage["user_id"] = "12345";
span->baggage["tenant"] = "acme-corp";

// When injected, these appear as:
// baggage-user_id: 12345
// baggage-tenant: acme-corp

// Service B: Read baggage (after extraction)
auto ctx = tracer.extract_context_from_carrier(incoming_headers).value();
std::string user_id = ctx.baggage["user_id"];   // "12345"
std::string tenant = ctx.baggage["tenant"];       // "acme-corp"
```

---

## 4. Creating and Managing Spans

### Starting a Root Span

```cpp
distributed_tracer tracer;

// Method 1: Direct API
auto result = tracer.start_span("process_payment", "payment-service");
if (result.is_ok()) {
    auto span = result.value();
    // span->trace_id is auto-generated
    // span->parent_span_id is empty (root span)
}
```

### Starting a Child Span

```cpp
// Create child span from parent
auto parent = tracer.start_span("handle_request", "api-gateway").value();
auto child = tracer.start_child_span(*parent, "validate_input").value();

// child->trace_id == parent->trace_id  (same trace)
// child->parent_span_id == parent->span_id
```

### Starting from Trace Context

For incoming requests from other services:

```cpp
// Extract context from incoming headers
auto ctx = tracer.extract_context_from_carrier(request.headers).value();

// Create span as a child of the remote span
auto span = tracer.start_span_from_context(ctx, "process_request").value();
// span->trace_id continues from remote trace
```

### Finishing Spans

```cpp
auto span = tracer.start_span("operation").value();

// ... do work ...

// Finish the span (records end_time and calculates duration)
auto result = tracer.finish_span(span);
// After finish: span->is_finished() == true
// span->duration is calculated automatically
```

### Span Builder (Fluent API)

For more control over span construction:

```cpp
span_builder builder;
auto span = builder
    .with_trace_id("custom-trace-id")          // Optional: custom trace ID
    .with_parent("parent-span-id")              // Optional: set parent
    .with_operation("database_query")            // Operation name
    .with_service("user-service")                // Service name
    .with_tag("db.type", "postgresql")           // Add tags
    .with_tag("db.statement", "SELECT * FROM users")
    .with_baggage("tenant", "acme-corp")         // Add baggage
    .build();

// span_id and trace_id auto-generated if not explicitly set
// start_time set to now() on build()
```

### Scoped Spans (RAII)

Automatic span lifecycle management with `scoped_span`:

```cpp
{
    auto span_result = tracer.start_span("operation");
    scoped_span scope(span_result.value(), &tracer);

    // Access span through scope
    scope->tags["key"] = "value";
    scope->status = trace_span::status_code::ok;

    // span automatically finished when scope exits
}
// span is now finished, exported
```

### TRACE_SPAN Macro

For the simplest instrumentation:

```cpp
void process_request() {
    TRACE_SPAN("process_request");
    // Span started, attached to global tracer

    // ... do work ...

    // Span automatically finished when function returns
}

void nested_work(const trace_span& parent) {
    TRACE_CHILD_SPAN(parent, "nested_work");
    // Child span under parent

    // ... do work ...
}
```

### Adding Tags and Metadata

```cpp
auto span = tracer.start_span("http_request").value();

// Semantic conventions (recommended tag keys)
span->tags["http.method"] = "GET";
span->tags["http.url"] = "https://api.example.com/users";
span->tags["http.status_code"] = "200";
span->tags["span.kind"] = "client";    // client, server, producer, consumer
span->tags["error"] = "false";

// Custom tags
span->tags["user.id"] = "12345";
span->tags["order.total"] = "99.99";
```

### Recording Errors

```cpp
auto span = tracer.start_span("operation").value();

try {
    // ... operation that may fail ...
} catch (const std::exception& e) {
    span->status = trace_span::status_code::error;
    span->status_message = e.what();
    span->tags["error"] = "true";
    span->tags["error.message"] = e.what();
    span->tags["error.type"] = "std::runtime_error";
}

tracer.finish_span(span);
```

### Thread-Local Active Span

```cpp
// Set the active span for the current thread
tracer.set_current_span(span);

// Later, retrieve the active span (from any code on same thread)
auto active = tracer.get_current_span();
if (active) {
    // Create child spans from the active span
    auto child = tracer.start_child_span(*active, "sub_operation");
}
```

---

## 5. Backend Integration

### Export Configuration

All exporters share a common configuration:

```cpp
struct trace_export_config {
    std::string endpoint;                     // Backend URL
    trace_export_format format;               // Wire format
    std::chrono::milliseconds timeout{30000}; // Request timeout
    std::chrono::milliseconds batch_timeout{5000}; // Batch export timeout
    std::size_t max_batch_size = 512;         // Spans per batch
    std::size_t max_queue_size = 2048;        // Max queued spans
    bool enable_compression = true;           // Enable compression
    std::unordered_map<std::string, std::string> headers; // Custom headers
    std::optional<std::string> service_name;  // Override service name
};
```

### Supported Formats

| Format | Enum | Backend |
|--------|------|---------|
| Jaeger Thrift (HTTP) | `jaeger_thrift` | Jaeger |
| Jaeger gRPC | `jaeger_grpc` | Jaeger |
| Zipkin JSON v2 | `zipkin_json` | Zipkin |
| Zipkin Protobuf | `zipkin_protobuf` | Zipkin |
| OTLP gRPC | `otlp_grpc` | OTLP Collector |
| OTLP HTTP JSON | `otlp_http_json` | OTLP Collector |
| OTLP HTTP Protobuf | `otlp_http_protobuf` | OTLP Collector |

### 5.1 Jaeger Backend

Jaeger supports both Thrift over HTTP and gRPC protocols.

**Thrift over HTTP** (default Jaeger endpoint `/api/traces`):

```cpp
// Using helper function
auto exporter = create_jaeger_exporter(
    "http://jaeger-collector:14268",
    trace_export_format::jaeger_thrift
);

// Using full configuration
trace_export_config config;
config.endpoint = "http://jaeger-collector:14268";
config.format = trace_export_format::jaeger_thrift;
config.timeout = std::chrono::seconds(10);
config.service_name = "my-service";
config.headers["Authorization"] = "Bearer <token>";

auto exporter = std::make_unique<jaeger_exporter>(config);
```

**gRPC** (Jaeger gRPC endpoint):

```cpp
auto exporter = create_jaeger_exporter(
    "jaeger-collector:14250",
    trace_export_format::jaeger_grpc
);
```

**Jaeger span format**: The exporter converts `trace_span` to `jaeger_span_data` which includes:
- Trace/span/parent IDs mapped to Jaeger's numeric format
- Operation name, service name
- Timestamps in microseconds since epoch
- Tags as key-value string pairs
- Process tags (including `service.name`)

**Default Jaeger ports**:

| Port | Protocol | Purpose |
|------|----------|---------|
| 14268 | HTTP | Thrift collector endpoint |
| 14250 | gRPC | gRPC collector endpoint |
| 16686 | HTTP | Query/UI frontend |
| 6831 | UDP | Thrift compact (agent) |

### 5.2 Zipkin Backend

Zipkin supports JSON v2 and Protocol Buffers formats.

**JSON v2** (default Zipkin endpoint `/api/v2/spans`):

```cpp
// Using helper function
auto exporter = create_zipkin_exporter(
    "http://zipkin-server:9411",
    trace_export_format::zipkin_json
);

// Using full configuration
trace_export_config config;
config.endpoint = "http://zipkin-server:9411";
config.format = trace_export_format::zipkin_json;
config.timeout = std::chrono::seconds(10);
config.service_name = "my-service";

auto exporter = std::make_unique<zipkin_exporter>(config);
```

**Zipkin span format**: The exporter converts `trace_span` to `zipkin_span_data` which includes:
- Trace/span/parent IDs as hex strings
- Span kind: `CLIENT`, `SERVER`, `PRODUCER`, `CONSUMER`, `INTERNAL`
- Local and remote endpoints with service names
- Timestamps in microseconds since epoch
- Tags as string key-value map
- `shared` flag for server-side of shared span

**Span kind detection**: The Zipkin exporter reads the `span.kind` tag to determine Zipkin's span kind. If not set, defaults to `INTERNAL`.

**Default Zipkin port**: `9411` (HTTP API)

### 5.3 OTLP Backend

OTLP (OpenTelemetry Protocol) supports gRPC, HTTP+JSON, and HTTP+Protobuf:

```cpp
// Create resource for OTLP export
auto resource = create_service_resource("my-service", "1.0.0");

// Using helper function
auto exporter = create_otlp_exporter(
    "otel-collector:4317",  // gRPC endpoint
    resource,
    trace_export_format::otlp_grpc
);

// HTTP JSON variant
trace_export_config config;
config.endpoint = "http://otel-collector:4318";
config.format = trace_export_format::otlp_http_json;
auto exporter = std::make_unique<otlp_exporter>(config, resource);
```

**OTLP export flow**: The OTLP exporter uses the `opentelemetry_tracer_adapter` to convert spans to OTel format before sending:

```
trace_span → opentelemetry_tracer_adapter::convert_spans() → otel_span_data → send
```

**Default OTLP ports**:

| Port | Protocol | Purpose |
|------|----------|---------|
| 4317 | gRPC | OTLP gRPC receiver |
| 4318 | HTTP | OTLP HTTP receiver |

### 5.4 Exporter Factory

Use the factory for dynamic exporter creation:

```cpp
trace_export_config config;
config.endpoint = "http://jaeger:14268";
config.format = trace_export_format::jaeger_thrift;

auto resource = create_service_resource("my-service", "1.0.0");
auto exporter = trace_exporter_factory::create_exporter(config, resource);

// Query supported formats for a backend
auto jaeger_formats = trace_exporter_factory::get_supported_formats("jaeger");
// → {jaeger_thrift, jaeger_grpc}

auto zipkin_formats = trace_exporter_factory::get_supported_formats("zipkin");
// → {zipkin_json, zipkin_protobuf}

auto otlp_formats = trace_exporter_factory::get_supported_formats("otlp");
// → {otlp_grpc, otlp_http_json, otlp_http_protobuf}
```

### Connecting Exporter to Tracer

```cpp
distributed_tracer tracer;

// Create and set exporter
auto exporter = create_jaeger_exporter("http://jaeger:14268");
tracer.set_exporter(std::move(exporter));

// Configure export settings
trace_export_settings settings;
settings.batch_size = 100;
settings.flush_interval = std::chrono::seconds(5);
settings.max_queue_size = 2048;
settings.export_on_finish = true;
tracer.configure_export(settings);
```

### Retry Behavior

Both Jaeger and Zipkin exporters implement exponential backoff retry:

```
Attempt 1: immediate
Attempt 2: 100ms delay
Attempt 3: 200ms delay  (base_retry_delay * 2^attempt)
```

- **Max retries**: 3 (default)
- **Retryable errors**: HTTP 5xx (server errors)
- **Non-retryable errors**: HTTP 4xx (client errors)

---

## 6. Cross-Service Tracing

### Pattern: HTTP Request Propagation

```
┌──────────────┐         ┌──────────────┐         ┌──────────────┐
│  Service A   │  HTTP   │  Service B   │  HTTP   │  Service C   │
│              │────────▶│              │────────▶│              │
│  trace_id:T  │ headers │  trace_id:T  │ headers │  trace_id:T  │
│  span_id:S1  │         │  span_id:S2  │         │  span_id:S3  │
│              │         │  parent:S1   │         │  parent:S2   │
└──────────────┘         └──────────────┘         └──────────────┘
```

**Service A** (initiating service):

```cpp
distributed_tracer tracer;

// Start a root span
auto span = tracer.start_span("GET /api/orders", "order-service").value();
span->tags["http.method"] = "GET";
span->tags["span.kind"] = "client";

// Extract context for propagation
auto ctx = tracer.extract_context(*span);

// Inject into outgoing HTTP headers
std::unordered_map<std::string, std::string> headers;
tracer.inject_context(ctx, headers);

// headers["traceparent"] = "00-{trace_id}-{span_id}-01"
// Send HTTP request with these headers to Service B
http_client.send("http://service-b/process", headers);

tracer.finish_span(span);
```

**Service B** (receiving service):

```cpp
distributed_tracer tracer;

// Extract context from incoming request headers
auto ctx_result = tracer.extract_context_from_carrier(request.headers);
if (ctx_result.is_ok()) {
    auto ctx = ctx_result.value();

    // Continue the trace with a new span
    auto span = tracer.start_span_from_context(ctx, "process_order").value();
    span->tags["span.kind"] = "server";
    span->service_name = "inventory-service";

    // Create child span for downstream call
    auto child = tracer.start_child_span(*span, "check_inventory").value();
    child->tags["span.kind"] = "client";

    // Propagate to Service C
    auto child_ctx = tracer.extract_context(*child);
    std::unordered_map<std::string, std::string> downstream_headers;
    tracer.inject_context(child_ctx, downstream_headers);
    // Send to Service C with downstream_headers

    tracer.finish_span(child);
    tracer.finish_span(span);
}
```

### Thread Context for In-Process Correlation

The `thread_context` provides thread-local storage for trace correlation within a service:

```cpp
// Set up thread context with trace information
auto& ctx = thread_context::create("req-12345");
ctx.correlation_id = thread_context_manager::generate_correlation_id();
ctx.trace_id = span->trace_id;
ctx.span_id = span->span_id;
ctx.add_tag("service", "order-service");

// Later, anywhere on the same thread:
if (thread_context::has_context()) {
    auto* current = thread_context::current();
    std::string trace = current->trace_id;  // Access trace ID
    std::string correlation = current->correlation_id;

    // Use in log messages for correlation
    logger.info("Processing order, trace_id={}, correlation={}",
                trace, correlation);
}

// Clean up when request is complete
thread_context::clear();
```

### Cross-Thread Context Propagation

```cpp
// Main thread: capture context
auto* parent_ctx = thread_context::current();
thread_context_data captured = *parent_ctx;  // Copy context

// Spawn worker thread with captured context
std::thread worker([captured, &tracer]() {
    // Restore context in worker thread
    thread_context::copy_from(captured);

    // Create child span in worker
    auto* ctx = thread_context::current();
    auto child = tracer.start_child_span(/* parent from ctx */, "async_work");

    // ... do work ...

    thread_context::clear();
});
```

---

## 7. OpenTelemetry Compatibility

### Resource Model

OpenTelemetry resources describe the entity producing telemetry:

```cpp
// Create service resource with standard attributes
auto resource = create_service_resource(
    "payment-service",  // service.name
    "2.1.0",            // service.version
    "payments"          // service.namespace (optional)
);

// resource automatically includes:
// - service.name = "payment-service"
// - service.version = "2.1.0"
// - service.namespace = "payments"
// - telemetry.sdk.name = "monitoring_system"
// - telemetry.sdk.version = "0.5.0"
// - telemetry.sdk.language = "cpp"
```

### Span Kind

OTel span kinds classify the relationship between spans:

| Kind | Enum | When to Use |
|------|------|-------------|
| Internal | `otel_span_kind::internal` | Default — operations within a service |
| Server | `otel_span_kind::server` | Handling an incoming request |
| Client | `otel_span_kind::client` | Making an outgoing request |
| Producer | `otel_span_kind::producer` | Initiating async message |
| Consumer | `otel_span_kind::consumer` | Processing async message |

### Span Conversion

The `opentelemetry_tracer_adapter` converts internal spans to OTel format:

```cpp
auto resource = create_service_resource("my-service", "1.0.0");
opentelemetry_tracer_adapter adapter(resource);

// Convert a single span
trace_span internal_span;
internal_span.trace_id = "abc123";
internal_span.operation_name = "process";
internal_span.tags["span.kind"] = "server";
internal_span.tags["error"] = "true";
internal_span.tags["error.message"] = "Connection refused";

auto result = adapter.convert_span(internal_span);
if (result.is_ok()) {
    auto otel_span = result.value();
    // otel_span.kind == otel_span_kind::server
    // otel_span.status_code == otel_status_code::error
    // otel_span.status_message == "Connection refused"
    // Custom tags preserved as attributes (excluding special fields)
}

// Convert batch of spans
std::vector<trace_span> spans = { ... };
auto batch_result = adapter.convert_spans(spans);
```

### Compatibility Layer

For full OpenTelemetry integration, use the compatibility layer:

```cpp
auto compat = create_opentelemetry_compatibility_layer("my-service", "1.0.0");
compat->initialize();

// Export spans through OTel pipeline
std::vector<trace_span> spans = collect_spans();
compat->export_spans(spans);

// Export metrics through OTel pipeline
monitoring_data data = collect_metrics();
compat->export_metrics(data);

// Flush pending data
compat->flush();

// Get statistics
auto stats = compat->get_stats();
// stats.pending_spans, stats.pending_metrics, etc.

// Clean shutdown
compat->shutdown();
```

---

## 8. Transport Layer

### HTTP Transport

The `http_transport` abstraction provides pluggable HTTP clients:

```cpp
// Production: auto-detect best transport
auto transport = create_default_transport();
// Returns network_http_transport (if MONITORING_HAS_NETWORK_SYSTEM) or simple_http_client

// Testing: use stub transport
auto stub = create_stub_transport();
stub->set_simulate_success(true);  // or false for failure testing
stub->set_response_handler([](const http_request& req) {
    http_response response;
    response.status_code = 200;
    return response;
});
```

**Transport implementations**:

| Transport | Compile Flag | Description |
|-----------|-------------|-------------|
| `simple_http_client` | Always available | Basic HTTP (stub for development) |
| `network_http_transport` | `MONITORING_HAS_NETWORK_SYSTEM` | Real HTTP via network_system |
| `stub_http_transport` | Always available | Configurable mock for testing |

### gRPC Transport

For OTLP gRPC and Jaeger gRPC protocols:

```cpp
// Production: auto-detect best transport
auto transport = create_default_grpc_transport();
// Returns network_grpc_transport (if MONITORING_HAS_GRPC) or stub_grpc_transport

// Connect to server
auto result = transport->connect("otel-collector", 4317);
if (result.is_ok()) {
    // Send gRPC request
    grpc_request request;
    request.service = "opentelemetry.proto.collector.trace.v1.TraceService";
    request.method = "Export";
    request.body = serialize_traces(spans);
    request.timeout = std::chrono::seconds(10);

    auto response = transport->send(request);
}

// Get statistics
auto stats = transport->get_statistics();
// stats.requests_sent, stats.bytes_sent, stats.send_failures
```

**gRPC Transport implementations**:

| Transport | Compile Flag | Description |
|-----------|-------------|-------------|
| `stub_grpc_transport` | Always available | Mock for testing |
| `network_grpc_transport` | `MONITORING_HAS_GRPC` | Real gRPC with channel pooling |

### gRPC Channel Management

When `MONITORING_HAS_GRPC` is defined, the `grpc_channel_manager` provides connection pooling:

```cpp
grpc_channel_config config;
config.target = "otel-collector:4317";
config.use_tls = true;
config.root_certificates = load_file("ca-cert.pem");
config.connect_timeout = std::chrono::seconds(5);
config.keepalive_time = std::chrono::seconds(10);
config.enable_retry = true;

grpc_channel_manager manager(config);

// Get or create channel (pooled)
auto channel = manager.get_channel("otel-collector:4317");
// Same target returns cached channel

// Health check
bool healthy = grpc_health_check("otel-collector:4317", config);
```

### Custom Transport Injection

Exporters accept custom transports for testability:

```cpp
// Create exporter with custom transport
auto transport = std::make_unique<stub_http_transport>();
transport->set_response_handler([](const http_request& req) {
    // Verify request format in tests
    assert(req.headers.at("Content-Type") == "application/json");
    http_response response;
    response.status_code = 202;
    return response;
});

trace_export_config config;
config.endpoint = "http://test-backend";
config.format = trace_export_format::zipkin_json;

zipkin_exporter exporter(config, std::move(transport));
```

---

## 9. Production Examples

### Example 1: Microservice Request Tracing

Complete request tracing across three microservices:

```cpp
#include <kcenon/monitoring/tracing/distributed_tracer.h>
#include <kcenon/monitoring/exporters/trace_exporters.h>

// --- Setup (application startup) ---
void setup_tracing() {
    auto& tracer = global_tracer();

    // Configure Jaeger exporter
    trace_export_config config;
    config.endpoint = "http://jaeger-collector:14268";
    config.format = trace_export_format::jaeger_thrift;
    config.service_name = "api-gateway";
    config.max_batch_size = 256;

    auto exporter = std::make_unique<jaeger_exporter>(config);
    tracer.set_exporter(std::move(exporter));

    trace_export_settings settings;
    settings.batch_size = 100;
    settings.flush_interval = std::chrono::seconds(5);
    tracer.configure_export(settings);
}

// --- API Gateway handler ---
void handle_order_request(const http_request& request, http_response& response) {
    auto& tracer = global_tracer();

    // Extract context from incoming request (if any)
    std::shared_ptr<trace_span> span;
    auto ctx_result = tracer.extract_context_from_carrier(request.headers);
    if (ctx_result.is_ok()) {
        span = tracer.start_span_from_context(ctx_result.value(), "handle_order").value();
    } else {
        span = tracer.start_span("handle_order", "api-gateway").value();
    }

    span->tags["http.method"] = "POST";
    span->tags["http.url"] = "/api/orders";
    span->tags["span.kind"] = "server";

    // Validate request
    {
        auto validate_span = tracer.start_child_span(*span, "validate_request").value();
        bool valid = validate_order(request);
        validate_span->status = valid ?
            trace_span::status_code::ok : trace_span::status_code::error;
        tracer.finish_span(validate_span);

        if (!valid) {
            span->status = trace_span::status_code::error;
            span->status_message = "Invalid order request";
            span->tags["http.status_code"] = "400";
            tracer.finish_span(span);
            response.status = 400;
            return;
        }
    }

    // Call downstream order service
    {
        auto call_span = tracer.start_child_span(*span, "call_order_service").value();
        call_span->tags["span.kind"] = "client";
        call_span->tags["peer.service"] = "order-service";

        // Propagate context
        auto ctx = tracer.extract_context(*call_span);
        std::unordered_map<std::string, std::string> downstream_headers;
        tracer.inject_context(ctx, downstream_headers);

        auto order_result = http_client.post("http://order-service/create",
                                             request.body, downstream_headers);

        call_span->tags["http.status_code"] = std::to_string(order_result.status);
        call_span->status = (order_result.status == 200) ?
            trace_span::status_code::ok : trace_span::status_code::error;
        tracer.finish_span(call_span);
    }

    span->tags["http.status_code"] = "200";
    span->status = trace_span::status_code::ok;
    tracer.finish_span(span);
    response.status = 200;
}
```

### Example 2: Database Operation Tracing

```cpp
#include <kcenon/monitoring/tracing/distributed_tracer.h>

class traced_database {
    distributed_tracer& tracer_;
    std::string service_name_;

public:
    traced_database(distributed_tracer& tracer, const std::string& service)
        : tracer_(tracer), service_name_(service) {}

    std::vector<User> find_users(const std::string& query) {
        auto span = tracer_.start_span("db.query", service_name_).value();
        span->tags["db.type"] = "postgresql";
        span->tags["db.statement"] = query;
        span->tags["db.name"] = "users_db";
        span->tags["span.kind"] = "client";

        try {
            auto results = execute_query(query);
            span->tags["db.rows_affected"] = std::to_string(results.size());
            span->status = trace_span::status_code::ok;
            tracer_.finish_span(span);
            return results;
        } catch (const std::exception& e) {
            span->status = trace_span::status_code::error;
            span->status_message = e.what();
            span->tags["error"] = "true";
            span->tags["error.message"] = e.what();
            tracer_.finish_span(span);
            throw;
        }
    }
};
```

### Example 3: Multi-Backend Export

Send traces to multiple backends simultaneously:

```cpp
#include <kcenon/monitoring/exporters/trace_exporters.h>

void setup_multi_backend_tracing() {
    auto& tracer = global_tracer();

    // Create Jaeger exporter for detailed development tracing
    trace_export_config jaeger_config;
    jaeger_config.endpoint = "http://jaeger:14268";
    jaeger_config.format = trace_export_format::jaeger_thrift;
    jaeger_config.service_name = "payment-service";

    auto jaeger = std::make_unique<jaeger_exporter>(jaeger_config);

    // Create OTLP exporter for production observability platform
    auto resource = create_service_resource("payment-service", "2.0.0", "payments");

    trace_export_config otlp_config;
    otlp_config.endpoint = "otel-collector:4317";
    otlp_config.format = trace_export_format::otlp_grpc;
    otlp_config.enable_compression = true;
    otlp_config.headers["Authorization"] = "Bearer " + get_auth_token();

    auto otlp = std::make_unique<otlp_exporter>(otlp_config, resource);

    // Export spans to both backends
    std::vector<trace_span> spans = { /* collected spans */ };
    jaeger->export_spans(spans);
    otlp->export_spans(spans);

    // Check export statistics
    auto jaeger_stats = jaeger->get_stats();
    auto otlp_stats = otlp->get_stats();
    // stats["exported_spans"], stats["failed_exports"], stats["dropped_spans"]
}
```

### Example 4: Async Message Queue Tracing

```cpp
#include <kcenon/monitoring/tracing/distributed_tracer.h>

// Producer side
void publish_order_event(const Order& order) {
    auto& tracer = global_tracer();
    auto span = tracer.start_span("publish_order", "order-service").value();
    span->tags["span.kind"] = "producer";
    span->tags["messaging.system"] = "rabbitmq";
    span->tags["messaging.destination"] = "orders.created";

    // Inject trace context into message headers
    auto ctx = tracer.extract_context(*span);
    std::unordered_map<std::string, std::string> msg_headers;
    tracer.inject_context(ctx, msg_headers);

    // Publish message with trace headers
    message_queue.publish("orders.created", serialize(order), msg_headers);

    span->status = trace_span::status_code::ok;
    tracer.finish_span(span);
}

// Consumer side
void handle_order_event(const Message& message) {
    auto& tracer = global_tracer();

    // Extract trace context from message headers
    auto ctx_result = tracer.extract_context_from_carrier(message.headers);

    std::shared_ptr<trace_span> span;
    if (ctx_result.is_ok()) {
        span = tracer.start_span_from_context(
            ctx_result.value(), "process_order_event").value();
    } else {
        span = tracer.start_span("process_order_event", "fulfillment-service").value();
    }

    span->tags["span.kind"] = "consumer";
    span->tags["messaging.system"] = "rabbitmq";
    span->tags["messaging.destination"] = "orders.created";
    span->tags["messaging.operation"] = "process";

    try {
        auto order = deserialize<Order>(message.body);
        process_order(order);
        span->status = trace_span::status_code::ok;
    } catch (const std::exception& e) {
        span->status = trace_span::status_code::error;
        span->status_message = e.what();
    }

    tracer.finish_span(span);
}
```

---

## 10. Performance Considerations

### Memory Usage

| Component | Memory Per Instance |
|-----------|-------------------|
| `trace_span` | ~320 bytes + tag/baggage strings |
| `trace_context` | ~160 bytes + baggage |
| `thread_context_data` | ~200 bytes + tags |
| `otel_span_data` | ~400 bytes + attributes |
| Exporter queue (per span) | ~16 bytes pointer |

### Export Settings by Scale

| Scale | batch_size | flush_interval | max_queue_size | Rationale |
|-------|-----------|----------------|----------------|-----------|
| Development | 1 | 1s | 100 | Immediate visibility |
| Small (< 100 rps) | 50 | 5s | 1024 | Low overhead |
| Medium (100-1k rps) | 256 | 5s | 2048 | Balanced batching |
| Large (1k+ rps) | 512 | 10s | 8192 | Maximize throughput |

### Overhead Minimization

1. **Use scoped spans**: RAII ensures spans are always finished, preventing memory leaks
2. **Batch exports**: Configure appropriate `batch_size` to amortize network overhead
3. **Limit tags**: Each tag adds string allocation overhead; keep to essential metadata
4. **Limit baggage**: Baggage propagates to every downstream service — keep it minimal
5. **Use compression**: Enable `enable_compression` for large batches

### Threading Model

| Component | Thread Safety |
|-----------|--------------|
| `distributed_tracer` | Thread-safe (internal pimpl with mutex) |
| `trace_span` | Not thread-safe — use one span per thread |
| `thread_context` | Thread-local — inherently per-thread |
| Exporters | Thread-safe (atomic counters + synchronized transport) |
| `scoped_span` | Not movable across threads |

---

## 11. API Reference

### Core Tracing (`distributed_tracer.h`)

| Type/Function | Purpose |
|---------------|---------|
| `trace_span` | Span data with trace/span IDs, timing, tags, baggage, status |
| `trace_context` | W3C-compatible propagation context |
| `span_builder` | Fluent API for span construction |
| `distributed_tracer` | Central tracer: start/finish spans, inject/extract context |
| `scoped_span` | RAII span lifecycle manager |
| `global_tracer()` | Access singleton tracer instance |
| `TRACE_SPAN(name)` | Macro for automatic scoped span |
| `TRACE_CHILD_SPAN(parent, name)` | Macro for automatic scoped child span |

### Context (`trace_context.h`, `thread_context.h`)

| Type/Function | Purpose |
|---------------|---------|
| `trace_context` (public) | Lightweight root/child context creation |
| `context_metadata` | Request/correlation IDs and tags |
| `thread_context_data` | Thread-local trace/span/request data |
| `thread_context` | Static thread-local context management |
| `thread_context_manager` | Legacy compatibility context manager |

### Exporters (`trace_exporters.h`)

| Type/Function | Purpose |
|---------------|---------|
| `trace_export_format` | Enum of 7 supported formats |
| `trace_export_config` | Common exporter configuration |
| `trace_exporter_interface` | Abstract base: `export_spans`, `flush`, `shutdown` |
| `jaeger_exporter` | Jaeger backend (Thrift HTTP + gRPC) |
| `zipkin_exporter` | Zipkin backend (JSON v2 + Protobuf) |
| `otlp_exporter` | OTLP backend (gRPC + HTTP JSON + HTTP Protobuf) |
| `trace_exporter_factory` | Factory for creating exporters by format |
| `create_jaeger_exporter()` | Helper to create Jaeger exporter |
| `create_zipkin_exporter()` | Helper to create Zipkin exporter |
| `create_otlp_exporter()` | Helper to create OTLP exporter |

### OpenTelemetry Adapter (`opentelemetry_adapter.h`)

| Type/Function | Purpose |
|---------------|---------|
| `otel_resource` | OTel resource with typed attributes |
| `otel_span_kind` | Span kinds: internal, server, client, producer, consumer |
| `otel_span_data` | OTel-format span data |
| `otel_metric_data` | OTel-format metric data |
| `opentelemetry_tracer_adapter` | Converts `trace_span` → `otel_span_data` |
| `opentelemetry_metrics_adapter` | Converts metrics to OTel format |
| `opentelemetry_compatibility_layer` | Full OTel-compatible export pipeline |
| `create_service_resource()` | Factory for OTel service resource |

### Transport (`http_transport.h`, `grpc_transport.h`)

| Type/Function | Purpose |
|---------------|---------|
| `http_transport` | Abstract HTTP client interface |
| `simple_http_client` | Basic HTTP (stub) |
| `stub_http_transport` | Configurable mock for testing |
| `network_http_transport` | Real HTTP via network_system |
| `grpc_transport` | Abstract gRPC client interface |
| `stub_grpc_transport` | Configurable mock for testing |
| `network_grpc_transport` | Real gRPC with channel pooling |
| `grpc_channel_manager` | gRPC channel connection pool |
| `create_default_transport()` | Auto-detect best HTTP transport |
| `create_default_grpc_transport()` | Auto-detect best gRPC transport |

---

## See Also

- [Stream Processing Guide](STREAM_PROCESSING.md) — Metrics aggregation framework
- [Advanced Alert Configuration Guide](ADVANCED_ALERTS.md) — Alert rules and notifications
- [Reliability Patterns Guide](RELIABILITY_PATTERNS.md) — Circuit breakers, retry strategies
- Source headers in `include/kcenon/monitoring/tracing/` and `include/kcenon/monitoring/exporters/`
