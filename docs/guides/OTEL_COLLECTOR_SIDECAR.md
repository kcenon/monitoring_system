# OpenTelemetry Collector Sidecar Pattern

> **Language:** **English** | [한국어](OTEL_COLLECTOR_SIDECAR.kr.md)

## Overview

This guide explains how to use OpenTelemetry Collector as a sidecar to enable OTLP gRPC transport functionality while monitoring_system's native gRPC support is under development.

**Version:** 0.2.0.0
**Last Updated:** 2025-01-09

---

## Table of Contents

- [Architecture](#architecture)
- [When to Use This Pattern](#when-to-use-this-pattern)
- [Quick Start](#quick-start)
- [Configuration](#configuration)
- [Docker Compose Setup](#docker-compose-setup)
- [Kubernetes Deployment](#kubernetes-deployment)
- [Troubleshooting](#troubleshooting)

---

## Architecture

The sidecar pattern uses OpenTelemetry Collector to bridge monitoring_system's HTTP-based exports to gRPC-based backends.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                            Host / Pod                                    │
│                                                                         │
│  ┌─────────────────────┐          ┌─────────────────────┐              │
│  │  monitoring_system  │  HTTP    │  OpenTelemetry      │              │
│  │  Application        │ ────────>│  Collector          │              │
│  │  (OTLP HTTP/JSON)   │  :4318   │  (Sidecar)          │              │
│  └─────────────────────┘          └──────────┬──────────┘              │
│                                              │                          │
└──────────────────────────────────────────────┼──────────────────────────┘
                                               │ gRPC :4317
                                               ▼
                                    ┌─────────────────────┐
                                    │  Backend Service    │
                                    │  (Jaeger, Tempo,    │
                                    │   Grafana, etc.)    │
                                    └─────────────────────┘
```

### Data Flow

1. **Application** exports traces via OTLP HTTP/JSON to `localhost:4318`
2. **OTel Collector** receives traces and converts to gRPC format
3. **Backend** receives traces via OTLP gRPC on port `4317`

---

## When to Use This Pattern

### Recommended When:

- Your backend **only supports gRPC** (e.g., some Jaeger configurations)
- You need **protocol transformation** between services
- You want **buffering and retry** logic without modifying application code
- You're deploying in **Kubernetes** where sidecar patterns are standard

### Not Needed When:

- Your backend supports HTTP/JSON (most modern backends do)
- You're using Jaeger or Zipkin with HTTP collectors
- Low latency is critical and you want to minimize hops

---

## Quick Start

### 1. Configure monitoring_system

```cpp
#include <kcenon/monitoring/exporters/trace_exporters.h>

using namespace kcenon::monitoring;

// Configure OTLP HTTP exporter to local collector
trace_export_config config;
config.endpoint = "http://localhost:4318";  // OTel Collector HTTP port
config.format = trace_export_format::otlp_http_json;
config.timeout = std::chrono::milliseconds(5000);

// Create exporter
auto resource = create_service_resource("my-service", "1.0.0");
auto exporter = create_otlp_exporter(config.endpoint, resource, config.format);
```

### 2. Create OTel Collector Configuration

Create `otel-collector-config.yaml`:

```yaml
receivers:
  otlp:
    protocols:
      http:
        endpoint: 0.0.0.0:4318
      grpc:
        endpoint: 0.0.0.0:4317

processors:
  batch:
    timeout: 1s
    send_batch_size: 512
    send_batch_max_size: 1024

exporters:
  otlp:
    endpoint: "your-backend:4317"
    tls:
      insecure: true  # Set to false in production with proper certs

service:
  pipelines:
    traces:
      receivers: [otlp]
      processors: [batch]
      exporters: [otlp]
```

### 3. Run OTel Collector

```bash
# Using Docker
docker run -d \
  --name otel-collector \
  -p 4317:4317 \
  -p 4318:4318 \
  -v $(pwd)/otel-collector-config.yaml:/etc/otelcol/config.yaml \
  otel/opentelemetry-collector:latest

# Or using the contrib image for more exporters
docker run -d \
  --name otel-collector \
  -p 4317:4317 \
  -p 4318:4318 \
  -v $(pwd)/otel-collector-config.yaml:/etc/otelcol-contrib/config.yaml \
  otel/opentelemetry-collector-contrib:latest
```

---

## Configuration

### OTel Collector Configuration Options

#### Receivers

```yaml
receivers:
  otlp:
    protocols:
      http:
        endpoint: 0.0.0.0:4318
        cors:
          allowed_origins:
            - "*"
        max_request_body_size: 5242880  # 5MB
      grpc:
        endpoint: 0.0.0.0:4317
        max_recv_msg_size_mib: 4
```

#### Processors

```yaml
processors:
  # Batch processor for efficiency
  batch:
    timeout: 1s
    send_batch_size: 512

  # Memory limiter to prevent OOM
  memory_limiter:
    check_interval: 1s
    limit_mib: 512
    spike_limit_mib: 128

  # Resource processor to add attributes
  resource:
    attributes:
      - key: environment
        value: production
        action: upsert
```

#### Exporters

```yaml
exporters:
  # OTLP gRPC exporter
  otlp:
    endpoint: "tempo:4317"
    tls:
      insecure: false
      cert_file: /etc/certs/cert.pem
      key_file: /etc/certs/key.pem
    headers:
      Authorization: "Bearer ${OTEL_AUTH_TOKEN}"
    retry_on_failure:
      enabled: true
      initial_interval: 5s
      max_interval: 30s
      max_elapsed_time: 300s

  # Jaeger exporter
  jaeger:
    endpoint: "jaeger:14250"
    tls:
      insecure: true

  # Debug exporter for development
  debug:
    verbosity: detailed
```

---

## Docker Compose Setup

Create `docker-compose.yml`:

```yaml
version: '3.8'

services:
  # Your application
  app:
    build: .
    environment:
      - OTEL_EXPORTER_ENDPOINT=http://otel-collector:4318
    depends_on:
      - otel-collector
    networks:
      - monitoring

  # OpenTelemetry Collector sidecar
  otel-collector:
    image: otel/opentelemetry-collector-contrib:latest
    command: ["--config=/etc/otel-collector-config.yaml"]
    volumes:
      - ./otel-collector-config.yaml:/etc/otel-collector-config.yaml:ro
    ports:
      - "4317:4317"   # gRPC
      - "4318:4318"   # HTTP
      - "8888:8888"   # Prometheus metrics (optional)
    networks:
      - monitoring
    depends_on:
      - jaeger

  # Jaeger backend (example)
  jaeger:
    image: jaegertracing/all-in-one:latest
    ports:
      - "16686:16686"  # UI
      - "14250:14250"  # gRPC
    environment:
      - COLLECTOR_OTLP_ENABLED=true
    networks:
      - monitoring

networks:
  monitoring:
    driver: bridge
```

### OTel Collector Config for Docker Compose

```yaml
# otel-collector-config.yaml
receivers:
  otlp:
    protocols:
      http:
        endpoint: 0.0.0.0:4318
      grpc:
        endpoint: 0.0.0.0:4317

processors:
  batch:
    timeout: 1s
    send_batch_size: 512

  memory_limiter:
    check_interval: 1s
    limit_mib: 256

exporters:
  otlp:
    endpoint: "jaeger:4317"
    tls:
      insecure: true

  debug:
    verbosity: basic

service:
  pipelines:
    traces:
      receivers: [otlp]
      processors: [memory_limiter, batch]
      exporters: [otlp, debug]
```

### Running

```bash
# Start all services
docker-compose up -d

# View logs
docker-compose logs -f otel-collector

# Access Jaeger UI
open http://localhost:16686
```

---

## Kubernetes Deployment

### Sidecar Container Pattern

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: my-application
spec:
  replicas: 3
  selector:
    matchLabels:
      app: my-application
  template:
    metadata:
      labels:
        app: my-application
    spec:
      containers:
        # Application container
        - name: app
          image: my-application:latest
          env:
            - name: OTEL_EXPORTER_ENDPOINT
              value: "http://localhost:4318"
          ports:
            - containerPort: 8080

        # OTel Collector sidecar
        - name: otel-collector
          image: otel/opentelemetry-collector:latest
          args:
            - "--config=/etc/otel/config.yaml"
          ports:
            - containerPort: 4317
              name: otlp-grpc
            - containerPort: 4318
              name: otlp-http
          volumeMounts:
            - name: otel-config
              mountPath: /etc/otel

      volumes:
        - name: otel-config
          configMap:
            name: otel-collector-config
---
apiVersion: v1
kind: ConfigMap
metadata:
  name: otel-collector-config
data:
  config.yaml: |
    receivers:
      otlp:
        protocols:
          http:
            endpoint: 0.0.0.0:4318
          grpc:
            endpoint: 0.0.0.0:4317

    processors:
      batch:
        timeout: 1s
        send_batch_size: 512

    exporters:
      otlp:
        endpoint: "otel-gateway.monitoring:4317"
        tls:
          insecure: true

    service:
      pipelines:
        traces:
          receivers: [otlp]
          processors: [batch]
          exporters: [otlp]
```

### Using OpenTelemetry Operator (Recommended)

```yaml
apiVersion: opentelemetry.io/v1alpha1
kind: OpenTelemetryCollector
metadata:
  name: sidecar
spec:
  mode: sidecar
  config: |
    receivers:
      otlp:
        protocols:
          http:
            endpoint: 0.0.0.0:4318
          grpc:
            endpoint: 0.0.0.0:4317

    processors:
      batch:
        timeout: 1s

    exporters:
      otlp:
        endpoint: "otel-gateway.monitoring:4317"
        tls:
          insecure: true

    service:
      pipelines:
        traces:
          receivers: [otlp]
          processors: [batch]
          exporters: [otlp]
---
apiVersion: apps/v1
kind: Deployment
metadata:
  name: my-application
  annotations:
    sidecar.opentelemetry.io/inject: "true"
spec:
  template:
    spec:
      containers:
        - name: app
          image: my-application:latest
```

---

## Troubleshooting

### Common Issues

#### 1. Connection Refused to Collector

**Symptom**: Application cannot connect to `localhost:4318`

**Solution**:
- Verify collector is running: `docker ps | grep otel`
- Check collector logs: `docker logs otel-collector`
- Ensure ports are correctly mapped

#### 2. No Traces Appearing in Backend

**Symptom**: Collector receives traces but backend shows nothing

**Diagnosis**:
```bash
# Enable debug exporter
docker exec -it otel-collector cat /etc/otel/config.yaml
# Add 'debug' to exporters in pipeline
```

**Common Causes**:
- TLS misconfiguration (try `insecure: true` for testing)
- Wrong endpoint URL
- Network policies blocking traffic

#### 3. High Memory Usage

**Symptom**: Collector OOM or high memory

**Solution**: Add memory limiter processor
```yaml
processors:
  memory_limiter:
    check_interval: 1s
    limit_mib: 256
    spike_limit_mib: 64
```

#### 4. Traces Delayed or Batched

**Symptom**: Traces appear with delay

**Explanation**: This is expected behavior with batch processor. Adjust for lower latency:
```yaml
processors:
  batch:
    timeout: 100ms  # Lower timeout for faster export
    send_batch_size: 100
```

### Debugging Commands

```bash
# Check collector health
curl -v http://localhost:8888/metrics

# Test OTLP HTTP endpoint
curl -X POST http://localhost:4318/v1/traces \
  -H "Content-Type: application/json" \
  -d '{"resourceSpans":[]}'

# View collector internal metrics
curl http://localhost:8888/metrics | grep otelcol
```

---

## Performance Considerations

| Aspect | Recommendation |
|--------|----------------|
| **Batch Size** | 512-1024 spans for optimal throughput |
| **Timeout** | 1-5s depending on latency requirements |
| **Memory Limit** | 256-512 MiB for sidecar, 1-2 GiB for gateway |
| **Retry** | Enable with exponential backoff |

### Resource Requests (Kubernetes)

```yaml
resources:
  requests:
    cpu: 100m
    memory: 128Mi
  limits:
    cpu: 500m
    memory: 512Mi
```

---

## Migration Path

When monitoring_system's native gRPC support is available (Issue #325), you can migrate:

1. **Update monitoring_system** to version with gRPC support
2. **Update configuration** to use native gRPC:
   ```cpp
   config.format = trace_export_format::otlp_grpc;
   config.endpoint = "backend:4317";
   ```
3. **Remove sidecar** from deployment
4. **Verify** traces still flow correctly

---

## References

- [OpenTelemetry Collector Documentation](https://opentelemetry.io/docs/collector/)
- [OTLP Specification](https://opentelemetry.io/docs/specs/otlp/)
- [OpenTelemetry Operator](https://github.com/open-telemetry/opentelemetry-operator)
- [monitoring_system Issue #325](https://github.com/kcenon/monitoring_system/issues/325)

---

**Last Updated**: 2025-01-09
**Maintainer**: kcenon@naver.com
