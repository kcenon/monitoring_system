# Monitoring System - 상세 기능

**언어:** [English](README.md) | **한국어**

**최종 업데이트**: 2025-11-28
**버전**: 3.0

이 문서는 Monitoring System의 모든 기능에 대한 포괄적인 세부 정보를 제공합니다.

---

## 목차

- [핵심 기능](#핵심-기능)
- [메트릭 수집](#메트릭-수집)
- [분산 추적](#분산-추적)
- [알림 시스템](#알림-시스템)
- [웹 대시보드](#웹-대시보드)
- [스토리지 백엔드](#스토리지-백엔드)
- [성능 특성](#성능-특성)
- [통합 기능](#통합-기능)

---

## 핵심 기능

### 시스템 개요

Monitoring System은 C++20 애플리케이션을 위한 프로덕션 준비 관측성 플랫폼입니다.

### 주요 기능

| 기능 | 설명 | 상태 |
|------|------|------|
| **메트릭 수집** | Counter, Gauge, Histogram, Summary | ✅ |
| **분산 추적** | 전체 트레이스 상관관계 및 분석 | ✅ |
| **실시간 알림** | 규칙 기반 알림 엔진 | ✅ |
| **다채널 알림** | Email, Slack, PagerDuty, Webhook | ✅ |
| **웹 대시보드** | 인터랙티브 시각화 | ✅ |
| **스토리지 백엔드** | 인메모리, 파일 기반, 커스텀 | ✅ |
| **익스포터** | Prometheus, OpenTelemetry, Jaeger | ✅ |

### 성능 특성

- **80ns 기록 지연시간**: 메트릭 기록 오버헤드 최소화
- **5M ops/s 처리량**: 초당 500만 메트릭 처리
- **<1% CPU**: 최소 CPU 오버헤드
- **스레드 안전**: TSan으로 검증된 동시성

---

## 메트릭 수집

### 메트릭 타입

#### Counter (카운터)

단조 증가하는 값:

```cpp
#include <kcenon/monitoring/metrics/counter.h>

// 카운터 생성
auto requests = metrics->create_counter("http_requests_total", {
    {"method", "GET"},
    {"endpoint", "/api/users"}
});

// 값 증가
requests->increment();
requests->increment(5);  // 5만큼 증가

// 현재 값 조회
uint64_t count = requests->value();
```

#### Gauge (게이지)

증가/감소할 수 있는 값:

```cpp
#include <kcenon/monitoring/metrics/gauge.h>

// 게이지 생성
auto active_connections = metrics->create_gauge("active_connections");
auto memory_usage = metrics->create_gauge("memory_usage_bytes");

// 값 설정
memory_usage->set(1024 * 1024 * 512);  // 512MB

// 증가/감소
active_connections->increment();
active_connections->decrement();

// 현재 값 조회
double value = active_connections->value();
```

#### Histogram (히스토그램)

값 분포 측정:

```cpp
#include <kcenon/monitoring/metrics/histogram.h>

// 커스텀 버킷으로 히스토그램 생성
auto latency = metrics->create_histogram("request_latency_seconds", {
    .buckets = {0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0, 5.0}
});

// 값 관찰
latency->observe(0.023);  // 23ms

// 통계 조회
auto stats = latency->statistics();
std::cout << "P50: " << stats.percentile(50) << "s" << std::endl;
std::cout << "P99: " << stats.percentile(99) << "s" << std::endl;
```

#### Summary (요약)

미리 계산된 백분위수:

```cpp
#include <kcenon/monitoring/metrics/summary.h>

// 요약 생성
auto response_size = metrics->create_summary("response_size_bytes", {
    .quantiles = {{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}}
});

// 값 관찰
response_size->observe(1024);

// 요약 조회
auto summary = response_size->get_summary();
std::cout << "Median: " << summary.quantile(0.5) << " bytes" << std::endl;
```

### 레이블 및 태그

```cpp
// 레이블이 있는 메트릭 생성
auto http_duration = metrics->create_histogram("http_request_duration_seconds", {
    .labels = {"method", "endpoint", "status_code"}
});

// 레이블 값과 함께 관찰
http_duration->with_labels({
    {"method", "POST"},
    {"endpoint", "/api/orders"},
    {"status_code", "200"}
})->observe(0.045);
```

---

## 분산 추적

### 추적 개념

- **Trace (트레이스)**: 단일 요청의 전체 경로
- **Span (스팬)**: 트레이스 내의 개별 작업 단위
- **Context (컨텍스트)**: 스팬 간 전파되는 메타데이터

### 기본 사용

```cpp
#include <kcenon/monitoring/tracing/tracer.h>

// 트레이서 생성
auto tracer = create_tracer("my_service");

// 새 트레이스 시작
auto span = tracer->start_span("process_request");

// 속성 추가
span->set_attribute("user_id", user_id);
span->set_attribute("request_size", request.size());

// 자식 스팬 생성
{
    auto db_span = span->start_child("database_query");
    db_span->set_attribute("query", "SELECT * FROM users");

    // 데이터베이스 작업 수행...

    db_span->end();  // 또는 스코프 종료 시 자동 종료
}

// 오류 기록
if (!result) {
    span->set_status(span_status::error, "처리 실패");
    span->record_exception(exception);
}

span->end();
```

### 컨텍스트 전파

```cpp
// HTTP 헤더에서 컨텍스트 추출
auto context = tracer->extract_context(http_headers);

// 추출된 컨텍스트로 스팬 시작
auto span = tracer->start_span("handle_request", {
    .parent = context
});

// 다운스트림 서비스로 컨텍스트 주입
tracer->inject_context(span->context(), outgoing_headers);
```

### 자동 계측

```cpp
#include <kcenon/monitoring/tracing/auto_instrumentation.h>

// HTTP 클라이언트 자동 계측
auto http_client = create_traced_http_client(tracer);

// 데이터베이스 자동 계측
auto db_connection = create_traced_connection(tracer, db);
```

---

## 알림 시스템

### 알림 규칙 정의

```cpp
#include <kcenon/monitoring/alerting/alert_rule.h>

// 알림 규칙 생성
auto high_error_rate = create_alert_rule({
    .name = "HighErrorRate",
    .expression = "rate(http_errors_total[5m]) / rate(http_requests_total[5m]) > 0.05",
    .duration = std::chrono::minutes(2),  // 2분간 지속 시 발생
    .severity = alert_severity::critical,
    .labels = {{"team", "backend"}},
    .annotations = {
        {"summary", "높은 HTTP 오류율 감지"},
        {"description", "최근 5분간 오류율이 5%를 초과했습니다."}
    }
});

// 알림 매니저에 등록
alert_manager->add_rule(high_error_rate);
```

### 알림 심각도

| 레벨 | 설명 | 응답 시간 |
|------|------|-----------|
| **critical** | 즉각적인 조치 필요 | 5분 이내 |
| **warning** | 주의 필요 | 30분 이내 |
| **info** | 정보성 알림 | 업무 시간 |

### 알림 채널

#### Slack 통합

```cpp
#include <kcenon/monitoring/alerting/channels/slack_channel.h>

auto slack = create_slack_channel({
    .webhook_url = "https://hooks.slack.com/services/...",
    .channel = "#alerts",
    .username = "AlertBot"
});

alert_manager->add_channel(slack);
```

#### Email 통합

```cpp
#include <kcenon/monitoring/alerting/channels/email_channel.h>

auto email = create_email_channel({
    .smtp_server = "smtp.example.com",
    .smtp_port = 587,
    .from_address = "alerts@example.com",
    .to_addresses = {"oncall@example.com", "team@example.com"},
    .use_tls = true
});

alert_manager->add_channel(email);
```

#### PagerDuty 통합

```cpp
#include <kcenon/monitoring/alerting/channels/pagerduty_channel.h>

auto pagerduty = create_pagerduty_channel({
    .integration_key = "your-pagerduty-key",
    .severity_mapping = {
        {alert_severity::critical, pagerduty_severity::critical},
        {alert_severity::warning, pagerduty_severity::warning}
    }
});

alert_manager->add_channel(pagerduty);
```

### 알림 그룹화 및 억제

```cpp
// 알림 그룹화 설정
alert_manager->set_grouping({
    .group_by = {"alertname", "service"},
    .group_wait = std::chrono::seconds(30),
    .group_interval = std::chrono::minutes(5)
});

// 알림 억제 규칙
alert_manager->add_inhibit_rule({
    .source_match = {{"severity", "critical"}},
    .target_match = {{"severity", "warning"}},
    .equal = {"alertname", "service"}
});
```

---

## 웹 대시보드

### 대시보드 설정

```cpp
#include <kcenon/monitoring/dashboard/dashboard_server.h>

// 대시보드 서버 생성
auto dashboard = create_dashboard_server({
    .port = 9090,
    .enable_auth = true,
    .cors_origins = {"https://admin.example.com"}
});

// 메트릭 및 추적과 연결
dashboard->set_metrics_source(metrics_collector);
dashboard->set_trace_source(tracer);

// 서버 시작
dashboard->start();
```

### REST API

| 엔드포인트 | 메서드 | 설명 |
|------------|--------|------|
| `/api/metrics` | GET | 모든 메트릭 조회 |
| `/api/metrics/{name}` | GET | 특정 메트릭 조회 |
| `/api/traces` | GET | 트레이스 목록 조회 |
| `/api/traces/{id}` | GET | 특정 트레이스 상세 |
| `/api/alerts` | GET | 활성 알림 조회 |
| `/api/health` | GET | 상태 확인 |

### 시각화

- **실시간 차트**: 메트릭 시계열 그래프
- **트레이스 뷰어**: 분산 추적 타임라인
- **알림 목록**: 활성/해결된 알림 목록
- **서비스 맵**: 서비스 간 의존성 시각화

---

## 스토리지 백엔드

### 인메모리 스토리지

```cpp
#include <kcenon/monitoring/storage/memory_storage.h>

auto storage = create_memory_storage({
    .max_samples = 1000000,
    .retention_period = std::chrono::hours(24)
});

metrics_collector->set_storage(storage);
```

### 파일 기반 스토리지

```cpp
#include <kcenon/monitoring/storage/file_storage.h>

auto storage = create_file_storage({
    .data_directory = "/var/lib/monitoring/data",
    .retention_days = 30,
    .compression = compression_type::lz4
});

metrics_collector->set_storage(storage);
```

### 원격 쓰기 (Prometheus)

```cpp
#include <kcenon/monitoring/exporters/prometheus_exporter.h>

auto exporter = create_prometheus_exporter({
    .remote_write_url = "http://prometheus:9090/api/v1/write",
    .batch_size = 1000,
    .flush_interval = std::chrono::seconds(10)
});

metrics_collector->add_exporter(exporter);
```

---

## 성능 특성

### 벤치마크 결과

| 작업 | 처리량 | 지연시간 (p50) | 지연시간 (p99) |
|------|--------|----------------|----------------|
| 카운터 증가 | 10M ops/s | 20ns | 100ns |
| 히스토그램 관찰 | 5M ops/s | 50ns | 200ns |
| 스팬 생성 | 2M ops/s | 100ns | 500ns |
| 컨텍스트 전파 | 1M ops/s | 200ns | 1μs |

### 메모리 사용

| 컴포넌트 | 기본 메모리 | 시나리오 |
|----------|------------|----------|
| 메트릭 수집기 | 10MB | 1000개 메트릭 |
| 추적 수집기 | 50MB | 10000개 활성 스팬 |
| 알림 엔진 | 5MB | 100개 규칙 |
| 대시보드 | 20MB | 기본 설정 |

---

## 통합 기능

### thread_system 통합

```cpp
#include <kcenon/monitoring/integration/thread_metrics.h>

// 스레드 풀 메트릭 자동 수집
auto pool = create_thread_pool(8);
auto metrics = create_thread_pool_metrics(pool);

// 수집되는 메트릭:
// - thread_pool_active_workers
// - thread_pool_queue_size
// - thread_pool_tasks_completed_total
// - thread_pool_task_duration_seconds
```

### logger_system 통합

```cpp
#include <kcenon/monitoring/integration/log_metrics.h>

// 로그 레벨별 메트릭 수집
auto logger = create_logger("my_app");
auto metrics = create_log_metrics(logger);

// 수집되는 메트릭:
// - log_messages_total{level="info|warning|error|..."}
```

### network_system 통합

```cpp
#include <kcenon/monitoring/integration/network_metrics.h>

// 네트워크 메트릭 자동 수집
auto server = create_messaging_server(8080);
auto metrics = create_network_metrics(server);

// 수집되는 메트릭:
// - network_connections_active
// - network_bytes_received_total
// - network_bytes_sent_total
// - network_request_duration_seconds
```

---

## 구성 예시

### 프로덕션 설정

```cpp
// 프로덕션 모니터링 구성
auto monitoring = create_monitoring_system({
    // 메트릭
    .metrics = {
        .enabled = true,
        .flush_interval = std::chrono::seconds(10),
        .default_buckets = {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0}
    },

    // 추적
    .tracing = {
        .enabled = true,
        .sample_rate = 0.1,  // 10% 샘플링
        .max_spans_per_trace = 1000
    },

    // 알림
    .alerting = {
        .enabled = true,
        .evaluation_interval = std::chrono::seconds(15)
    },

    // 대시보드
    .dashboard = {
        .enabled = true,
        .port = 9090,
        .enable_auth = true
    },

    // 스토리지
    .storage = {
        .type = storage_type::file,
        .retention_days = 30
    }
});
```

---

## 참고사항

### 스레드 안전성

- **메트릭**: 원자적 연산으로 스레드 안전
- **추적**: 스팬은 단일 스레드에서 사용 권장
- **알림**: 내부 동기화 있음
- **대시보드**: HTTP 요청별 격리

### 베스트 프랙티스

1. **메트릭 이름 규칙**: `{namespace}_{name}_{unit}` 형식 사용
2. **레이블 카디널리티**: 고카디널리티 레이블 피하기
3. **샘플링**: 고트래픽에서는 추적 샘플링 사용
4. **보존 정책**: 스토리지 비용과 분석 요구 균형

---

**최종 업데이트**: 2025-11-28
**버전**: 3.0

---

Made with ❤️ by 🍀☀🌕🌥 🌊
