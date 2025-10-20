# Monitoring System 아키텍처 가이드

> **Language:** [English](ARCHITECTURE_GUIDE.md) | **한국어**

**Phase 4 - 현재 구현 아키텍처**

## 개요

Monitoring System은 고성능 애플리케이션 모니터링을 위해 설계된 모듈식이고 확장 가능한 프레임워크입니다. **Phase 4**는 핵심 컴포넌트가 완전히 구현되고 테스트된 **탄탄한 기초**를 제공하며, 향후 개발을 위한 확장 가능한 인터페이스를 유지합니다. 최신 C++20으로 구축되었으며 안정성, 테스트 가능성 및 점진적 개발을 강조합니다.

## 목차

1. [아키텍처 원칙](#아키텍처-원칙)
2. [Phase 4 구현 상태](#phase-4-구현-상태)
3. [핵심 기초 아키텍처](#핵심-기초-아키텍처)
4. [시스템 컴포넌트](#시스템-컴포넌트)
5. [설계 패턴](#설계-패턴)
6. [테스트 아키텍처](#테스트-아키텍처)
7. [빌드 및 통합](#빌드-및-통합)
8. [미래 아키텍처](#미래-아키텍처)

---

## 아키텍처 원칙

### 1. 기초 우선 (Phase 4 초점)
- **핵심 안정성**: 기능을 구축하기 전에 기본 컴포넌트를 완전히 구현하고 테스트
- **점진적 개발**: 탄탄하고 테스트된 기초 위에 복잡한 기능 구축
- **양보다 질**: 부분적으로 작동하는 기능보다 100% 성공률의 37개 통과 테스트

### 2. 클린 아키텍처
- **관심사 분리**: 각 컴포넌트는 단일하고 명확한 책임을 가짐
- **인터페이스 분리**: 컴포넌트 간 깨끗한 인터페이스
- **의존성 역전**: 구체적인 구현이 아닌 추상화에 의존
- **Result 패턴**: 예외 없는 포괄적인 에러 처리

### 3. 최신 C++ 모범 사례
- **RAII**: 스마트 포인터와 스코프 객체를 통한 리소스 관리
- **템플릿 메타프로그래밍**: 타입 안전한 의존성 주입 및 에러 처리
- **Move Semantics**: 효율적인 리소스 전송
- **Concepts** (향후): 더 나은 컴파일 타임 검사를 위한 타입 제약

### 4. 테스트 가능성 및 신뢰성
- **테스트 주도 접근**: 모든 핵심 기능은 철저히 테스트됨
- **Stub 구현**: 향후 구현을 위한 기능적 인터페이스 준비
- **크로스 플랫폼 호환성**: Windows, Linux, macOS 지원 검증됨
- **에러 처리**: 전반적인 포괄적 Result<T> 패턴

---

## Phase 4 구현 상태

### ✅ 완전히 구현 및 프로덕션 준비
| 컴포넌트 | 상태 | 테스트 커버리지 | 설명 |
|-----------|--------|---------------|-------------|
| **Result Types** | ✅ 완료 | 13 tests | Monadic 에러 처리, 포괄적 Result<T> 구현 |
| **DI Container** | ✅ 완료 | 9 tests | singleton/transient 라이프사이클을 가진 완전한 의존성 주입 |
| **Thread Context** | ✅ 완료 | 6 tests | correlation ID를 가진 스레드 로컬 컨텍스트 관리 |
| **Core Interfaces** | ✅ 완료 | 9 tests | 기본 모니터링 추상화 및 계약 |

### ⚠️ Stub 구현 (인터페이스 완료)
| 컴포넌트 | 인터페이스 상태 | 구현 | 향후 개발 |
|-----------|------------------|----------------|-------------------|
| **Performance Monitor** | ✅ 완료 | 기본 stub | 고급 메트릭 수집 |
| **Distributed Tracing** | ✅ 완료 | 기본 stub | 전체 W3C Trace Context |
| **Storage Backends** | ✅ 완료 | File backend | 데이터베이스, 클라우드 스토리지 |
| **Health Monitoring** | ✅ 완료 | 기본 검사 | 고급 의존성 모니터링 |
| **Circuit Breakers** | ✅ 완료 | 상태 관리 | 고급 실패 감지 |

### 🔄 향후 구현
- 실시간 알림 시스템
- WebSocket 스트리밍을 가진 웹 대시보드
- 고급 스토리지 엔진
- OpenTelemetry 통합
- 스트림 처리 기능

---

## 핵심 기초 아키텍처

### 현재 Phase 4 아키텍처

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
│  ┌──────────────┐ ┌──────────────┐ ┌─────────────────┐     │
│  │   Examples   │ │  User Code   │ │   Test Suite    │     │
│  └──────────────┘ └──────────────┘ └─────────────────┘     │
└─────────────────────────────────────────────────────────────┘
                              │
                         API Layer
                              │
┌─────────────────────────────────────────────────────────────┐
│                     Core Components                         │
│  ┌──────────────┐ ┌──────────────┐ ┌─────────────────┐     │
│  │ Result<T>    │ │ DI Container │ │ Thread Context  │     │
│  │ Error Handling│ │ Service Mgmt │ │ Correlation IDs │     │
│  └──────────────┘ └──────────────┘ └─────────────────┘     │
└─────────────────────────────────────────────────────────────┘
                              │
                         Interface Layer
                              │
┌─────────────────────────────────────────────────────────────┐
│                   Monitoring Interfaces                     │
│  ┌──────────────┐ ┌──────────────┐ ┌─────────────────┐     │
│  │  Monitorable │ │   Collector  │ │  Storage API    │     │
│  │  Interface   │ │  Interface   │ │                 │     │
│  └──────────────┘ └──────────────┘ └─────────────────┘     │
└─────────────────────────────────────────────────────────────┘
                              │
                         Implementation Layer
                              │
┌─────────────────────────────────────────────────────────────┐
│                 Stub Implementations                        │
│  ┌──────────────┐ ┌──────────────┐ ┌─────────────────┐     │
│  │ Performance  │ │ Distributed  │ │ File Storage    │     │
│  │ Monitor Stub │ │ Tracing Stub │ │ Backend         │     │
│  └──────────────┘ └──────────────┘ └─────────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

### 컴포넌트 상호작용 흐름

```
Application Code
       │
       ▼
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│             │    │             │    │             │
│   Result    │◄──►│ DI Container│◄──►│Thread Context│
│   Types     │    │             │    │             │
│             │    │             │    │             │
└─────────────┘    └─────────────┘    └─────────────┘
       │                   │                   │
       ▼                   ▼                   ▼
┌─────────────────────────────────────────────────────┐
│            Monitoring Interfaces                    │
│                                                     │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐ │
│  │ Monitorable │  │ Collector   │  │ Storage     │ │
│  │ Interface   │  │ Interface   │  │ Interface   │ │
│  └─────────────┘  └─────────────┘  └─────────────┘ │
└─────────────────────────────────────────────────────┘
       │                   │                   │
       ▼                   ▼                   ▼
┌─────────────────────────────────────────────────────┐
│              Stub Implementations                   │
│  (향후 기능 개발 준비)                                │
└─────────────────────────────────────────────────────┘
```

### 디렉토리 구조 (Phase 4)

```
monitoring_system/
├── 📁 include/kcenon/monitoring/          # Public headers
│   ├── 📁 core/                          # ✅ 핵심 타입 (Result, errors)
│   ├── 📁 di/                            # ✅ 의존성 주입
│   ├── 📁 context/                       # ✅ 스레드 컨텍스트
│   ├── 📁 interfaces/                    # ✅ 추상 인터페이스
│   ├── 📁 collectors/                    # ⚠️ Collector stubs
│   ├── 📁 performance/                   # ⚠️ Performance monitor stub
│   ├── 📁 tracing/                       # ⚠️ Tracing stubs
│   └── 📁 storage/                       # ⚠️ Storage stubs
├── 📁 src/                               # 구현 파일
├── 📁 tests/                             # ✅ 포괄적 테스트 스위트
├── 📁 examples/                          # ✅ 작동하는 예제
├── 📁 docs/                              # ✅ 업데이트된 문서
└── CMakeLists.txt                        # ✅ 빌드 구성

범례:
✅ 완전히 구현되고 테스트됨
⚠️ 인터페이스 완료, stub 구현
```

---

## 시스템 컴포넌트

```
┌─────────────────────────────────────────────────────────────┐
│                     Application Layer                        │
├─────────────────────────────────────────────────────────────┤
│                    Monitoring System API                     │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │ Metrics  │  │  Health  │  │ Tracing  │  │ Logging  │   │
│  │Collector │  │ Monitor  │  │  System  │  │  System  │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
├─────────────────────────────────────────────────────────────┤
│                     Core Services Layer                      │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │  Stream  │  │ Storage  │  │Reliability│ │ Adaptive │   │
│  │Processing│  │ Backend  │  │ Features  │ │Optimizer │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
├─────────────────────────────────────────────────────────────┤
│                    Infrastructure Layer                      │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │  Thread  │  │    DI    │  │  Result  │  │  Error   │   │
│  │ Context  │  │Container │  │  Types   │  │ Handling │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
├─────────────────────────────────────────────────────────────┤
│                     Export/Integration Layer                 │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │   OTLP   │  │  Jaeger  │  │Prometheus│  │  Custom  │   │
│  │ Exporter │  │ Exporter │  │ Exporter │  │Exporters │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## 핵심 설계 패턴

### 1. Result Monad 패턴
예외 대신 명시적 에러 처리를 선호:

```cpp
template<typename T>
class result {
    std::variant<T, error_info> data_;
public:
    // Monadic bind 연산
    template<typename F>
    auto and_then(F&& f) -> result</*...*/>;
};
```

**이점:**
- 명시적 에러 전파
- 합성 가능한 에러 처리
- 숨겨진 제어 흐름 없음
- 더 나은 성능 (스택 언와인딩 없음)

### 2. Builder 패턴
구성에 광범위하게 사용:

```cpp
monitoring_builder builder;
auto system = builder
    .with_config(config)
    .add_collector(collector)
    .with_storage(storage)
    .build();
```

**이점:**
- Fluent API
- 선택적 매개변수
- 컴파일 타임 검증
- 불변 구성

### 3. Strategy 패턴
플러그 가능한 알고리즘용:

```cpp
class buffering_strategy {
    virtual result<bool> add(const metric_data& data) = 0;
    virtual bool should_flush() const = 0;
};
```

**구현:**
- `time_based_buffer`
- `size_based_buffer`
- `adaptive_buffer`

### 4. Observer 패턴
이벤트 알림용:

```cpp
class health_monitor {
    std::vector<health_listener*> listeners_;
    void notify_health_change(health_status status);
};
```

### 5. RAII 패턴
객체 수명을 통한 리소스 관리:

```cpp
class scoped_timer {
    ~scoped_timer() {
        // 자동으로 기간 기록
    }
};
```

---

## 컴포넌트 아키텍처

### 메트릭 수집 서브시스템

```
┌────────────────────────────────────┐
│      metrics_collector (base)      │
├────────────────────────────────────┤
│ + collect(): result<metrics>       │
│ + initialize(): result<void>       │
│ + cleanup(): result<void>          │
└────────────────────────────────────┘
            ▲          ▲
            │          │
┌───────────┴───┐ ┌────┴──────────┐
│ performance_  │ │ system_       │
│ monitor       │ │ monitor       │
├───────────────┤ ├───────────────┤
│ - profiler_   │ │ - cpu_usage   │
│ - thresholds_ │ │ - memory_info │
└───────────────┘ └───────────────┘
```

**주요 기능:**
- 플러그 가능한 collectors
- 비동기 수집
- 구성 가능한 간격
- 자동 집계

### 건강 모니터링 서브시스템

```
┌─────────────────────────────────────┐
│         health_monitor              │
├─────────────────────────────────────┤
│ - checks_: map<string, health_check>│
│ - dependencies_: dependency_graph   │
│ - recovery_handlers_: map<string,fn>│
├─────────────────────────────────────┤
│ + register_check()                  │
│ + add_dependency()                  │
│ + check_all()                       │
└─────────────────────────────────────┘
            uses
            ▼
┌─────────────────────────────────────┐
│      health_dependency_graph        │
├─────────────────────────────────────┤
│ + add_node()                        │
│ + would_create_cycle()              │
│ + topological_sort()                │
└─────────────────────────────────────┘
```

**의존성 해결:**
- 위상 정렬
- 사이클 감지
- 영향 분석
- 계단식 건강 검사

### 분산 추적 서브시스템

```
┌──────────────────────────┐
│   distributed_tracer     │
├──────────────────────────┤
│ - active_spans_          │
│ - span_storage_          │
│ - context_propagator_    │
├──────────────────────────┤
│ + start_span()           │
│ + inject_context()       │
│ + extract_context()      │
└──────────────────────────┘
        creates
           ▼
┌──────────────────────────┐
│      trace_span          │
├──────────────────────────┤
│ - trace_id               │
│ - span_id                │
│ - parent_span_id         │
│ - tags                   │
│ - baggage                │
└──────────────────────────┘
```

**컨텍스트 전파:**
- W3C Trace Context 형식
- Baggage 전파
- 서비스 간 상관관계

### 스토리지 백엔드 서브시스템

```
┌─────────────────────────────────────┐
│      storage_backend (interface)    │
├─────────────────────────────────────┤
│ + initialize(): result<bool>        │
│ + write(): result<bool>             │
│ + read(): result<string>            │
│ + flush(): result<bool>             │
└─────────────────────────────────────┘
            ▲
            │ implements
    ┌───────┴────────┬────────────┬─────────────┐
    │                │            │             │
┌───▼────┐    ┌─────▼──────┐ ┌──▼──────┐ ┌───▼────┐
│ File   │    │  Database  │ │  Cloud  │ │ Memory │
│Storage │    │  Storage   │ │ Storage │ │ Buffer │
└────────┘    └────────────┘ └─────────┘ └────────┘
```

**스토리지 전략:**
- 동기 vs 비동기
- 배칭 및 버퍼링
- 압축 지원
- 보존 정책

---

## 데이터 흐름

### 메트릭 수집 흐름

```
Application Code
      │
      ▼
[Metric Event] ──► [Collector] ──► [Aggregator]
                         │              │
                         ▼              ▼
                   [Raw Metrics]  [Aggregated]
                         │              │
                         └──────┬───────┘
                                │
                                ▼
                         [Stream Processor]
                                │
                    ┌───────────┼───────────┐
                    ▼           ▼           ▼
              [Storage]   [Exporter]  [Analyzer]
```

### 추적 수집 흐름

```
[Request Start]
      │
      ▼
[Create Root Span]
      │
      ├──► [Inject Context to Headers]
      │
      ▼
[Child Operations]
      │
      ├──► [Create Child Spans]
      │
      ▼
[Complete Spans]
      │
      ▼
[Export to Backend]
```

### 건강 검사 흐름

```
[Health Check Trigger]
      │
      ▼
[Dependency Graph Traversal]
      │
      ▼
[Execute Checks in Order]
      │
      ├──► [Success] ──► [Update Status]
      │
      └──► [Failure] ──► [Recovery Handler]
                              │
                              ▼
                        [Circuit Breaker]
```

---

## 배포 아키텍처

### 단일 프로세스 배포

```
┌─────────────────────────────────┐
│      Application Process        │
├─────────────────────────────────┤
│  ┌─────────────────────────┐   │
│  │   Application Code      │   │
│  └─────────────────────────┘   │
│  ┌─────────────────────────┐   │
│  │   Monitoring System     │   │
│  │  ┌──────┐ ┌──────────┐ │   │
│  │  │Metrics│ │ Storage  │ │   │
│  │  └──────┘ └──────────┘ │   │
│  └─────────────────────────┘   │
└─────────────────────────────────┘
```

### 분산 배포

```
┌──────────────┐     ┌──────────────┐
│   Service A  │────►│   Service B  │
│ [Monitoring] │     │ [Monitoring] │
└──────────────┘     └──────────────┘
       │                    │
       └────────┬───────────┘
                ▼
        ┌──────────────┐
        │  Collector   │
        │   Service    │
        └──────────────┘
                │
                ▼
        ┌──────────────┐
        │   Storage    │
        │   Backend    │
        └──────────────┘
```

### 고가용성 설정

```
        Load Balancer
             │
    ┌────────┼────────┐
    ▼        ▼        ▼
[App-1]  [App-2]  [App-3]
    │        │        │
    └────────┼────────┘
             ▼
    [Monitoring Aggregator]
             │
    ┌────────┼────────┐
    ▼        ▼        ▼
[Store-1] [Store-2] [Store-3]
    (복제된 스토리지)
```

---

## 통합 포인트

### OpenTelemetry 통합

```cpp
// OTLP로 자동 변환
opentelemetry_adapter adapter;
adapter.export_traces(spans);
adapter.export_metrics(metrics);
```

### Prometheus 통합

```cpp
// 메트릭 엔드포인트 노출
prometheus_exporter exporter;
exporter.serve_metrics("/metrics", 9090);
```

### 맞춤 통합

```cpp
// 맞춤 exporter 구현
class custom_exporter : public metrics_exporter {
    result<bool> export_batch(const std::vector<metric_data>& metrics) override {
        // 맞춤 export 로직
    }
};
```

---

## 성능 아키텍처

### 메모리 관리

1. **Object Pooling**: 자주 할당되는 객체 재사용
2. **Arena Allocation**: 관련 객체를 위한 대량 메모리 할당
3. **Copy-on-Write**: 읽기 중심 워크로드를 위한 복사 최소화
4. **Smart Pointers**: shared_ptr/unique_ptr를 사용한 자동 메모리 관리

### 동시성 모델

1. **Thread Pools**: 구성 가능한 워커 스레드
2. **Lock-Free Queues**: 고처리량 메트릭 수집용
3. **Read-Write Locks**: 읽기 중심 시나리오에 최적화
4. **Async I/O**: 논블로킹 스토리지 연산

### 최적화 전략

1. **Sampling**: 구성 가능한 샘플링 비율로 오버헤드 감소
2. **Batching**: 효율성을 위한 연산 집계
3. **Caching**: 자주 접근하는 데이터 캐싱
4. **Lazy Evaluation**: 비용이 많이 드는 계산 연기

### 성능 메트릭

목표 성능 특성:
- **메트릭 수집**: < 1μs/메트릭
- **Span 생성**: < 500ns/span
- **건강 검사**: < 10ms/검사
- **스토리지 쓰기**: < 5ms 배치 쓰기
- **CPU 오버헤드**: < 5% 전체
- **메모리 오버헤드**: < 50MB 기준선

---

## 보안 아키텍처

### 데이터 보호

1. **저장 시 암호화**: 저장된 메트릭을 위한 선택적 암호화
2. **전송 중 암호화**: 네트워크 통신을 위한 TLS
3. **데이터 정제**: 민감한 정보 제거
4. **접근 제어**: 메트릭에 대한 역할 기반 접근

### 보안 기능

```cpp
// 민감한 데이터 마스킹
span->tags["password"] = mask_sensitive_data(password);

// 안전한 스토리지
storage_config config;
config.encryption = encryption_type::aes_256;
config.key_provider = std::make_unique<kms_provider>();
```

### 위협 모델

다음으로부터 보호:
- **정보 공개**: 마스킹된 민감 데이터
- **서비스 거부**: 속도 제한 및 circuit breakers
- **리소스 고갈**: 제한된 큐 및 타임아웃
- **주입 공격**: 입력 검증 및 정제

---

## 확장성 고려사항

### 수평 확장

1. **무상태 설계**: 인스턴스 간 공유 상태 없음
2. **파티셔닝**: 키로 메트릭 샤딩
3. **로드 밸런싱**: collectors 간 부하 분산
4. **연합**: 여러 소스에서 메트릭 집계

### 수직 확장

1. **적응형 스레딩**: CPU 코어와 함께 스레드 풀 확장
2. **메모리 풀링**: 효율적인 메모리 사용
3. **배치 처리**: 메트릭을 배치로 처리
4. **압축**: 스토리지 및 네트워크 오버헤드 감소

### 확장 전략

```cpp
// 적응형 확장
adaptive_optimizer optimizer;
optimizer.set_target_overhead(5.0); // 최대 5% 오버헤드
optimizer.enable_auto_scaling(true);

// 파티션된 스토리지
partitioned_storage storage;
storage.add_partition("metrics_1", 0, 1000);
storage.add_partition("metrics_2", 1001, 2000);
```

### 용량 계획

인스턴스당 일반적인 용량:
- **메트릭**: 100K 메트릭/초
- **추적**: 10K spans/초
- **건강 검사**: 1K 검사/초
- **스토리지**: 압축된 1GB/시간
- **네트워크**: 평균 10Mbps

---

## 피해야 할 안티패턴

### 1. 동기 메트릭 수집
❌ **하지 마세요:**
```cpp
void process_request() {
    // 요청 처리를 블록킹
    metrics.record_sync("latency", duration);
}
```

✅ **하세요:**
```cpp
void process_request() {
    // 논블로킹
    metrics.record_async("latency", duration);
}
```

### 2. 무제한 큐
❌ **하지 마세요:**
```cpp
std::queue<metric_data> queue; // 무한정 증가 가능
```

✅ **하세요:**
```cpp
bounded_queue<metric_data> queue(10000); // 제한된 크기
```

### 3. 전역 상태
❌ **하지 마세요:**
```cpp
global_metrics g_metrics; // 전역 가변 상태
```

✅ **하세요:**
```cpp
thread_local metrics t_metrics; // 스레드 로컬 상태
```

---

## 아키텍처 문제 해결

### 성능 저하

1. **샘플링 비율 확인**: 오버헤드를 줄이기 위해 샘플링 증가
2. **배치 크기 검토**: 배칭 매개변수 최적화
3. **큐 깊이 모니터링**: 큐 백프레셔 확인
4. **락 경합 분석**: 프로파일러를 사용하여 핫스팟 찾기

### 메모리 누수

1. **메모리 추적 활성화**: 내장 메모리 프로파일러 사용
2. **순환 참조 확인**: shared_ptr 사용 검토
3. **객체 풀 모니터링**: 적절한 객체 반환 확인
4. **정리 검증**: 소멸자가 호출되는지 확인

### 통합 실패

1. **네트워크 연결 확인**: 엔드포인트에 도달 가능한지 확인
2. **인증 검토**: 자격 증명이 유효한지 확인
3. **데이터 형식 검증**: 직렬화/역직렬화 확인
4. **Circuit breakers 모니터링**: 열린 circuits 확인

---

## 미래 아키텍처 방향

### 계획된 개선

1. **Coroutine 지원**: 비동기 연산을 위한 C++20 coroutines
2. **GPU 가속**: 메트릭 집계를 위한 CUDA/OpenCL
3. **머신 러닝**: 이상 감지 및 예측
4. **Edge Computing**: 경량 edge 모니터링 에이전트
5. **WebAssembly**: 브라우저 기반 모니터링 대시보드

### 연구 분야

1. **양자 저항 암호화**: 미래 대비 보안
2. **AI 기반 최적화**: 자가 조정 모니터링
3. **블록체인 통합**: 불변 감사 추적
4. **5G 네트워크 지원**: 초저지연 모니터링

---

## 아키텍처 결정 기록 (ADRs)

### ADR-001: 예외 대신 Result 타입 사용
**상태**: 수락됨
**맥락**: 숨겨진 제어 흐름 없이 명시적 에러 처리 필요
**결정**: Monadic Result<T> 타입 사용
**결과**: 더 장황하지만 더 안전한 에러 처리

### ADR-002: 템플릿 기반 확장성
**상태**: 수락됨
**맥락**: 성능을 위한 컴파일 타임 다형성 필요
**결정**: 일반 컴포넌트에 템플릿 사용
**결과**: 더 긴 컴파일 시간이지만 더 나은 런타임 성능

### ADR-003: Lock-free 데이터 구조
**상태**: 수락됨
**맥락**: 높은 동시성 메트릭 수집
**결정**: 가능한 곳에 atomic 연산 사용
**결과**: 복잡한 구현이지만 더 나은 확장성

---

## 결론

Monitoring System 아키텍처는 다음을 위해 설계되었습니다:
- **고성능**: 모니터링되는 애플리케이션에 최소 오버헤드
- **신뢰성**: 우아한 저하를 가진 내결함성
- **확장 가능**: 수평 및 수직 확장 지원
- **확장 가능**: 새로운 기능 및 통합을 쉽게 추가
- **유지 관리 가능**: 명확한 경계를 가진 클린 아키텍처

구현 세부사항은 [API Reference](API_REFERENCE.md)를 참조하세요.
실용적인 예제는 [Examples Directory](../examples/)를 참조하세요.

---

*Last Updated: 2025-10-20*
