# OpenTelemetry Collector 사이드카 패턴

> **Language:** [English](OTEL_COLLECTOR_SIDECAR.md) | **한국어**

## 개요

이 가이드는 monitoring_system의 네이티브 gRPC 지원이 개발 중인 동안 OpenTelemetry Collector를 사이드카로 사용하여 OTLP gRPC 전송 기능을 활성화하는 방법을 설명합니다.

**버전:** 0.2.0.0
**최종 업데이트:** 2025-01-09

---

## 목차

- [아키텍처](#아키텍처)
- [이 패턴을 사용해야 할 때](#이-패턴을-사용해야-할-때)
- [빠른 시작](#빠른-시작)
- [설정](#설정)
- [Docker Compose 설정](#docker-compose-설정)
- [Kubernetes 배포](#kubernetes-배포)
- [문제 해결](#문제-해결)

---

## 아키텍처

사이드카 패턴은 OpenTelemetry Collector를 사용하여 monitoring_system의 HTTP 기반 내보내기를 gRPC 기반 백엔드로 연결합니다.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                            호스트 / 파드                                 │
│                                                                         │
│  ┌─────────────────────┐          ┌─────────────────────┐              │
│  │  monitoring_system  │  HTTP    │  OpenTelemetry      │              │
│  │  애플리케이션        │ ────────>│  Collector          │              │
│  │  (OTLP HTTP/JSON)   │  :4318   │  (사이드카)          │              │
│  └─────────────────────┘          └──────────┬──────────┘              │
│                                              │                          │
└──────────────────────────────────────────────┼──────────────────────────┘
                                               │ gRPC :4317
                                               ▼
                                    ┌─────────────────────┐
                                    │  백엔드 서비스       │
                                    │  (Jaeger, Tempo,    │
                                    │   Grafana 등)       │
                                    └─────────────────────┘
```

### 데이터 흐름

1. **애플리케이션**이 OTLP HTTP/JSON을 통해 `localhost:4318`로 트레이스를 내보냄
2. **OTel Collector**가 트레이스를 수신하고 gRPC 형식으로 변환
3. **백엔드**가 포트 `4317`에서 OTLP gRPC를 통해 트레이스를 수신

---

## 이 패턴을 사용해야 할 때

### 권장 상황:

- 백엔드가 **gRPC만 지원**하는 경우 (예: 일부 Jaeger 구성)
- 서비스 간 **프로토콜 변환**이 필요한 경우
- 애플리케이션 코드 수정 없이 **버퍼링과 재시도** 로직이 필요한 경우
- 사이드카 패턴이 표준인 **Kubernetes**에 배포하는 경우

### 필요하지 않은 경우:

- 백엔드가 HTTP/JSON을 지원하는 경우 (대부분의 최신 백엔드)
- Jaeger나 Zipkin을 HTTP 컬렉터와 함께 사용하는 경우
- 지연 시간이 중요하여 홉을 최소화해야 하는 경우

---

## 빠른 시작

### 1. monitoring_system 설정

```cpp
#include <kcenon/monitoring/exporters/trace_exporters.h>

using namespace kcenon::monitoring;

// OTLP HTTP 익스포터를 로컬 컬렉터에 연결하도록 설정
trace_export_config config;
config.endpoint = "http://localhost:4318";  // OTel Collector HTTP 포트
config.format = trace_export_format::otlp_http_json;
config.timeout = std::chrono::milliseconds(5000);

// 익스포터 생성
auto resource = create_service_resource("my-service", "1.0.0");
auto exporter = create_otlp_exporter(config.endpoint, resource, config.format);
```

### 2. OTel Collector 설정 파일 생성

`otel-collector-config.yaml` 생성:

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
      insecure: true  # 프로덕션에서는 적절한 인증서와 함께 false로 설정

service:
  pipelines:
    traces:
      receivers: [otlp]
      processors: [batch]
      exporters: [otlp]
```

### 3. OTel Collector 실행

```bash
# Docker 사용
docker run -d \
  --name otel-collector \
  -p 4317:4317 \
  -p 4318:4318 \
  -v $(pwd)/otel-collector-config.yaml:/etc/otelcol/config.yaml \
  otel/opentelemetry-collector:latest

# 또는 더 많은 익스포터를 위해 contrib 이미지 사용
docker run -d \
  --name otel-collector \
  -p 4317:4317 \
  -p 4318:4318 \
  -v $(pwd)/otel-collector-config.yaml:/etc/otelcol-contrib/config.yaml \
  otel/opentelemetry-collector-contrib:latest
```

---

## 설정

### OTel Collector 설정 옵션

#### 리시버 (Receivers)

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

#### 프로세서 (Processors)

```yaml
processors:
  # 효율성을 위한 배치 프로세서
  batch:
    timeout: 1s
    send_batch_size: 512

  # OOM 방지를 위한 메모리 제한
  memory_limiter:
    check_interval: 1s
    limit_mib: 512
    spike_limit_mib: 128

  # 속성 추가를 위한 리소스 프로세서
  resource:
    attributes:
      - key: environment
        value: production
        action: upsert
```

#### 익스포터 (Exporters)

```yaml
exporters:
  # OTLP gRPC 익스포터
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

  # Jaeger 익스포터
  jaeger:
    endpoint: "jaeger:14250"
    tls:
      insecure: true

  # 개발용 디버그 익스포터
  debug:
    verbosity: detailed
```

---

## Docker Compose 설정

`docker-compose.yml` 생성:

```yaml
version: '3.8'

services:
  # 애플리케이션
  app:
    build: .
    environment:
      - OTEL_EXPORTER_ENDPOINT=http://otel-collector:4318
    depends_on:
      - otel-collector
    networks:
      - monitoring

  # OpenTelemetry Collector 사이드카
  otel-collector:
    image: otel/opentelemetry-collector-contrib:latest
    command: ["--config=/etc/otel-collector-config.yaml"]
    volumes:
      - ./otel-collector-config.yaml:/etc/otel-collector-config.yaml:ro
    ports:
      - "4317:4317"   # gRPC
      - "4318:4318"   # HTTP
      - "8888:8888"   # Prometheus 메트릭 (선택)
    networks:
      - monitoring
    depends_on:
      - jaeger

  # Jaeger 백엔드 (예시)
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

### Docker Compose용 OTel Collector 설정

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

### 실행

```bash
# 모든 서비스 시작
docker-compose up -d

# 로그 확인
docker-compose logs -f otel-collector

# Jaeger UI 접속
open http://localhost:16686
```

---

## Kubernetes 배포

### 사이드카 컨테이너 패턴

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
        # 애플리케이션 컨테이너
        - name: app
          image: my-application:latest
          env:
            - name: OTEL_EXPORTER_ENDPOINT
              value: "http://localhost:4318"
          ports:
            - containerPort: 8080

        # OTel Collector 사이드카
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

### OpenTelemetry Operator 사용 (권장)

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

## 문제 해결

### 일반적인 문제

#### 1. Collector 연결 거부

**증상**: 애플리케이션이 `localhost:4318`에 연결할 수 없음

**해결 방법**:
- Collector 실행 확인: `docker ps | grep otel`
- Collector 로그 확인: `docker logs otel-collector`
- 포트가 올바르게 매핑되었는지 확인

#### 2. 백엔드에 트레이스가 나타나지 않음

**증상**: Collector는 트레이스를 수신하지만 백엔드에 표시되지 않음

**진단**:
```bash
# 디버그 익스포터 활성화
docker exec -it otel-collector cat /etc/otel/config.yaml
# 파이프라인 익스포터에 'debug' 추가
```

**일반적인 원인**:
- TLS 잘못된 설정 (테스트용으로 `insecure: true` 시도)
- 잘못된 엔드포인트 URL
- 트래픽을 차단하는 네트워크 정책

#### 3. 높은 메모리 사용량

**증상**: Collector OOM 또는 높은 메모리

**해결 방법**: 메모리 제한 프로세서 추가
```yaml
processors:
  memory_limiter:
    check_interval: 1s
    limit_mib: 256
    spike_limit_mib: 64
```

#### 4. 트레이스 지연 또는 배치됨

**증상**: 트레이스가 지연되어 나타남

**설명**: 배치 프로세서를 사용하면 예상되는 동작. 더 낮은 지연 시간을 위해 조정:
```yaml
processors:
  batch:
    timeout: 100ms  # 더 빠른 내보내기를 위한 낮은 타임아웃
    send_batch_size: 100
```

### 디버깅 명령어

```bash
# Collector 상태 확인
curl -v http://localhost:8888/metrics

# OTLP HTTP 엔드포인트 테스트
curl -X POST http://localhost:4318/v1/traces \
  -H "Content-Type: application/json" \
  -d '{"resourceSpans":[]}'

# Collector 내부 메트릭 확인
curl http://localhost:8888/metrics | grep otelcol
```

---

## 성능 고려사항

| 항목 | 권장 사항 |
|------|----------|
| **배치 크기** | 최적의 처리량을 위해 512-1024 스팬 |
| **타임아웃** | 지연 시간 요구사항에 따라 1-5초 |
| **메모리 제한** | 사이드카 256-512 MiB, 게이트웨이 1-2 GiB |
| **재시도** | 지수 백오프와 함께 활성화 |

### 리소스 요청 (Kubernetes)

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

## 마이그레이션 경로

monitoring_system의 네이티브 gRPC 지원이 가능해지면 (Issue #325) 마이그레이션할 수 있습니다:

1. gRPC 지원이 포함된 버전으로 **monitoring_system 업데이트**
2. 네이티브 gRPC를 사용하도록 **설정 업데이트**:
   ```cpp
   config.format = trace_export_format::otlp_grpc;
   config.endpoint = "backend:4317";
   ```
3. 배포에서 **사이드카 제거**
4. 트레이스가 여전히 올바르게 흐르는지 **확인**

---

## 참고 자료

- [OpenTelemetry Collector 문서](https://opentelemetry.io/docs/collector/)
- [OTLP 명세](https://opentelemetry.io/docs/specs/otlp/)
- [OpenTelemetry Operator](https://github.com/open-telemetry/opentelemetry-operator)
- [monitoring_system Issue #325](https://github.com/kcenon/monitoring_system/issues/325)

---

**최종 업데이트**: 2025-01-09
**관리자**: kcenon@naver.com
