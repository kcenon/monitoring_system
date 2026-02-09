# Exporter Development Guide

> **Version**: 1.0.0
> **Last Updated**: 2026-02-09
> **Parent Epic**: [Documentation Gap Analysis](https://github.com/kcenon/common_system/issues/325)
> **Related Issue**: [#456](https://github.com/kcenon/monitoring_system/issues/456)

## Table of Contents

1. [Overview](#1-overview)
2. [Exporter Pipeline Architecture](#2-exporter-pipeline-architecture)
3. [Built-in Metric Exporters](#3-built-in-metric-exporters)
4. [Built-in Trace Exporters](#4-built-in-trace-exporters)
5. [Transport Layers](#5-transport-layers)
6. [Custom Exporter Implementation](#6-custom-exporter-implementation)
7. [OpenTelemetry Integration](#7-opentelemetry-integration)
8. [Configuration Examples](#8-configuration-examples)

---

## 1. Overview

The monitoring_system exporter subsystem provides a pluggable architecture for
sending collected metrics and traces to external observability backends. The
system separates **data formatting** (exporter) from **network delivery**
(transport), enabling flexible combinations of wire formats and protocols.

### Key Characteristics

| Aspect | Description |
|--------|-------------|
| **Dual pipeline** | Separate interfaces for metrics and traces |
| **Transport abstraction** | HTTP, gRPC, and UDP transports are decoupled from exporters |
| **Factory pattern** | Exporters are created via format-based factory functions |
| **Result-based errors** | All operations return `common::Result<T>` or `common::VoidResult` |
| **Conditional compilation** | Real network backends compile only when dependencies are available |
| **Test-friendly** | Every transport provides a stub implementation for unit testing |

### Source File Map

| File | Lines | Purpose |
|------|-------|---------|
| [`metric_exporters.h`](../../include/kcenon/monitoring/exporters/metric_exporters.h) | 1189 | Metric exporter interface, Prometheus/StatsD/OTLP implementations |
| [`trace_exporters.h`](../../include/kcenon/monitoring/exporters/trace_exporters.h) | 845 | Trace exporter interface, Jaeger/Zipkin/OTLP implementations |
| [`opentelemetry_adapter.h`](../../include/kcenon/monitoring/exporters/opentelemetry_adapter.h) | 569 | OpenTelemetry compatibility layer and data converters |
| [`otlp_grpc_exporter.h`](../../include/kcenon/monitoring/exporters/otlp_grpc_exporter.h) | 656 | Dedicated OTLP gRPC exporter with hand-rolled protobuf |
| [`http_transport.h`](../../include/kcenon/monitoring/exporters/http_transport.h) | 341 | HTTP transport abstraction |
| [`grpc_transport.h`](../../include/kcenon/monitoring/exporters/grpc_transport.h) | 607 | gRPC transport abstraction with channel pooling |
| [`udp_transport.h`](../../include/kcenon/monitoring/exporters/udp_transport.h) | 503 | UDP transport abstraction |

---

## 2. Exporter Pipeline Architecture

### 2.1 High-Level Data Flow

```
                    ┌─────────────────────────────────────────────────┐
                    │              Monitoring System                   │
                    │                                                  │
   Collectors ──►   │  monitoring_data / metrics_snapshot              │
                    │         │                    │                   │
                    │         ▼                    ▼                   │
                    │  ┌──────────────┐   ┌───────────────┐           │
                    │  │ Metric       │   │ Trace         │           │
                    │  │ Exporters    │   │ Exporters     │           │
                    │  └──────┬───────┘   └───────┬───────┘           │
                    │         │                    │                   │
                    │         ▼                    ▼                   │
                    │  ┌─────────────────────────────────────┐        │
                    │  │         Transport Layer              │        │
                    │  │  ┌─────┐  ┌──────┐  ┌─────┐        │        │
                    │  │  │HTTP │  │ gRPC │  │ UDP │        │        │
                    │  │  └──┬──┘  └──┬───┘  └──┬──┘        │        │
                    │  └─────┼────────┼─────────┼────────────┘        │
                    └────────┼────────┼─────────┼─────────────────────┘
                             │        │         │
                             ▼        ▼         ▼
                        Prometheus  Jaeger   StatsD
                        Zipkin      OTLP     DataDog
```

### 2.2 Metric Exporter Interface

All metric exporters implement `metric_exporter_interface`, defined in
[`metric_exporters.h:~170`](../../include/kcenon/monitoring/exporters/metric_exporters.h):

```cpp
class metric_exporter_interface {
public:
    virtual ~metric_exporter_interface() = default;

    // Core export methods
    virtual common::VoidResult export_metrics(const monitoring_data& data) = 0;
    virtual common::VoidResult export_snapshot(const metrics_snapshot& snapshot) = 0;

    // Lifecycle
    virtual common::VoidResult start() = 0;
    virtual common::VoidResult stop() = 0;
    virtual common::VoidResult flush() = 0;
    virtual common::VoidResult shutdown() = 0;

    // Introspection
    virtual export_statistics get_stats() const = 0;
    virtual std::string name() const = 0;
    virtual metric_export_format format() const = 0;
};
```

**Key methods**:
- `export_metrics()` — accepts raw `monitoring_data` from collectors
- `export_snapshot()` — accepts an aggregated `metrics_snapshot`
- `start()` / `stop()` — control the exporter lifecycle
- `flush()` — force pending data to be sent immediately
- `shutdown()` — graceful shutdown with final flush

### 2.3 Trace Exporter Interface

All trace exporters implement `trace_exporter_interface`, defined in
[`trace_exporters.h:~130`](../../include/kcenon/monitoring/exporters/trace_exporters.h):

```cpp
class trace_exporter_interface {
public:
    virtual ~trace_exporter_interface() = default;

    // Core export method
    virtual common::VoidResult export_spans(
        const std::vector<trace_span>& spans) = 0;

    // Lifecycle
    virtual common::VoidResult flush() = 0;
    virtual common::VoidResult shutdown() = 0;

    // Introspection
    virtual export_statistics get_stats() const = 0;
    virtual std::string name() const = 0;
    virtual trace_export_format format() const = 0;
};
```

**Key difference from metrics**: trace exporters receive a batch of `trace_span`
objects rather than snapshots, since traces are naturally event-oriented.

### 2.4 Format Enums

The system uses two format enums to select exporters:

```cpp
// Metric export formats
enum class metric_export_format {
    prometheus_text,        // Prometheus text exposition format
    prometheus_protobuf,    // Prometheus protobuf format
    statsd_plain,           // Plain StatsD format
    statsd_datadog,         // DataDog-extended StatsD format
    otlp_grpc,              // OTLP over gRPC
    otlp_http_json,         // OTLP over HTTP (JSON)
    otlp_http_protobuf      // OTLP over HTTP (protobuf)
};

// Trace export formats
enum class trace_export_format {
    jaeger_thrift,          // Jaeger Thrift over HTTP
    jaeger_grpc,            // Jaeger gRPC
    zipkin_json,            // Zipkin JSON v2
    zipkin_protobuf,        // Zipkin protobuf
    otlp_grpc,              // OTLP over gRPC
    otlp_http_json,         // OTLP over HTTP (JSON)
    otlp_http_protobuf      // OTLP over HTTP (protobuf)
};
```

### 2.5 Factory Pattern

Exporters are created through factory functions that map formats to
implementations:

```cpp
// Metric exporter factory
class metric_exporter_factory {
public:
    static std::unique_ptr<metric_exporter_interface> create_exporter(
        const metric_export_config& config);
};

// Trace exporter factory
class trace_exporter_factory {
public:
    static std::unique_ptr<trace_exporter_interface> create_exporter(
        const trace_export_config& config);
};
```

Both factories inspect the `format` field in the config struct and return the
appropriate exporter instance. Convenience helper functions are also provided:

```cpp
// Metric helpers
auto prom = create_prometheus_exporter(9090, "my_service");
auto statsd = create_statsd_exporter("localhost", 8125, /*datadog=*/false);
auto otlp_m = create_otlp_metrics_exporter("localhost:4317", resource);

// Trace helpers
auto jaeger = create_jaeger_exporter("localhost:14268", "my_service");
auto zipkin = create_zipkin_exporter("localhost:9411", "my_service");
auto otlp_t = create_otlp_exporter("localhost:4317", "my_service");
```

---

## 3. Built-in Metric Exporters

### 3.1 Prometheus Exporter

**Model**: Pull-based (HTTP scraping)
**File**: [`metric_exporters.h:~250`](../../include/kcenon/monitoring/exporters/metric_exporters.h)
**Transport**: HTTP (serves metrics endpoint for Prometheus to scrape)

The `prometheus_exporter` converts internal metrics to the Prometheus text
exposition format and serves them via an HTTP endpoint.

#### Wire Format

Each metric produces three lines:

```
# HELP metric_name Description text
# TYPE metric_name gauge
metric_name{label1="value1",label2="value2"} 42.5 1700000000000
```

#### Name Sanitization

Prometheus has strict naming rules. The exporter automatically sanitizes:

| Character | Replacement | Example |
|-----------|-------------|---------|
| `.` (dot) | `_` | `cpu.usage` → `cpu_usage` |
| `-` (dash) | `_` | `mem-used` → `mem_used` |
| Leading digit | Prefixed with `_` | `2xx_count` → `_2xx_count` |

Label names follow the same rules, and label values are escaped
(backslash, double-quote, newline).

#### Data Conversion

The exporter converts `prometheus_metric_data` from internal metrics:

```cpp
struct prometheus_metric_data {
    std::string name;
    std::string help;
    std::string type;   // "gauge", "counter", "histogram", "summary"
    std::vector<std::pair<std::string, std::string>> labels;
    double value;
    std::optional<int64_t> timestamp_ms;

    std::string to_prometheus_text() const;
};
```

#### Usage

```cpp
auto exporter = create_prometheus_exporter(
    9090,           // HTTP port
    "my_service"    // Job name
);
exporter->start();  // Starts HTTP server

// Metrics are accumulated internally
exporter->export_metrics(data);

// Prometheus scrapes GET /metrics → returns text exposition format
std::string text = exporter->get_metrics_text();
```

### 3.2 StatsD Exporter

**Model**: Push-based (fire-and-forget over UDP)
**File**: [`metric_exporters.h:~500`](../../include/kcenon/monitoring/exporters/metric_exporters.h)
**Transport**: UDP via `udp_transport`

The `statsd_exporter` pushes metrics over UDP in either plain StatsD or
DataDog-extended format.

#### Wire Formats

**Plain StatsD**:
```
metric.name:42|g|@1.0
```

**DataDog StatsD** (adds tags):
```
metric.name:42|g|@1.0|#tag1:value1,tag2:value2
```

Format components: `name:value|type|@sample_rate|#tags`

| Type Code | Meaning |
|-----------|---------|
| `c` | Counter |
| `g` | Gauge |
| `ms` | Timer / Duration |
| `h` | Histogram |
| `s` | Set |

#### Data Conversion

```cpp
struct statsd_metric_data {
    std::string name;
    double value;
    std::string type;       // "g", "c", "ms", "h", "s"
    double sample_rate{1.0};
    std::vector<std::pair<std::string, std::string>> tags;

    std::string to_statsd_format(bool datadog_format = false) const;
};
```

#### Batching and Auto-Connect

The exporter batches metrics and sends them as a single UDP packet using
newline separation. If not connected, it auto-connects on the first send:

```cpp
auto exporter = create_statsd_exporter(
    "localhost",  // StatsD host
    8125,         // StatsD port
    false         // Plain format (true for DataDog)
);
exporter->start();
exporter->export_metrics(data);  // Auto-connects, batches, sends UDP
```

### 3.3 OTLP Metrics Exporter

**Model**: Push-based
**File**: [`metric_exporters.h:~750`](../../include/kcenon/monitoring/exporters/metric_exporters.h)
**Transport**: HTTP (`/v1/metrics` endpoint) or gRPC (`MetricsService/Export`)

The `otlp_metrics_exporter` supports dual transport and uses the
`opentelemetry_metrics_adapter` to convert internal metrics to OTEL format.

#### Transport Selection

| Format | Transport | Endpoint |
|--------|-----------|----------|
| `otlp_grpc` | gRPC | `MetricsService/Export` |
| `otlp_http_json` | HTTP POST | `/v1/metrics` |
| `otlp_http_protobuf` | HTTP POST | `/v1/metrics` |

#### Usage

```cpp
auto resource = create_service_resource("my_service", "1.0.0");

auto exporter = create_otlp_metrics_exporter(
    "localhost:4317",        // OTLP endpoint
    resource,                // Service resource
    metric_export_format::otlp_grpc
);
exporter->start();
exporter->export_snapshot(snapshot);
```

---

## 4. Built-in Trace Exporters

### 4.1 Jaeger Exporter

**Model**: Push-based
**File**: [`trace_exporters.h:~200`](../../include/kcenon/monitoring/exporters/trace_exporters.h)
**Transport**: HTTP (Thrift JSON) or gRPC

The `jaeger_exporter` sends spans to Jaeger backends using either Thrift HTTP
or gRPC protocol.

#### Transport Selection

| Format | Transport | Endpoint |
|--------|-----------|----------|
| `jaeger_thrift` | HTTP POST | `/api/traces` |
| `jaeger_grpc` | gRPC | `jaeger.api_v2.CollectorService/PostSpans` |

#### Data Conversion

Spans are converted to `jaeger_span_data`:

```cpp
struct jaeger_span_data {
    std::string trace_id;
    std::string span_id;
    std::string parent_span_id;
    std::string operation_name;
    std::string service_name;
    int64_t start_time_us;   // Microseconds since epoch
    int64_t duration_us;
    std::vector<jaeger_tag> tags;
    std::vector<jaeger_log> logs;

    std::string to_thrift_json() const;
    std::vector<uint8_t> to_protobuf() const;
};
```

#### Retry with Exponential Backoff

Failed sends are retried with exponential backoff:

```cpp
// Internal retry logic
common::VoidResult send_with_retry(const std::vector<uint8_t>& data) {
    for (int attempt = 0; attempt < max_retries_; ++attempt) {
        auto result = transport_->send(request);
        if (result.is_ok()) return common::ok();

        auto delay = base_delay_ * std::pow(2, attempt);
        std::this_thread::sleep_for(delay);
    }
    return common::VoidResult::err(/* ... */);
}
```

#### Usage

```cpp
auto exporter = create_jaeger_exporter(
    "localhost:14268",   // Jaeger collector endpoint
    "my_service"         // Service name
);
exporter->export_spans(spans);
```

### 4.2 Zipkin Exporter

**Model**: Push-based
**File**: [`trace_exporters.h:~450`](../../include/kcenon/monitoring/exporters/trace_exporters.h)
**Transport**: HTTP POST

The `zipkin_exporter` sends spans to Zipkin in JSON v2 or protobuf format.

#### Transport Selection

| Format | Content-Type | Endpoint |
|--------|-------------|----------|
| `zipkin_json` | `application/json` | `/api/v2/spans` |
| `zipkin_protobuf` | `application/x-protobuf` | `/api/v2/spans` |

#### Data Conversion

Spans are converted to `zipkin_span_data`:

```cpp
struct zipkin_span_data {
    std::string trace_id;      // 16 or 32 hex characters
    std::string id;            // 16 hex characters
    std::string parent_id;
    std::string name;
    std::string kind;          // "CLIENT", "SERVER", "PRODUCER", "CONSUMER"
    int64_t timestamp_us;
    int64_t duration_us;
    zipkin_endpoint local_endpoint;
    zipkin_endpoint remote_endpoint;
    std::map<std::string, std::string> tags;
    std::vector<zipkin_annotation> annotations;

    std::string to_json_v2() const;
    std::vector<uint8_t> to_protobuf() const;
};
```

#### Usage

```cpp
auto exporter = create_zipkin_exporter(
    "localhost:9411",    // Zipkin endpoint
    "my_service"         // Service name
);
exporter->export_spans(spans);
```

### 4.3 OTLP Trace Exporter

**Model**: Push-based
**File**: [`trace_exporters.h:~650`](../../include/kcenon/monitoring/exporters/trace_exporters.h)
**Transport**: gRPC or HTTP

The `otlp_exporter` supports all three OTLP transport variants for traces.

| Format | Transport | Endpoint |
|--------|-----------|----------|
| `otlp_grpc` | gRPC | `TraceService/Export` |
| `otlp_http_json` | HTTP POST | `/v1/traces` |
| `otlp_http_protobuf` | HTTP POST | `/v1/traces` |

```cpp
auto exporter = create_otlp_exporter(
    "localhost:4317",    // OTLP endpoint
    "my_service"         // Service name
);
exporter->export_spans(spans);
```

### 4.4 OTLP gRPC Exporter (Dedicated)

**Model**: Push-based with full gRPC support
**File**: [`otlp_grpc_exporter.h`](../../include/kcenon/monitoring/exporters/otlp_grpc_exporter.h)
**Transport**: gRPC with retry, TLS, batching

The `otlp_grpc_exporter` is a dedicated, full-featured OTLP gRPC
implementation with hand-rolled protobuf serialization.

#### Configuration

```cpp
struct otlp_grpc_config {
    std::string endpoint{"localhost:4317"};
    std::chrono::milliseconds timeout{10000};
    std::size_t max_batch_size{512};
    std::size_t max_queue_size{2048};
    std::chrono::milliseconds batch_timeout{5000};

    // Retry configuration
    int max_retries{3};
    std::chrono::milliseconds initial_backoff{1000};
    std::chrono::milliseconds max_backoff{30000};
    double backoff_multiplier{2.0};

    // TLS configuration
    bool use_tls{false};
    std::string root_certificates;
    std::string client_certificate;
    std::string client_key;

    // Custom headers (e.g., authentication tokens)
    std::unordered_map<std::string, std::string> headers;

    // Resource attributes
    std::unordered_map<std::string, std::string> resource_attributes;
};
```

#### Hand-Rolled Protobuf Serialization

The `otlp_span_converter` implements OTLP protobuf serialization without
requiring the protobuf library, using manual wire format encoding:

```cpp
class otlp_span_converter {
public:
    // Convert spans to OTLP ExportTraceServiceRequest bytes
    std::vector<uint8_t> convert_to_otlp(
        const std::vector<trace_span>& spans,
        const std::unordered_map<std::string, std::string>& resource_attrs);

private:
    // Protobuf wire format helpers
    void write_varint(std::vector<uint8_t>& buffer, uint64_t value);
    void write_fixed64(std::vector<uint8_t>& buffer, uint64_t value);
    void write_length_delimited(std::vector<uint8_t>& buffer,
                                 uint32_t field_number,
                                 const std::vector<uint8_t>& data);
    std::vector<uint8_t> hex_to_bytes(const std::string& hex);
};
```

#### Retryable gRPC Status Codes

The exporter retries on transient gRPC failures:

| Status Code | Name | Retry? |
|-------------|------|--------|
| 0 | OK | No (success) |
| 1 | CANCELLED | Yes |
| 4 | DEADLINE_EXCEEDED | Yes |
| 8 | RESOURCE_EXHAUSTED | Yes |
| 10 | ABORTED | Yes |
| 14 | UNAVAILABLE | Yes |
| Others | Various | No (permanent failure) |

#### Export Statistics

```cpp
struct otlp_exporter_stats {
    std::size_t spans_exported{0};
    std::size_t spans_dropped{0};
    std::size_t export_failures{0};
    std::size_t retries{0};
    std::chrono::milliseconds total_export_time{0};
    std::chrono::milliseconds last_export_time{0};
};
```

#### Usage

```cpp
otlp_grpc_config config;
config.endpoint = "otel-collector:4317";
config.use_tls = true;
config.root_certificates = load_file("ca.pem");
config.headers["authorization"] = "Bearer " + token;
config.resource_attributes["service.name"] = "my_service";

auto exporter = create_otlp_grpc_exporter(config);
exporter->start();
exporter->export_spans(spans);
exporter->shutdown();  // Flushes remaining spans
```

---

## 5. Transport Layers

The transport layer provides protocol-level abstraction, decoupling exporters
from network implementation details. Each transport type follows the same
pattern:

1. **Abstract interface** — defines the contract
2. **Stub implementation** — for testing without network
3. **Real implementation(s)** — conditionally compiled when dependencies exist
4. **Factory function** — `create_default_*_transport()` selects the best available

### 5.1 HTTP Transport

**File**: [`http_transport.h`](../../include/kcenon/monitoring/exporters/http_transport.h)
**Used by**: Prometheus, Jaeger (Thrift), Zipkin, OTLP HTTP exporters

#### Interface

```cpp
class http_transport {
public:
    virtual ~http_transport() = default;
    virtual common::Result<http_response> send(const http_request& request) = 0;
    virtual bool is_available() const = 0;
    virtual std::string name() const = 0;
};
```

#### Request and Response

```cpp
struct http_request {
    std::string url;
    std::string method{"POST"};
    std::unordered_map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    std::chrono::milliseconds timeout{30000};
    bool enable_compression{false};
};

struct http_response {
    int status_code{0};
    std::string status_message;
    std::unordered_map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    std::chrono::milliseconds elapsed{0};
};
```

#### Implementations

| Class | Macro Guard | Backend | Methods |
|-------|-------------|---------|---------|
| `stub_http_transport` | — | In-memory | Custom response handler, simulate success/failure |
| `simple_http_client` | — | Basic socket | URL parsing, stub responses (placeholder) |
| `network_http_transport` | `MONITORING_HAS_NETWORK_SYSTEM` | `network_system::core::http_client` | GET, POST, PUT, DELETE, HEAD, PATCH |

#### Factory Functions

```cpp
// Returns network_http_transport if available, else simple_http_client
auto transport = create_default_transport();

// Explicitly create a stub for testing
auto stub = create_stub_transport();

// Explicitly create network_system transport (when available)
auto net = create_network_transport(std::chrono::milliseconds(5000));
```

### 5.2 gRPC Transport

**File**: [`grpc_transport.h`](../../include/kcenon/monitoring/exporters/grpc_transport.h)
**Used by**: OTLP gRPC, Jaeger gRPC exporters

#### Interface

```cpp
class grpc_transport {
public:
    virtual ~grpc_transport() = default;
    virtual common::VoidResult connect(const std::string& host, uint16_t port) = 0;
    virtual common::Result<grpc_response> send(const grpc_request& request) = 0;
    virtual bool is_connected() const = 0;
    virtual void disconnect() = 0;
    virtual bool is_available() const = 0;
    virtual std::string name() const = 0;
    virtual grpc_statistics get_statistics() const = 0;
    virtual void reset_statistics() = 0;
};
```

#### Request, Response, and Statistics

```cpp
struct grpc_request {
    std::string service;    // e.g., "opentelemetry.proto.collector.trace.v1.TraceService"
    std::string method;     // e.g., "Export"
    std::vector<uint8_t> body;
    std::chrono::milliseconds timeout{30000};
    std::unordered_map<std::string, std::string> metadata;
};

struct grpc_response {
    int status_code{0};     // gRPC status code (0 = OK)
    std::string status_message;
    std::vector<uint8_t> body;
    std::chrono::milliseconds elapsed{0};
};

struct grpc_statistics {
    std::size_t requests_sent{0};
    std::size_t bytes_sent{0};
    std::size_t send_failures{0};
};
```

#### Channel Management

When the gRPC library is available (`MONITORING_HAS_GRPC`), the system provides
connection pooling via `grpc_channel_manager`:

```cpp
struct grpc_channel_config {
    std::string target;                     // "host:port"
    bool use_tls{false};
    std::string root_certificates;          // PEM
    std::string private_key;                // PEM
    std::string certificate_chain;          // PEM
    std::chrono::milliseconds connect_timeout{5000};
    std::chrono::milliseconds keepalive_time{10000};
    bool enable_retry{true};
};

class grpc_channel_manager {
public:
    // Get or create a pooled channel
    std::shared_ptr<grpc::Channel> get_channel(const std::string& target);
    std::shared_ptr<grpc::Channel> get_channel(
        const std::string& target, const grpc_channel_config& config);
    void shutdown();
    std::size_t channel_count() const;
};
```

The channel manager:
- Pools connections by target address
- Detects dead channels (`GRPC_CHANNEL_SHUTDOWN`) and recreates them
- Supports TLS with mutual authentication
- Configures keepalive parameters

#### Implementations

| Class | Macro Guard | Backend |
|-------|-------------|---------|
| `stub_grpc_transport` | — | In-memory, custom response handler |
| `network_grpc_transport` | `MONITORING_HAS_GRPC` | `grpc::GenericStub` with channel pooling |

#### Health Check

```cpp
// Check if a gRPC endpoint is reachable
bool healthy = grpc_health_check(
    "localhost:4317",
    grpc_channel_config{},
    std::chrono::seconds(5)
);
```

#### Factory Functions

```cpp
// Returns network_grpc_transport if available, else stub
auto transport = create_default_grpc_transport();

// Explicitly create a stub for testing
auto stub = create_stub_grpc_transport();
```

### 5.3 UDP Transport

**File**: [`udp_transport.h`](../../include/kcenon/monitoring/exporters/udp_transport.h)
**Used by**: StatsD exporter

#### Interface

```cpp
class udp_transport {
public:
    virtual ~udp_transport() = default;
    virtual common::VoidResult connect(const std::string& host, uint16_t port) = 0;
    virtual common::VoidResult send(std::span<const uint8_t> data) = 0;
    virtual bool is_connected() const = 0;
    virtual void disconnect() = 0;
    virtual bool is_available() const = 0;
    virtual std::string name() const = 0;
    virtual udp_statistics get_statistics() const = 0;
    virtual void reset_statistics() = 0;

    // Convenience overload for string data
    common::VoidResult send(const std::string& data);
};
```

The `send(const std::string&)` method provides a convenience wrapper that
converts strings to `std::span<const uint8_t>` via `reinterpret_cast`.

#### Statistics

```cpp
struct udp_statistics {
    std::size_t packets_sent{0};
    std::size_t bytes_sent{0};
    std::size_t send_failures{0};
};
```

#### Implementations

| Class | Macro Guard | Backend |
|-------|-------------|---------|
| `stub_udp_transport` | — | In-memory, simulate success/failure |
| `common_udp_transport` | `MONITORING_HAS_COMMON_TRANSPORT_INTERFACES` | `kcenon::common::interfaces::IUdpClient` |
| `network_udp_transport` | `MONITORING_HAS_NETWORK_SYSTEM` | `network_system::udp::udp_client` |

#### Factory Functions

```cpp
// Returns network_udp_transport > stub (priority order)
auto transport = create_default_udp_transport();

// Explicitly create stub for testing
auto stub = create_stub_udp_transport();

// Create with common_system backend
auto common = create_common_udp_transport(udp_client);

// Create with network_system backend
auto net = create_network_udp_transport();
```

### 5.4 Transport Selection Summary

| Transport | Stub | Simple/Basic | Common System | Network System | gRPC Library |
|-----------|------|-------------|--------------|----------------|-------------|
| HTTP | `stub_http_transport` | `simple_http_client` | — | `network_http_transport` | — |
| gRPC | `stub_grpc_transport` | — | — | — | `network_grpc_transport` |
| UDP | `stub_udp_transport` | — | `common_udp_transport` | `network_udp_transport` | — |

**Conditional compilation macros**:
- `MONITORING_HAS_NETWORK_SYSTEM` — enables `network_system`-based transports
- `MONITORING_HAS_GRPC` — enables gRPC library-based transport
- `MONITORING_HAS_COMMON_TRANSPORT_INTERFACES` — enables `common_system`-based UDP

---

## 6. Custom Exporter Implementation

### 6.1 Step 1: Choose the Interface

Decide whether you are exporting metrics or traces:

```cpp
// For metrics
class my_exporter : public metric_exporter_interface { ... };

// For traces
class my_exporter : public trace_exporter_interface { ... };
```

### 6.2 Step 2: Implement a Custom Metric Exporter

Here is a complete example of a custom metric exporter that writes to a file:

```cpp
#include <kcenon/monitoring/exporters/metric_exporters.h>
#include <fstream>
#include <mutex>

class file_metric_exporter : public metric_exporter_interface {
private:
    std::string file_path_;
    std::ofstream file_;
    std::mutex mutex_;
    export_statistics stats_{};
    bool running_{false};

public:
    explicit file_metric_exporter(const std::string& path)
        : file_path_(path) {}

    common::VoidResult export_metrics(const monitoring_data& data) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return common::VoidResult::err(error_info(
                monitoring_error_code::invalid_state,
                "Exporter not started"
            ).to_common_error());
        }

        file_ << "[" << data.timestamp << "] "
              << data.source << ": " << data.value << "\n";
        file_.flush();

        stats_.items_exported++;
        return common::ok();
    }

    common::VoidResult export_snapshot(const metrics_snapshot& snapshot) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return common::VoidResult::err(error_info(
                monitoring_error_code::invalid_state,
                "Exporter not started"
            ).to_common_error());
        }

        for (const auto& metric : snapshot.metrics) {
            file_ << "[snapshot] " << metric.name
                  << " = " << metric.value << "\n";
            stats_.items_exported++;
        }
        file_.flush();
        return common::ok();
    }

    common::VoidResult start() override {
        std::lock_guard<std::mutex> lock(mutex_);
        file_.open(file_path_, std::ios::app);
        if (!file_.is_open()) {
            return common::VoidResult::err(error_info(
                monitoring_error_code::operation_failed,
                "Cannot open file: " + file_path_
            ).to_common_error());
        }
        running_ = true;
        return common::ok();
    }

    common::VoidResult stop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        return common::ok();
    }

    common::VoidResult flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            file_.flush();
        }
        return common::ok();
    }

    common::VoidResult shutdown() override {
        auto result = flush();
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        file_.close();
        return result;
    }

    export_statistics get_stats() const override {
        return stats_;
    }

    std::string name() const override {
        return "file_metric_exporter";
    }

    metric_export_format format() const override {
        return metric_export_format::prometheus_text; // or custom
    }
};
```

### 6.3 Step 3: Implement a Custom Trace Exporter

```cpp
#include <kcenon/monitoring/exporters/trace_exporters.h>
#include <iostream>

class console_trace_exporter : public trace_exporter_interface {
private:
    export_statistics stats_{};

public:
    common::VoidResult export_spans(
        const std::vector<trace_span>& spans) override {

        for (const auto& span : spans) {
            std::cout << "[TRACE] " << span.operation_name
                      << " trace_id=" << span.trace_id
                      << " span_id=" << span.span_id
                      << " duration=" << span.duration_us << "us\n";
            stats_.items_exported++;
        }
        return common::ok();
    }

    common::VoidResult flush() override {
        std::cout.flush();
        return common::ok();
    }

    common::VoidResult shutdown() override {
        return flush();
    }

    export_statistics get_stats() const override {
        return stats_;
    }

    std::string name() const override {
        return "console_trace_exporter";
    }

    trace_export_format format() const override {
        return trace_export_format::otlp_grpc; // or custom
    }
};
```

### 6.4 Step 4: Use a Custom Transport

If your exporter needs a non-standard protocol, implement a custom transport:

```cpp
class websocket_transport : public http_transport {
private:
    std::string ws_url_;
    bool connected_{false};

public:
    explicit websocket_transport(const std::string& url)
        : ws_url_(url) {}

    common::Result<http_response> send(const http_request& request) override {
        // Convert HTTP request to WebSocket frame and send
        // Return response wrapped in http_response
        http_response response;
        response.status_code = 200;
        response.status_message = "OK";
        return common::ok(response);
    }

    bool is_available() const override { return connected_; }
    std::string name() const override { return "websocket"; }
};
```

### 6.5 Best Practices for Custom Exporters

| Practice | Rationale |
|----------|-----------|
| **Thread safety** | Multiple collectors may call `export_*()` concurrently |
| **Graceful shutdown** | `shutdown()` should flush pending data before closing |
| **Error propagation** | Return `common::VoidResult` with descriptive error info |
| **Statistics tracking** | Keep counters for exported, failed, and dropped items |
| **Batching** | Accumulate data and send in batches for better throughput |
| **Retry logic** | Implement exponential backoff for transient failures |
| **Resource cleanup** | Close connections and file handles in destructor |

---

## 7. OpenTelemetry Integration

### 7.1 Compatibility Layer Overview

**File**: [`opentelemetry_adapter.h`](../../include/kcenon/monitoring/exporters/opentelemetry_adapter.h)

The OpenTelemetry compatibility layer bridges monitoring_system's internal
data model to the OpenTelemetry data model without requiring the official
OpenTelemetry C++ SDK.

```
  monitoring_system data model          OpenTelemetry data model
  ─────────────────────────            ─────────────────────────
  trace_span                 ──────►   otel_span_data
  metrics_snapshot           ──────►   otel_metric_data
  monitoring_data            ──────►   otel_metric_data
                                       otel_resource (service info)
```

### 7.2 OTEL Data Types

```cpp
// Resource identification
struct otel_resource {
    otel_resource_type type;  // service, host, process, container, cloud
    std::string name;
    std::string version;
    std::unordered_map<std::string, std::string> attributes;
};

// Span data
struct otel_span_data {
    std::string trace_id;       // 32 hex chars
    std::string span_id;        // 16 hex chars
    std::string parent_span_id;
    std::string name;
    otel_span_kind kind;        // internal, server, client, producer, consumer
    otel_status_code status;    // unset, ok, error
    int64_t start_time_ns;
    int64_t end_time_ns;
    std::vector<otel_attribute> attributes;
    std::vector<otel_span_event> events;
};

// Metric data
struct otel_metric_data {
    std::string name;
    std::string description;
    std::string unit;
    otel_metric_type type;      // gauge, sum, histogram
    std::vector<otel_data_point> data_points;
};
```

### 7.3 Adapter Classes

#### Tracer Adapter

Converts `trace_span` → `otel_span_data`:

```cpp
class opentelemetry_tracer_adapter {
public:
    otel_span_data convert_span(const trace_span& span) const;
    std::vector<otel_span_data> convert_spans(
        const std::vector<trace_span>& spans) const;
};
```

The adapter handles:
- Span kind mapping (`trace_span_kind` → `otel_span_kind`)
- Status code mapping
- Attribute conversion
- Timestamp conversion (microseconds → nanoseconds)

#### Metrics Adapter

Converts `metrics_snapshot` and `monitoring_data` → `otel_metric_data`:

```cpp
class opentelemetry_metrics_adapter {
public:
    std::vector<otel_metric_data> convert_snapshot(
        const metrics_snapshot& snapshot) const;
    otel_metric_data convert_monitoring_data(
        const monitoring_data& data) const;
};
```

### 7.4 Compatibility Layer

The `opentelemetry_compatibility_layer` provides a unified interface for
sending both spans and metrics through the OTEL pipeline:

```cpp
class opentelemetry_compatibility_layer {
public:
    explicit opentelemetry_compatibility_layer(
        const opentelemetry_exporter_config& config);

    common::VoidResult initialize();
    common::VoidResult shutdown();

    // Export operations
    common::VoidResult export_spans(const std::vector<trace_span>& spans);
    common::VoidResult export_metrics(const metrics_snapshot& snapshot);

    // Batch management
    common::VoidResult flush();
    std::size_t pending_spans() const;
    std::size_t pending_metrics() const;
};
```

#### Configuration

```cpp
struct opentelemetry_exporter_config {
    std::string endpoint;
    std::string protocol;           // "grpc", "http", "json"
    std::chrono::milliseconds timeout{30000};
    std::size_t max_batch_size{512};
    bool enable_compression{false};
    std::unordered_map<std::string, std::string> headers;
};
```

### 7.5 Resource Creation

```cpp
// Create a service resource with SDK metadata
otel_resource resource = create_service_resource(
    "my_service",   // service.name
    "1.2.3"         // service.version
);

// Resulting attributes:
// service.name = "my_service"
// service.version = "1.2.3"
// telemetry.sdk.name = "monitoring_system"
// telemetry.sdk.language = "cpp"
// telemetry.sdk.version = "0.x.y.z"
```

### 7.6 Integration Pattern

```cpp
// 1. Create resource
auto resource = create_service_resource("my_service", "1.0.0");

// 2. Configure OTEL
opentelemetry_exporter_config config;
config.endpoint = "otel-collector:4317";
config.protocol = "grpc";
config.enable_compression = true;

// 3. Initialize compatibility layer
opentelemetry_compatibility_layer otel(config);
otel.initialize();

// 4. Export data
otel.export_spans(collected_spans);
otel.export_metrics(metrics_snapshot);

// 5. Shutdown
otel.flush();
otel.shutdown();
```

---

## 8. Configuration Examples

### 8.1 Production: Multi-Backend Setup

This configuration sends metrics to Prometheus and traces to an OTLP collector
simultaneously:

```cpp
#include <kcenon/monitoring/exporters/metric_exporters.h>
#include <kcenon/monitoring/exporters/trace_exporters.h>
#include <kcenon/monitoring/exporters/otlp_grpc_exporter.h>

// === Metric Exporters ===

// Prometheus for metrics dashboard (Grafana)
auto prometheus = create_prometheus_exporter(9090, "production_service");
prometheus->start();

// StatsD for real-time counters (DataDog)
auto statsd = create_statsd_exporter("statsd.internal", 8125, /*datadog=*/true);
statsd->start();

// === Trace Exporter ===

// OTLP gRPC for distributed tracing
otlp_grpc_config otlp_config;
otlp_config.endpoint = "otel-collector.internal:4317";
otlp_config.use_tls = true;
otlp_config.root_certificates = load_pem("ca.pem");
otlp_config.headers["authorization"] = "Bearer " + auth_token;
otlp_config.resource_attributes["service.name"] = "production_service";
otlp_config.resource_attributes["deployment.environment"] = "production";
otlp_config.max_batch_size = 1024;
otlp_config.max_retries = 5;

auto traces = create_otlp_grpc_exporter(otlp_config);
traces->start();

// === Export Data ===
prometheus->export_metrics(data);
statsd->export_metrics(data);
traces->export_spans(spans);

// === Graceful Shutdown ===
prometheus->shutdown();
statsd->shutdown();
traces->shutdown();
```

### 8.2 Development: Local Observability Stack

Minimal setup for local development with Jaeger and console output:

```cpp
#include <kcenon/monitoring/exporters/trace_exporters.h>
#include <kcenon/monitoring/exporters/metric_exporters.h>

// Prometheus on localhost for metrics
auto prometheus = create_prometheus_exporter(9090, "dev_service");
prometheus->start();

// Jaeger on localhost for traces (Docker: jaegertracing/all-in-one)
auto jaeger = create_jaeger_exporter("localhost:14268", "dev_service");

// Export
prometheus->export_snapshot(snapshot);
jaeger->export_spans(spans);

// Check stats
auto stats = jaeger->get_stats();
std::cout << "Exported: " << stats.items_exported
          << " Failed: " << stats.items_failed << "\n";
```

### 8.3 Testing: Stub Transports

Use stub transports to test exporter logic without network dependencies:

```cpp
#include <kcenon/monitoring/exporters/http_transport.h>
#include <kcenon/monitoring/exporters/grpc_transport.h>
#include <kcenon/monitoring/exporters/udp_transport.h>

// === HTTP Stub ===
auto http_stub = create_stub_transport();
http_stub->set_response_handler([](const http_request& req) {
    http_response resp;
    resp.status_code = 202;
    resp.status_message = "Accepted";
    // Verify request contents
    assert(req.method == "POST");
    assert(!req.body.empty());
    return resp;
});

// === gRPC Stub ===
auto grpc_stub = create_stub_grpc_transport();
grpc_stub->connect("localhost", 4317);
assert(grpc_stub->is_connected());

grpc_request req;
req.service = "opentelemetry.proto.collector.trace.v1.TraceService";
req.method = "Export";
req.body = serialized_spans;
auto result = grpc_stub->send(req);
assert(result.is_ok());

auto stats = grpc_stub->get_statistics();
assert(stats.requests_sent == 1);

// === UDP Stub ===
auto udp_stub = create_stub_udp_transport();
udp_stub->connect("localhost", 8125);
udp_stub->send("cpu.usage:45.2|g");

auto udp_stats = udp_stub->get_statistics();
assert(udp_stats.packets_sent == 1);

// === Simulate Failures ===
udp_stub->set_simulate_success(false);
auto fail_result = udp_stub->send("metric:1|c");
assert(fail_result.is_err());
assert(udp_stats.send_failures > 0);
```

### 8.4 Zipkin with Custom Headers

Send traces to a Zipkin instance behind an API gateway:

```cpp
trace_export_config config;
config.endpoint = "https://zipkin.example.com";
config.format = trace_export_format::zipkin_json;
config.timeout = std::chrono::milliseconds(5000);
config.batch_timeout = std::chrono::milliseconds(2000);
config.max_batch_size = 100;
config.compression = true;
config.headers["X-API-Key"] = api_key;
config.headers["X-Tenant-ID"] = tenant_id;
config.service_name = "gateway_service";

auto exporter = trace_exporter_factory::create_exporter(config);
exporter->export_spans(spans);
```

---

## Appendix A: Exporter Quick Reference

| Exporter | Pipeline | Protocol | Transport | Pull/Push |
|----------|----------|----------|-----------|-----------|
| `prometheus_exporter` | Metrics | Text exposition | HTTP (server) | Pull |
| `statsd_exporter` | Metrics | StatsD / DataDog | UDP | Push |
| `otlp_metrics_exporter` | Metrics | OTLP | HTTP or gRPC | Push |
| `jaeger_exporter` | Traces | Thrift / Protobuf | HTTP or gRPC | Push |
| `zipkin_exporter` | Traces | JSON v2 / Protobuf | HTTP | Push |
| `otlp_exporter` | Traces | OTLP | HTTP or gRPC | Push |
| `otlp_grpc_exporter` | Traces | OTLP Protobuf | gRPC | Push |

## Appendix B: Configuration Structs Reference

### metric_export_config

```cpp
struct metric_export_config {
    std::string endpoint;
    uint16_t port{0};
    metric_export_format format;
    std::chrono::milliseconds push_interval{10000};
    std::chrono::milliseconds timeout{30000};
    std::size_t max_batch_size{1000};
    std::size_t max_queue_size{10000};
    bool compression{false};
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> default_labels;
};
```

### trace_export_config

```cpp
struct trace_export_config {
    std::string endpoint;
    trace_export_format format;
    std::chrono::milliseconds timeout{30000};
    std::chrono::milliseconds batch_timeout{5000};
    std::size_t max_batch_size{512};
    std::size_t max_queue_size{2048};
    bool compression{false};
    std::unordered_map<std::string, std::string> headers;
    std::string service_name;
};
```

## Appendix C: File Reference

All source files referenced in this guide:

| File | Path |
|------|------|
| `metric_exporters.h` | `include/kcenon/monitoring/exporters/metric_exporters.h` |
| `trace_exporters.h` | `include/kcenon/monitoring/exporters/trace_exporters.h` |
| `opentelemetry_adapter.h` | `include/kcenon/monitoring/exporters/opentelemetry_adapter.h` |
| `otlp_grpc_exporter.h` | `include/kcenon/monitoring/exporters/otlp_grpc_exporter.h` |
| `http_transport.h` | `include/kcenon/monitoring/exporters/http_transport.h` |
| `grpc_transport.h` | `include/kcenon/monitoring/exporters/grpc_transport.h` |
| `udp_transport.h` | `include/kcenon/monitoring/exporters/udp_transport.h` |
| `result_types.h` | `include/kcenon/monitoring/core/result_types.h` |
| `error_codes.h` | `include/kcenon/monitoring/core/error_codes.h` |

---

*This guide is part of the monitoring_system documentation suite.*
*See also: [Collector Development Guide](COLLECTOR_DEVELOPMENT.md) | [Architecture](../ARCHITECTURE.md) | [Plugin Architecture](../plugin_architecture.md)*
