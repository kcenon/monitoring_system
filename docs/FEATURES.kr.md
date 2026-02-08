# Monitoring System - 상세 기능

**언어:** [English](README.md) | **한국어**

**최종 업데이트**: 2026-02-08
**버전**: 0.4.0

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
- [플러그인 시스템](#플러그인-시스템)
- [적응형 모니터링](#적응형-모니터링)
- [SIMD 최적화](#simd-최적화)

---

## 핵심 기능

### 시스템 개요

Monitoring System은 C++20 애플리케이션을 위한 개발 중인 관측성 플랫폼입니다.

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

**최종 업데이트**: 2026-02-08
**버전**: 0.4.0

---

## 플러그인 시스템

### 개요

플러그인 시스템은 런타임에 메트릭 수집기를 동적으로 로드할 수 있는 확장 가능한 아키텍처를 제공합니다. 플러그인은 공유 라이브러리(`.so`/`.dylib`/`.dll`)로 컴파일되며, API 버전 호환성 검사가 포함된 크로스 플랫폼 로더를 통해 로드됩니다. 이를 통해 컨테이너 모니터링, 하드웨어 센서 등의 선택적 기능을 필요한 경우에만 로드하여 바이너리 크기와 수집 오버헤드를 줄일 수 있습니다.

**아키텍처 구성요소**:

| 구성요소 | 헤더 | 목적 |
|----------|------|------|
| **collector_plugin** | `plugins/collector_plugin.h` | 모든 수집기 플러그인의 순수 가상 인터페이스 |
| **plugin_api** | `plugins/plugin_api.h` | 크로스 컴파일러 호환을 위한 C ABI 인터페이스 |
| **plugin_loader** | `plugins/plugin_loader.h` | 동적 공유 라이브러리 로딩 및 심볼 해석 |
| **collector_registry** | `plugins/collector_registry.h` | 플러그인 수명 주기 관리를 위한 싱글턴 레지스트리 |

### 플러그인 카테고리

플러그인은 `plugin_category` 열거형으로 분류됩니다:

| 카테고리 | 설명 | 예시 플러그인 |
|----------|------|---------------|
| `system` | 시스템 통합 (스레드, 로거, 컨테이너) | 컨테이너 플러그인 |
| `hardware` | 하드웨어 센서 (GPU, 온도, 배터리, 전력) | 하드웨어 플러그인 |
| `platform` | 플랫폼 특화 (VM, 업타임, 인터럽트) | - |
| `network` | 네트워크 메트릭 (연결성, 대역폭) | - |
| `process` | 프로세스 수준 메트릭 (리소스, 성능) | - |
| `custom` | 사용자 정의 플러그인 | 커스텀 구현 |

### 제공되는 플러그인 타입

#### 컨테이너 플러그인

컨테이너 플러그인(`plugins/container/container_plugin.h`)은 Docker, Kubernetes, cgroup 기반 컨테이너 런타임 모니터링을 제공합니다.

**지원 런타임**: Docker, containerd, Podman, CRI-O (자동 감지 지원)

**메트릭**: 컨테이너 CPU/메모리/네트워크/I/O, 실행 중인 컨테이너 수, Kubernetes 파드/디플로이먼트 메트릭, cgroup CPU 시간 및 메모리 사용량/제한.

```cpp
#include <kcenon/monitoring/plugins/container/container_plugin.h>

using namespace kcenon::monitoring::plugins;

// Create with custom configuration
container_plugin_config config;
config.enable_docker = true;
config.enable_kubernetes = false;
config.docker_socket = "/var/run/docker.sock";
config.collect_network_metrics = true;

auto plugin = container_plugin::create(config);

// Check container environment before loading
if (container_plugin::is_running_in_container()) {
    registry.register_plugin(std::move(plugin));
}

// Detect runtime automatically
auto runtime = container_plugin::detect_runtime();
```

#### 하드웨어 플러그인

하드웨어 플러그인(`plugins/hardware/hardware_plugin.h`)은 배터리, 전력 소비, 온도, GPU 모니터링을 데스크톱/노트북 환경에 제공합니다.

**메트릭**: 배터리 잔량/건강/사이클, 전력 소비(와트/RAPL), CPU/GPU/메인보드 온도, GPU 활용도/VRAM/클럭/팬 속도.

```cpp
#include <kcenon/monitoring/plugins/hardware/hardware_plugin.h>

using namespace kcenon::monitoring::plugins;

// Create with custom configuration
hardware_plugin_config config;
config.enable_battery = true;
config.enable_temperature = true;
config.enable_gpu = true;
config.gpu_collect_utilization = true;
config.gpu_collect_memory = true;

auto plugin = hardware_plugin::create(config);

// Check individual sensor availability
if (plugin->is_battery_available()) {
    // Battery metrics will be collected
}
if (plugin->is_gpu_available()) {
    // GPU metrics will be collected
}
```

### 플러그인 로딩 및 등록 흐름

플러그인 수명 주기는 다음 순서를 따릅니다:

1. **로드**: `dynamic_plugin_loader`가 공유 라이브러리를 열고 `create_plugin`, `destroy_plugin`, `get_plugin_info` 심볼을 해석
2. **검증**: `plugin_api_metadata.api_version`을 `PLUGIN_API_VERSION`과 비교하여 API 버전 호환성 확인
3. **생성**: 해석된 `create_plugin` 함수를 통해 플러그인 인스턴스 생성
4. **등록**: `collector_registry`에 추가하여 소유권 관리
5. **초기화**: 구성 매개변수와 함께 `initialize()` 호출
6. **수집**: `interval()`에 따라 주기적으로 `collect()` 호출
7. **종료**: 플러그인 파괴 및 라이브러리 언로드 전에 `shutdown()` 호출

```cpp
// Load and register a plugin from a shared library
auto& registry = collector_registry::instance();
bool loaded = registry.load_plugin("/path/to/libmy_plugin.so");

if (!loaded) {
    std::cerr << "Error: " << registry.get_plugin_loader_error() << std::endl;
}

// Access a registered plugin
if (auto* plugin = registry.get_plugin("my_collector")) {
    auto metrics = plugin->collect();
}

// List plugins by category
auto hw_plugins = registry.get_plugins_by_category(plugin_category::hardware);

// Factory-based lazy registration
registry.register_factory<my_collector_plugin>("my_collector");

// Initialize all plugins at once
registry.initialize_all(config);

// Shutdown all plugins
registry.shutdown_all();
```

### 커스텀 플러그인 구현

플러그인은 세 개의 C 링키지 함수를 내보내야 합니다. `IMPLEMENT_PLUGIN` 매크로로 간소화할 수 있습니다:

```cpp
#include <kcenon/monitoring/plugins/plugin_api.h>
#include <kcenon/monitoring/plugins/collector_plugin.h>

class my_sensor_plugin : public kcenon::monitoring::collector_plugin {
public:
    auto name() const -> std::string_view override { return "my_sensor"; }

    auto collect() -> std::vector<metric> override {
        // Collect metrics from custom hardware/source
        return { /* ... */ };
    }

    auto interval() const -> std::chrono::milliseconds override {
        return std::chrono::seconds(5);
    }

    auto is_available() const -> bool override {
        return true; // Check hardware availability
    }

    auto get_metric_types() const -> std::vector<std::string> override {
        return {"gauge"};
    }
};

// Export required C ABI functions
IMPLEMENT_PLUGIN(
    my_sensor_plugin,    // Plugin class
    "my_sensor",         // Plugin name
    "1.0.0",             // Version
    "My Sensor Plugin",  // Description
    "Author Name",       // Author
    "hardware"           // Category
)
```

> [!TIP]
> 자세한 플러그인 개발 지침은 [Plugin Development Guide](plugin_development_guide.md)를 참조하세요. 전체 API 참조는 [Plugin API Reference](plugin_api_reference.md)를, 아키텍처 세부 사항은 [Plugin Architecture](plugin_architecture.md)를 참조하세요.

---

## 적응형 모니터링

### 개요

적응형 모니터링 시스템은 현재 시스템 리소스 사용률에 따라 수집 간격, 샘플링 비율, 메트릭 세분화 수준을 자동으로 조정합니다. 고부하 상황에서는 모니터링 오버헤드를 최소화하고, 유휴 시에는 상세한 데이터를 제공합니다.

**주요 기능**:
- 자동 부하 수준 감지 (idle, low, moderate, high, critical)
- 수준별 설정 가능한 수집 간격 및 샘플링 비율
- 세 가지 적응 전략: conservative, balanced, aggressive
- 진동 방지를 위한 히스테리시스 및 쿨다운 메커니즘
- 안정적인 부하 추정을 위한 지수 평활법
- RAII 스코프 관리를 통한 스레드 안전 운영

### 적응 전략

| 전략 | 동작 | 사용 사례 |
|------|------|-----------|
| `conservative` | 유효 부하를 20% 줄여 시스템 안정성 우선 | 엄격한 SLA가 있는 프로덕션 서버 |
| `balanced` | 조정 없이 원시 부하 메트릭 직접 사용 | 범용 모니터링 |
| `aggressive` | 유효 부하를 20% 높여 모니터링 상세도 유지 | 개발 및 디버깅 환경 |

### 부하 수준 및 기본값

| 부하 수준 | CPU 임계값 | 기본 간격 | 기본 샘플링 비율 |
|-----------|-----------|----------|-----------------|
| `idle` | < 20% | 100ms | 100% |
| `low` | 20-40% | 250ms | 80% |
| `moderate` | 40-60% | 500ms | 50% |
| `high` | 60-80% | 1000ms | 20% |
| `critical` | > 80% | 5000ms | 10% |

### 기본 사용법

```cpp
#include <kcenon/monitoring/adaptive/adaptive_monitor.h>

using namespace kcenon::monitoring;

// Configure adaptive behavior
adaptive_config config;
config.strategy = adaptation_strategy::balanced;
config.idle_interval = std::chrono::milliseconds(100);
config.critical_interval = std::chrono::milliseconds(5000);
config.smoothing_factor = 0.7;
config.enable_hysteresis = true;
config.hysteresis_margin = 5.0;

// Register a collector with the global adaptive monitor
auto& monitor = global_adaptive_monitor();
monitor.register_collector("my_service", my_collector, config);
monitor.start();

// Check adaptation statistics
auto stats = monitor.get_collector_stats("my_service");
if (stats.is_ok()) {
    auto s = stats.value();
    std::cout << "Current load: " << static_cast<int>(s.current_load_level) << std::endl;
    std::cout << "Sampling rate: " << s.current_sampling_rate << std::endl;
    std::cout << "Total adaptations: " << s.total_adaptations << std::endl;
}

// Stop when done
monitor.stop();
```

### RAII 스코프 관리

`adaptive_scope` 클래스는 자동 등록 및 정리를 제공합니다:

```cpp
#include <kcenon/monitoring/adaptive/adaptive_monitor.h>

using namespace kcenon::monitoring;

{
    // Automatically registers collector with the global adaptive monitor
    adaptive_scope scope("my_service", my_collector, config);

    if (scope.is_registered()) {
        // Collector is active and will be adaptively monitored
    }
    // Collector is automatically unregistered when scope exits
}
```

### 히스테리시스 및 쿨다운

임계값 경계에서의 급격한 진동을 방지하기 위해:

- **히스테리시스**: 부하가 임계값을 `hysteresis_margin`(기본: 5%)만큼 초과해야 수준 변경이 발생
- **쿨다운**: 수준 변경 사이에 최소 `cooldown_period`(기본: 1000ms)가 경과해야 함

```cpp
adaptive_config config;
config.enable_hysteresis = true;
config.hysteresis_margin = 5.0;      // 5% margin above/below thresholds
config.enable_cooldown = true;
config.cooldown_period = std::chrono::milliseconds(1000);
```

---

## SIMD 최적화

### 개요

SIMD 집계기는 SIMD(Single Instruction Multiple Data) 명령어를 사용하여 메트릭 데이터에 대한 고성능 통계 연산을 제공합니다. 런타임에 사용 가능한 명령어 세트를 자동 감지하며, SIMD를 사용할 수 없거나 데이터셋이 너무 작아 이점이 없는 경우 스칼라 연산으로 자동 대체됩니다.

**지원 명령어 세트**:

| 명령어 세트 | 플랫폼 | 벡터 폭 (double) |
|-------------|--------|-------------------|
| AVX2 | x86_64 | 4 |
| SSE2 | x86_64 | 2 |
| NEON | ARM64 (aarch64) | 2 |
| 스칼라 대체 | 모든 플랫폼 | 1 |

### 통계 연산

| 연산 | 메서드 | 설명 |
|------|--------|------|
| 합계 | `sum(data)` | 모든 요소의 합 |
| 평균 | `mean(data)` | 산술 평균 |
| 최솟값 | `min(data)` | 최솟값 |
| 최댓값 | `max(data)` | 최댓값 |
| 분산 | `variance(data)` | 표본 분산 |
| 요약 | `compute_summary(data)` | 전체 통계 요약 (count, sum, mean, variance, std_dev, min, max) |

### 기본 사용법

```cpp
#include <kcenon/monitoring/optimization/simd_aggregator.h>

using namespace kcenon::monitoring;

// Create with default configuration (SIMD enabled, auto-detect)
auto aggregator = make_simd_aggregator();

// Or with custom configuration
simd_config config;
config.enable_simd = true;
config.vector_size = 8;
config.alignment = 32;
config.use_fma = true;

auto aggregator = make_simd_aggregator(config);

// Perform operations
std::vector<double> data = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};

auto sum_result = aggregator->sum(data);
if (sum_result.is_ok()) {
    std::cout << "Sum: " << sum_result.value() << std::endl;  // 36.0
}

auto mean_result = aggregator->mean(data);
if (mean_result.is_ok()) {
    std::cout << "Mean: " << mean_result.value() << std::endl;  // 4.5
}

// Full statistical summary
auto summary = aggregator->compute_summary(data);
if (summary.is_ok()) {
    auto s = summary.value();
    std::cout << "Count: " << s.count << std::endl;
    std::cout << "Sum: " << s.sum << std::endl;
    std::cout << "Mean: " << s.mean << std::endl;
    std::cout << "Std Dev: " << s.std_dev << std::endl;
    std::cout << "Min: " << s.min_val << std::endl;
    std::cout << "Max: " << s.max_val << std::endl;
}
```

### 런타임 기능 감지

```cpp
auto aggregator = make_simd_aggregator();

// Query available SIMD instruction sets
const auto& caps = aggregator->get_capabilities();
std::cout << "SSE2: " << caps.sse2_available << std::endl;
std::cout << "AVX2: " << caps.avx2_available << std::endl;
std::cout << "NEON: " << caps.neon_available << std::endl;

// Self-test to verify SIMD correctness
auto test_result = aggregator->test_simd();
if (test_result.is_ok() && test_result.value()) {
    std::cout << "SIMD self-test passed" << std::endl;
}
```

### 성능 통계

집계기는 SIMD 경로와 스칼라 경로의 사용 빈도를 추적합니다:

```cpp
auto aggregator = make_simd_aggregator();

// Perform several operations...
aggregator->sum(data);
aggregator->mean(data);
aggregator->min(data);

// Check utilization
const auto& stats = aggregator->get_statistics();
std::cout << "Total operations: " << stats.total_operations << std::endl;
std::cout << "SIMD operations: " << stats.simd_operations << std::endl;
std::cout << "Scalar operations: " << stats.scalar_operations << std::endl;
std::cout << "SIMD utilization: " << stats.get_simd_utilization() << "%" << std::endl;
std::cout << "Elements processed: " << stats.total_elements_processed << std::endl;

// Reset statistics
aggregator->reset_statistics();
```

### 성능 특성

- SIMD 경로는 `2 * vector_size` 요소보다 큰 데이터셋에 대해 자동으로 선택됩니다
- 작은 데이터셋은 SIMD 설정 오버헤드를 피하기 위해 스칼라 경로를 사용합니다
- AVX2는 사이클당 4개의 double을 처리합니다; SSE2와 NEON은 사이클당 2개의 double을 처리합니다
- SIMD 벡터를 완전히 채우지 못하는 나머지 요소는 자동으로 처리됩니다
- 모든 연산은 빈 입력에 대한 적절한 오류 처리와 함께 `Result<T>`를 반환합니다

### 설정 옵션

| 옵션 | 기본값 | 설명 |
|------|--------|------|
| `enable_simd` | true | SIMD 가속 활성화/비활성화 |
| `vector_size` | 8 | 처리를 위한 SIMD 벡터 폭 |
| `alignment` | 32 | SIMD 연산을 위한 메모리 정렬 (바이트) |
| `use_fma` | true | 사용 가능한 경우 fused multiply-add 사용 |

> [!NOTE]
> SIMD 집계기는 지원되는 SIMD 명령어 세트가 없는 플랫폼이나 설정을 통해 SIMD를 명시적으로 비활성화한 경우 자동으로 스칼라 연산으로 대체됩니다. 크로스 플랫폼 호환을 위한 코드 변경이 필요하지 않습니다.

---

Made with ❤️ by 🍀☀🌕🌥 🌊
