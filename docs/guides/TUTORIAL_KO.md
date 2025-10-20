# Monitoring System 튜토리얼

> **Language:** [English](TUTORIAL.md) | **한국어**

## 소개

Monitoring System 튜토리얼에 오신 것을 환영합니다! 이 가이드는 기본 설정부터 고급 기능까지 애플리케이션에서 모니터링 시스템을 사용하는 방법을 안내합니다.

## 목차

1. [시작하기](#시작하기)
2. [기본 모니터링](#기본-모니터링)
3. [분산 추적](#분산-추적)
4. [건강 모니터링](#건강-모니터링)
5. [신뢰성 기능](#신뢰성-기능)
6. [Result 패턴을 사용한 에러 처리](#result-패턴을-사용한-에러-처리)
7. [모범 사례](#모범-사례)

---

## 시작하기

### 전제 조건

- C++17 이상 컴파일러
- CMake 3.15 이상
- 스레드 지원

### 설치

1. 저장소 클론:
```bash
git clone <repository-url>
cd monitoring_system
```

2. 프로젝트 빌드:
```bash
mkdir build
cd build
cmake ..
make
```

3. 테스트 실행:
```bash
./tests/monitoring_system_tests
```

### 프로젝트에 포함

CMakeLists.txt에 추가:
```cmake
add_subdirectory(monitoring_system)
target_link_libraries(your_app PRIVATE monitoring_system)
```

코드에 헤더 포함:
```cpp
#include <monitoring/monitoring.h>
#include <monitoring/performance/performance_monitor.h>
```

---

## 기본 모니터링

### 단계 1: 모니터링 시스템 초기화

```cpp
#include <monitoring/monitoring.h>

using namespace monitoring_system;

// 모니터링 구성
monitoring_config config;
config.history_size = 1000;
config.collection_interval = std::chrono::seconds(1);

// 모니터링 인스턴스 빌드
monitoring_builder builder;
auto monitoring_result = builder
    .with_history_size(config.history_size)
    .with_collection_interval(config.collection_interval)
    .enable_compression(true)
    .build();

if (!monitoring_result) {
    // 에러 처리
    std::cerr << "Failed: " << monitoring_result.get_error().message << std::endl;
    return;
}

auto& monitoring = *monitoring_result.value();
```

### 단계 2: Collectors 추가

```cpp
// 성능 모니터 추가
auto perf_monitor = std::make_unique<performance_monitor>("my_app");
monitoring.add_collector(std::move(perf_monitor));

// 모니터링 시작
monitoring.start();
```

### 단계 3: 메트릭 기록

```cpp
// 맞춤 메트릭 기록
monitoring.record_metric("request_count", 1.0, metric_unit::count);
monitoring.record_metric("response_time", 45.3, metric_unit::milliseconds);
monitoring.record_metric("memory_usage", 128.5, metric_unit::megabytes);

// 자동 기간 측정을 위한 스코프 타이머 사용
{
    auto timer = perf_monitor->time_operation("database_query");
    // ... 데이터베이스 쿼리 수행 ...
} // 타이머가 소멸될 때 자동으로 기간 기록
```

### 단계 4: 메트릭 쿼리

```cpp
// 현재 메트릭의 스냅샷 가져오기
auto snapshot = monitoring.get_snapshot();
if (snapshot) {
    for (const auto& [name, data] : snapshot.value().metrics) {
        std::cout << name << ": " << data.values.size() << " samples" << std::endl;
    }
}

// 통계 가져오기
auto stats = monitoring.get_statistics();
std::cout << "Metrics recorded: " << stats.metrics_recorded << std::endl;
```

### 완전한 예제

완전한 작동 예제는 [basic_monitoring_example.cpp](basic_monitoring_example.cpp)를 참조하세요.

---

## 분산 추적

### Span 생성

```cpp
#include <monitoring/tracing/distributed_tracer.h>

distributed_tracer tracer;

// 루트 span 시작
auto root_span = tracer.start_span("process_request", "frontend_service");
if (root_span) {
    auto span = root_span.value();

    // 태그 추가
    span->tags["http.method"] = "GET";
    span->tags["http.url"] = "/api/users";
    span->tags["user.id"] = "12345";

    // baggage 추가 (자식에게 전파됨)
    span->baggage["session.id"] = "abc123";

    // span 완료
    tracer.finish_span(span);
}
```

### 부모-자식 관계

```cpp
// 자식 span 생성
auto child_span = tracer.start_child_span(*parent_span, "database_query");
if (child_span) {
    auto span = child_span.value();
    span->tags["db.type"] = "postgresql";
    span->tags["db.statement"] = "SELECT * FROM users";

    // 연산 수행...

    tracer.finish_span(span);
}
```

### 컨텍스트 전파

```cpp
// 전파를 위한 컨텍스트 추출
auto context = tracer.extract_context(*span);

// HTTP 헤더에 주입
std::map<std::string, std::string> headers;
tracer.inject_context(context, headers);

// 수신 서비스에서 컨텍스트 추출
auto extracted = tracer.extract_context_from_carrier(headers);
if (extracted) {
    // 추적 계속
    auto continued_span = tracer.start_span_from_context(
        extracted.value(),
        "continued_operation"
    );
}
```

### 편의를 위한 매크로 사용

```cpp
void process_request() {
    TRACE_SPAN("process_request");

    // Span이 자동으로 생성되며 함수 종료 시 완료됨

    validate_input();

    {
        TRACE_CHILD_SPAN(*_scoped_span, "database_operation");
        // 이 블록을 위한 자식 span
        query_database();
    }
}
```

### 완전한 예제

완전한 작동 예제는 [distributed_tracing_example.cpp](distributed_tracing_example.cpp)를 참조하세요.

---

## 건강 모니터링

### 건강 검사 설정

```cpp
#include <monitoring/health/health_monitor.h>

health_monitor monitor;

// liveness 검사 등록
monitor.register_check("database",
    health_check_builder()
        .with_name("database_check")
        .with_type(health_check_type::liveness)
        .with_check([]() {
            // 데이터베이스 연결 확인
            if (can_connect_to_database()) {
                return health_check_result::healthy("Database connected");
            }
            return health_check_result::unhealthy("Cannot connect to database");
        })
        .with_timeout(5s)
        .critical(true)
        .build()
);

// readiness 검사 등록
monitor.register_check("api",
    health_check_builder()
        .with_name("api_check")
        .with_type(health_check_type::readiness)
        .with_check([]() {
            // API 준비 여부 확인
            if (api_initialized && !overloaded) {
                return health_check_result::healthy("API ready");
            }
            if (overloaded) {
                return health_check_result::degraded("High load");
            }
            return health_check_result::unhealthy("API not ready");
        })
        .build()
);
```

### 건강 의존성

```cpp
// 서비스 간 의존성 정의
monitor.add_dependency("api", "database");
monitor.add_dependency("api", "cache");

// 의존성이 순서대로 확인됨
auto results = monitor.check_all();
```

### 복구 핸들러

```cpp
// 자동 복구 등록
monitor.register_recovery_handler("database",
    []() -> bool {
        // 재연결 시도
        return reconnect_to_database();
    }
);
```

### 건강 엔드포인트

```cpp
// 건강 검사를 위한 HTTP 엔드포인트 생성
void health_endpoint(const http_request& req, http_response& res) {
    auto health = monitor.get_overall_status();

    if (health == health_status::healthy) {
        res.status = 200;
        res.body = "OK";
    } else if (health == health_status::degraded) {
        res.status = 200;
        res.body = "DEGRADED";
    } else {
        res.status = 503;
        res.body = "UNHEALTHY";
    }
}
```

### 완전한 예제

완전한 작동 예제는 [health_reliability_example.cpp](health_reliability_example.cpp)를 참조하세요.

---

## 신뢰성 기능

### Circuit Breakers

실패하는 서비스로의 호출을 중단하여 연쇄 실패를 방지:

```cpp
#include <monitoring/reliability/circuit_breaker.h>

// circuit breaker 구성
circuit_breaker_config config;
config.failure_threshold = 5;        // 5번 실패 후 열림
config.reset_timeout = 30s;          // 30초 후 재시도
config.success_threshold = 2;        // 닫기 위해 2번 성공 필요

circuit_breaker<std::string> breaker("external_api", config);

// circuit breaker 사용
auto result = breaker.execute(
    []() {
        // 외부 서비스 호출
        return call_external_api();
    },
    []() {
        // circuit이 열렸을 때 fallback
        return result<std::string>::success("cached_response");
    }
);
```

### Retry Policies

실패한 연산을 자동으로 재시도:

```cpp
#include <monitoring/reliability/retry_policy.h>

// retry 구성
retry_config config;
config.max_attempts = 3;
config.strategy = retry_strategy::exponential_backoff;
config.initial_delay = 100ms;
config.max_delay = 5s;

retry_policy<std::string> retry(config);

// retry와 함께 실행
auto result = retry.execute([]() {
    return potentially_failing_operation();
});
```

### Error Boundaries

시스템 전체 실패를 방지하기 위해 에러 격리:

```cpp
#include <monitoring/reliability/error_boundary.h>

error_boundary boundary("critical_section");

// 에러 핸들러 설정
boundary.set_error_handler([](const error_info& error) {
    log_error("Error in critical section: {}", error.message);
    send_alert(error);
});

// boundary 내에서 실행
auto result = boundary.execute<int>([]() {
    return risky_operation();
});
```

### 신뢰성 기능 결합

```cpp
// 계층화된 신뢰성: retry → circuit breaker → error boundary
auto reliable_operation = [&]() {
    return error_boundary.execute<std::string>([&]() {
        return circuit_breaker.execute([&]() {
            return retry_policy.execute([&]() {
                return external_service_call();
            });
        });
    });
};
```

---

## Result 패턴을 사용한 에러 처리

### 기본 사용

Result 패턴은 예외 없이 명시적인 에러 처리를 제공합니다:

```cpp
#include <monitoring/core/result_types.h>

// 실패할 수 있는 함수
result<int> parse_config_value(const std::string& str) {
    try {
        int value = std::stoi(str);
        return result<int>::success(value);
    } catch (...) {
        return make_error<int>(
            monitoring_error_code::invalid_argument,
            "Cannot parse integer: " + str
        );
    }
}

// result 사용
auto result = parse_config_value("42");
if (result) {
    std::cout << "Value: " << result.value() << std::endl;
} else {
    std::cout << "Error: " << result.get_error().message << std::endl;
}
```

### 연산 체이닝

```cpp
// and_then으로 연산 체인
auto process = parse_config_value("100")
    .and_then([](int value) {
        if (value < 0) {
            return make_error<int>(
                monitoring_error_code::out_of_range,
                "Value must be positive"
            );
        }
        return result<int>::success(value * 2);
    })
    .map([](int value) {
        return value + 10;
    });

// or_else로 에러 복구
auto with_default = parse_config_value("invalid")
    .or_else([](const error_info&) {
        return result<int>::success(42); // 기본값
    });
```

### API에서 Result

```cpp
class DatabaseClient {
public:
    result<User> get_user(int id) {
        if (!connected_) {
            return make_error<User>(
                monitoring_error_code::unavailable,
                "Database not connected"
            );
        }

        auto query_result = execute_query("SELECT * FROM users WHERE id = ?", id);
        if (!query_result) {
            return make_error<User>(
                query_result.get_error().code,
                "Query failed: " + query_result.get_error().message
            );
        }

        User user;
        // ... 쿼리 결과에서 user 파싱 ...
        return result<User>::success(user);
    }
};
```

### 완전한 예제

더 많은 예제는 [result_pattern_example.cpp](result_pattern_example.cpp)를 참조하세요.

---

## 모범 사례

### 1. 리소스 관리

자동 리소스 관리를 위해 항상 RAII 사용:

```cpp
// 좋음: 자동 정리
{
    scoped_timer timer(&profiler, "operation");
    perform_operation();
} // 타이머가 자동으로 기간 기록

// 좋음: 스코프 span
{
    TRACE_SPAN("process_batch");
    process_batch();
} // Span이 자동으로 완료됨
```

### 2. 에러 처리

항상 Results 확인:

```cpp
// 좋음: result 확인
auto result = operation();
if (!result) {
    log_error("Operation failed: {}", result.get_error().message);
    return result; // 에러 전파
}
use_value(result.value());

// 나쁨: 에러 무시
operation(); // Result 무시됨!
```

### 3. 구성

사용 전에 구성 검증:

```cpp
monitoring_config config;
// ... config 값 설정 ...

auto validation = config.validate();
if (!validation) {
    log_error("Invalid config: {}", validation.get_error().message);
    return;
}
```

### 4. 성능

보수적인 설정으로 시작하고 측정에 기반하여 조정:

```cpp
// 보수적으로 시작
config.sampling_rate = 0.01;  // 1% 샘플링
config.collection_interval = 10s;

// 오버헤드 모니터링
auto overhead = monitor.get_overhead_percent();
if (overhead < 2.0) {
    // 더 많은 세부 정보 감당 가능
    config.sampling_rate = 0.1;  // 10% 샘플링
}
```

### 5. 테스트

단위 테스트에서 모니터링 테스트:

```cpp
TEST(MyService, MetricsRecorded) {
    MyService service;
    service.process_request();

    auto metrics = service.get_metrics();
    EXPECT_TRUE(metrics.has_value());
    EXPECT_GT(metrics.value().size(), 0);
}
```

### 6. 프로덕션 배포

환경별로 다른 구성 사용:

```cpp
monitoring_config get_config(Environment env) {
    switch (env) {
        case Environment::Development:
            return dev_config();      // 전체 세부 정보, 샘플링 없음
        case Environment::Staging:
            return staging_config();   // 중간 세부 정보
        case Environment::Production:
            return production_config(); // 낮은 오버헤드에 최적화
    }
}
```

### 7. 문제 해결

문제 조사 시 디버그 로깅 활성화:

```cpp
#ifdef DEBUG
    monitoring.enable_debug_logging(true);
    monitoring.set_log_level(log_level::trace);
#endif
```

---

## 고급 주제

### 맞춤 Collectors

맞춤 메트릭 collectors 생성:

```cpp
class CustomCollector : public metrics_collector {
public:
    std::string get_name() const override {
        return "custom_collector";
    }

    result<metrics_snapshot> collect() override {
        metrics_snapshot snapshot;

        // 맞춤 메트릭 수집
        metric_data data;
        data.name = "custom_metric";
        data.unit = metric_unit::count;
        data.values.push_back({get_custom_value(), now()});

        snapshot.metrics["custom_metric"] = data;
        return result<metrics_snapshot>::success(snapshot);
    }
};

// 맞춤 collector 등록
monitoring.add_collector(std::make_unique<CustomCollector>());
```

### 맞춤 Exporters

백엔드를 위한 맞춤 exporters 생성:

```cpp
class CustomExporter : public metrics_exporter {
public:
    result<bool> export_batch(const std::vector<metric_data>& metrics) override {
        // 백엔드로 메트릭 전송
        for (const auto& metric : metrics) {
            send_to_backend(metric);
        }
        return result<bool>::success(true);
    }
};
```

### 기존 시스템과 통합

#### Prometheus 통합

```cpp
#include <monitoring/export/metric_exporters.h>

prometheus_exporter exporter;
exporter.serve_metrics("/metrics", 9090);

// http://localhost:9090/metrics에서 메트릭 사용 가능
```

#### OpenTelemetry 통합

```cpp
#include <monitoring/adapters/opentelemetry_adapter.h>

opentelemetry_adapter adapter;
adapter.export_traces(spans);
adapter.export_metrics(metrics);
```

---

## 문제 해결

### 높은 메모리 사용량

1. 큐 크기 확인:
```cpp
auto stats = monitoring.get_queue_stats();
if (stats.queue_depth > 10000) {
    // 큐 백업 - flush 빈도 증가
    config.flush_interval = 1s;
}
```

2. 메모리 제한 활성화:
```cpp
config.max_memory_mb = 50;
config.memory_warning_threshold = 0.8;
```

### 메트릭 누락

1. collectors가 활성화되었는지 확인:
```cpp
for (const auto& collector : monitoring.get_collectors()) {
    std::cout << collector->get_name() << ": "
              << (collector->is_enabled() ? "enabled" : "disabled")
              << std::endl;
}
```

2. 샘플링 비율 확인:
```cpp
if (config.sampling_rate < 0.01) {
    // 매우 낮은 샘플링 - 이벤트를 놓칠 수 있음
    config.sampling_rate = 0.1;
}
```

### 성능 문제

1. 적응형 최적화 사용:
```cpp
adaptive_optimizer optimizer;
optimizer.set_target_overhead(5.0); // 최대 5% CPU
optimizer.enable_auto_tuning(true);
```

2. 모니터링 시스템 프로파일링:
```cpp
auto profile = monitoring.profile_overhead();
std::cout << "Monitoring overhead: " << profile.cpu_percent << "%" << std::endl;
```

더 많은 문제 해결 팁은 [Troubleshooting Guide](../docs/TROUBLESHOOTING.md)를 참조하세요.

---

## 추가 리소스

- [API Reference](../docs/API_REFERENCE.md) - 완전한 API 문서
- [Architecture Guide](../docs/ARCHITECTURE_GUIDE.md) - 시스템 설계 및 아키텍처
- [Performance Tuning](../docs/PERFORMANCE_TUNING.md) - 최적화 가이드
- [Examples](.) - 작동하는 코드 예제

---

## 도움 받기

- [문서](../docs/) 확인
- [예제](.) 살펴보기
- 사용 패턴을 위해 [테스트](../tests/) 검토
- GitHub에 이슈 보고

---

## 결론

이제 다음을 수행할 수 있는 지식을 갖추었습니다:
- ✅ 기본 모니터링 설정
- ✅ 분산 추적 구현
- ✅ 건강 검사 구성
- ✅ 신뢰성 기능 사용
- ✅ 에러를 적절히 처리
- ✅ 모범 사례 준수

기본 예제로 시작하고 필요에 따라 점진적으로 더 많은 기능을 추가하세요. 모니터링 오버헤드를 측정하고 그에 따라 구성을 조정하는 것을 잊지 마세요.

즐거운 모니터링 되세요! 🎉

---

*Last Updated: 2025-10-20*
