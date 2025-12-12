# Monitoring System API 레퍼런스

> **Language:** [English](API_REFERENCE.md) | **한국어**

**Phase 4 - 현재 구현 상태**

이 문서는 monitoring system Phase 4에서 **실제로 구현된** API와 인터페이스를 설명합니다. *(Stub Implementation)*으로 표시된 항목은 기능적 인터페이스를 제공하지만 향후 개발을 위한 기반으로 단순화된 구현을 가지고 있습니다.

## 목차

1. [핵심 컴포넌트](#핵심-컴포넌트) ✅ **구현 완료**
2. [모니터링 인터페이스](#모니터링-인터페이스) ✅ **구현 완료**
3. [성능 모니터링](#성능-모니터링) ✅ **구현 완료**
4. [분산 추적](#분산-추적) ⚠️ **Stub 구현**
5. [스토리지 백엔드](#스토리지-백엔드) ⚠️ **Stub 구현**
6. [안정성 기능](#안정성-기능) ⚠️ **Stub 구현**
7. [테스트 커버리지](#테스트-커버리지) ✅ **37개 테스트 통과**

---

## 핵심 컴포넌트

### Result 타입 ✅ **완전 구현**
**헤더:** `include/kcenon/monitoring/core/result_types.h`

#### `result<T>`
예외 없이 오류 처리를 위한 모나딕 result 타입. **13개의 통과 테스트로 완전히 테스트됨.**

```cpp
template<typename T>
class result {
public:
    // 생성자
    result(const T& value);
    result(T&& value);
    result(const error_info& error);

    // 값 포함 여부 확인
    bool has_value() const;
    bool is_ok() const;
    bool is_error() const;
    explicit operator bool() const;

    // 값 접근
    T& value();
    const T& value() const;
    T value_or(const T& default_value) const;

    // 오류 접근
    error_info get_error() const;

    // 모나딕 연산
    template<typename F>
    auto map(F&& f) const;

    template<typename F>
    auto and_then(F&& f) const;
};
```

#### `result_void`
값을 반환하지 않는 연산을 위한 특수화된 result 타입.

```cpp
class result_void {
public:
    static result_void success();
    static result_void error(const error_info& error);

    bool is_ok() const;
    bool is_error() const;
    error_info get_error() const;
};
```

**사용 예제:**
```cpp
result<int> divide(int a, int b) {
    if (b == 0) {
        return result<int>(error_info{
            .code = monitoring_error_code::invalid_argument,
            .message = "Division by zero"
        });
    }
    return result<int>(a / b);
}

// 연산 체이닝
auto result = divide(10, 2)
    .map([](int x) { return x * 2; })
    .and_then([](int x) { return divide(x, 3); });
```

### 스레드 컨텍스트 ✅ **완전 구현**
**헤더:** `include/kcenon/monitoring/context/thread_context.h`

#### `thread_context`
상관관계 및 추적을 위한 스레드 로컬 컨텍스트를 관리합니다. **6개의 통과 테스트로 완전히 테스트됨.**

```cpp
class thread_context {
public:
    // 현재 스레드 컨텍스트 가져오기
    static context_metadata& current();

    // 선택적 요청 ID로 새 컨텍스트 생성
    static context_metadata& create(const std::string& request_id = "");

    // 현재 컨텍스트 삭제
    static void clear();

    // 고유 ID 생성
    static std::string generate_request_id();
    static std::string generate_correlation_id();

    // 컨텍스트 존재 여부 확인
    static bool has_current();
};
```

#### `context_metadata`
추적 및 상관관계를 위한 스레드 로컬 컨텍스트 정보.

```cpp
struct context_metadata {
    std::string request_id;
    std::string correlation_id;
    std::string trace_id;
    std::string span_id;
    std::chrono::system_clock::time_point created_at;
    std::unordered_map<std::string, std::string> baggage;
};
```

### 의존성 주입 컨테이너 ✅ **완전 구현**
**헤더:** `include/kcenon/monitoring/di/di_container.h`

#### `di_container`
서비스 인스턴스 관리를 위한 경량 의존성 주입 컨테이너. **9개의 통과 테스트로 완전히 테스트됨.**

```cpp
class di_container {
public:
    // 서비스 등록
    template<typename T>
    void register_transient(std::function<std::shared_ptr<T>()> factory);

    template<typename T>
    void register_singleton(std::function<std::shared_ptr<T>()> factory);

    template<typename T>
    void register_singleton_instance(std::shared_ptr<T> instance);

    // 이름이 지정된 서비스 등록
    template<typename T>
    void register_named(const std::string& name,
                       std::function<std::shared_ptr<T>()> factory);

    // 서비스 해석
    template<typename T>
    std::shared_ptr<T> resolve();

    template<typename T>
    std::shared_ptr<T> resolve_named(const std::string& name);

    // 유틸리티
    void clear();
    bool has_service(const std::string& type_name) const;
    size_t service_count() const;
};
```

**사용 예제:**
```cpp
di_container container;

// 서비스 등록
container.register_singleton<database_service>([]() {
    return std::make_shared<sqlite_database>();
});

container.register_transient<user_service>([&]() {
    auto db = container.resolve<database_service>();
    return std::make_shared<user_service>(db);
});

// 서비스 해석
auto user_svc = container.resolve<user_service>();
```

---

## 모니터링 인터페이스

### 모니터링 인터페이스
**헤더:** `sources/monitoring/interfaces/monitoring_interface.h`

#### `metrics_collector`
모든 메트릭 수집기를 위한 기본 인터페이스.

```cpp
class metrics_collector {
public:
    virtual std::string get_name() const = 0;
    virtual bool is_enabled() const = 0;
    virtual result_void set_enabled(bool enable) = 0;
    virtual result_void initialize() = 0;
    virtual result_void cleanup() = 0;
    virtual result<metrics_snapshot> collect() = 0;
};
```

#### `monitorable`
모니터링될 수 있는 객체를 위한 인터페이스.

```cpp
class monitorable {
public:
    virtual std::string get_name() const = 0;
    virtual result<metrics_snapshot> get_metrics() const = 0;
    virtual result_void reset_metrics() = 0;
    virtual result<std::string> get_status() const = 0;
};
```

---

## 성능 모니터링

### Performance Monitor
**헤더:** `sources/monitoring/performance/performance_monitor.h`

#### `performance_monitor`
시스템 및 애플리케이션 성능 메트릭을 모니터링합니다.

```cpp
class performance_monitor : public metrics_collector {
public:
    explicit performance_monitor(const std::string& name = "performance_monitor");

    // 범위가 지정된 타이머 생성
    scoped_timer time_operation(const std::string& operation_name);

    // 프로파일러 가져오기
    performance_profiler& get_profiler();

    // 시스템 모니터 가져오기
    system_monitor& get_system_monitor();

    // 임계값 설정
    void set_cpu_threshold(double threshold);
    void set_memory_threshold(double threshold);
    void set_latency_threshold(std::chrono::milliseconds threshold);
};
```

#### `scoped_timer`
작업 기간 측정을 위한 RAII 타이머.

```cpp
class scoped_timer {
public:
    scoped_timer(performance_profiler* profiler, const std::string& operation_name);
    ~scoped_timer();

    void mark_failed();
    void complete();
    std::chrono::nanoseconds elapsed() const;
};
```

### Adaptive Optimizer
**헤더:** `sources/monitoring/performance/adaptive_optimizer.h`

#### `adaptive_optimizer`
시스템 부하에 따라 모니터링 매개변수를 동적으로 최적화합니다.

```cpp
class adaptive_optimizer {
public:
    struct optimization_config {
        double cpu_threshold = 80.0;
        double memory_threshold = 90.0;
        double target_overhead = 5.0;
        std::chrono::seconds adaptation_interval{60};
    };

    explicit adaptive_optimizer(const optimization_config& config = {});

    result<bool> start();
    result<bool> stop();
    result<optimization_decision> analyze_and_optimize();
    result<bool> apply_optimization(const optimization_decision& decision);
};
```

---

## 분산 추적

### Distributed Tracer
**헤더:** `sources/monitoring/tracing/distributed_tracer.h`

#### `distributed_tracer`
서비스 간 분산 추적을 관리합니다.

```cpp
class distributed_tracer {
public:
    // 스팬 시작
    result<std::shared_ptr<trace_span>> start_span(
        const std::string& operation_name,
        const std::string& service_name = "monitoring_system");

    result<std::shared_ptr<trace_span>> start_child_span(
        const trace_span& parent,
        const std::string& operation_name);

    // 컨텍스트 전파
    trace_context extract_context(const trace_span& span) const;

    template<typename Carrier>
    void inject_context(const trace_context& context, Carrier& carrier);

    template<typename Carrier>
    result<trace_context> extract_context_from_carrier(const Carrier& carrier);

    // 스팬 완료
    result<bool> finish_span(std::shared_ptr<trace_span> span);
};
```

#### `trace_span`
분산 추적에서 작업 단위를 나타냅니다.

```cpp
struct trace_span {
    std::string trace_id;
    std::string span_id;
    std::string parent_span_id;
    std::string operation_name;
    std::string service_name;

    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;

    std::unordered_map<std::string, std::string> tags;
    std::unordered_map<std::string, std::string> baggage;

    enum class status_code { unset, ok, error };
    status_code status;
    std::string status_message;
};
```

---

## 사용 예제

### 기본 모니터링 설정

```cpp
// 모니터링 인스턴스 생성
monitoring_builder builder;
auto monitoring = builder
    .with_history_size(1000)
    .with_collection_interval(1s)
    .add_collector(std::make_unique<performance_monitor>())
    .with_storage(std::make_unique<sqlite_storage_backend>(config))
    .enable_compression(true)
    .build();

// 모니터링 시작
monitoring.value()->start();
```

### 헬스 체크 설정

```cpp
// 헬스 모니터 생성
health_monitor monitor;

// 헬스 체크 등록
monitor.register_check("database",
    health_check_builder()
        .with_name("database_check")
        .with_type(health_check_type::readiness)
        .with_check([]() { return check_database(); })
        .build()
);

// 의존성 추가
monitor.add_dependency("api", "database");

// 모니터링 시작
monitor.start();
```

### 분산 추적

```cpp
// 추적 시작
distributed_tracer& tracer = global_tracer();
auto span = tracer.start_span("process_request");

// 태그 추가
span.value()->tags["user_id"] = "12345";
span.value()->tags["endpoint"] = "/api/users";

// 자식 스팬 생성
auto child_span = tracer.start_child_span(*span.value(), "database_query");

// 스팬 완료
tracer.finish_span(child_span.value());
tracer.finish_span(span.value());
```

---

## 현재 구현 상태

### ✅ 완전히 구현 및 테스트됨
- **Result 타입**: 완전한 오류 처리 프레임워크
- **DI 컨테이너**: 완전한 의존성 주입 기능
- **스레드 컨텍스트**: 스레드 안전 컨텍스트 관리
- **핵심 인터페이스**: 기본 모니터링 추상화

### ⚠️ Stub 구현 (향후 개발을 위한 기반)
- **성능 모니터링**: 인터페이스 완료, 기본 구현
- **분산 추적**: 인터페이스 완료, stub 기능
- **스토리지 백엔드**: 인터페이스 완료, 파일 스토리지 stub
- **헬스 모니터링**: 인터페이스 완료, 기본 헬스 체크
- **Circuit Breaker**: 인터페이스 완료, 기본 상태 관리

---

## Phase 4 마이그레이션 노트

Phase 4는 기능 완전성보다는 **핵심 기반 안정성**에 초점을 맞춥니다. 이 접근 방식은 다음을 제공합니다:

1. **견고한 기반**: 모든 핵심 타입과 패턴이 완전히 구현되고 테스트됨
2. **확장 가능한 아키텍처**: Stub 구현은 향후 확장을 위한 명확한 인터페이스 제공
3. **고품질 코어**: 오류 처리, DI 및 컨텍스트 관리가 고품질
4. **점진적 개발**: 기존 코드를 손상시키지 않고 기능을 점진적으로 추가 가능

---

## 추가 자료

- [Phase 4 문서](PHASE4.md) - 현재 구현 세부 사항
- [아키텍처 가이드](ARCHITECTURE_GUIDE.md) - 시스템 설계 및 패턴
- [예제](../examples/) - 작동하는 코드 예제
- [변경 로그](CHANGELOG.md) - 버전 기록 및 변경 사항

---

*Last Updated: 2025-10-20*
