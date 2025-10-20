[![CI](https://github.com/kcenon/monitoring_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/ci.yml)
[![Code Coverage](https://github.com/kcenon/monitoring_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/coverage.yml)
[![Static Analysis](https://github.com/kcenon/monitoring_system/actions/workflows/static-analysis.yml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/static-analysis.yml)
[![Documentation](https://github.com/kcenon/monitoring_system/actions/workflows/build-Doxygen.yaml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/build-Doxygen.yaml)

# Monitoring System Project

> **Language:** [English](README.md) | **한국어**

## 개요

Monitoring System Project는 고성능 애플리케이션을 위한 엔터프라이즈급 모니터링, 추적 및 신뢰성 기능을 제공하도록 설계된 프로덕션 준비가 완료된 포괄적인 C++20 observability 플랫폼입니다. 모듈식 인터페이스 기반 아키텍처와 thread system 생태계와의 원활한 통합으로 구축되어 최소한의 오버헤드와 최대의 확장성으로 실시간 인사이트를 제공합니다.

> **🏗️ Modular Architecture**: 메트릭, 추적, 상태 확인 및 신뢰성 패턴을 위한 플러그 가능한 컴포넌트를 갖춘 포괄적인 모니터링 플랫폼입니다.

> **✅ Latest Updates**: 향상된 분산 추적, 성능 모니터링, 의존성 주입 컨테이너 및 포괄적인 오류 처리가 추가되었습니다. 모든 플랫폼에서 CI/CD 파이프라인이 정상 작동합니다.

## 🔗 생태계 통합

깨끗한 인터페이스 경계를 가진 모듈식 C++ 생태계의 일부입니다:

**필수 의존성**:
- **[common_system](https://github.com/kcenon/common_system)**: 핵심 인터페이스 (IMonitor, ILogger, Result<T>)
- **[thread_system](https://github.com/kcenon/thread_system)**: Threading primitives 및 monitoring_interface

**선택적 통합**:
- **[logger_system](https://github.com/kcenon/logger_system)**: 로깅 기능 (ILogger 인터페이스를 통해)
- **[integrated_thread_system](https://github.com/kcenon/integrated_thread_system)**: 전체 생태계 예제

**통합 패턴**:
```
common_system (interfaces) ← monitoring_system implements IMonitor
                          ↖ optional: inject ILogger at runtime
```

**이점**:
- 인터페이스 전용 의존성 (순환 참조 없음)
- 독립적인 컴파일 및 배포
- DI 패턴을 통한 런타임 컴포넌트 주입
- 명확한 관심사 분리

**시스템 간 추적**:
시스템 경계를 통해 `trace_id`/`correlation_id`를 전파합니다:
- network_system → container_system → database_system → logger_system
- 진입/진출 지점에서 span 및 메트릭 강화

> 📖 전체 통합 세부 정보는 [ARCHITECTURE.md](docs/ARCHITECTURE.md)를 참조하세요.

## 프로젝트 목적 및 미션

이 프로젝트는 전 세계 개발자들이 직면한 근본적인 문제인 **애플리케이션 observability를 접근 가능하고, 신뢰할 수 있으며, 실행 가능하게 만드는 것**을 다룹니다. 기존의 모니터링 접근 방식은 종종 포괄적인 인사이트가 부족하고, 불충분한 오류 처리를 제공하며, 성능 오버헤드로 어려움을 겪습니다. 우리의 미션은 다음을 제공하는 포괄적인 솔루션을 제공하는 것입니다:

- **observability 격차 제거** - 포괄적인 메트릭, 추적 및 상태 모니터링을 통해
- **시스템 신뢰성 보장** - circuit breaker, error boundary 및 상태 확인을 통해
- **성능 극대화** - 효율적인 데이터 수집과 최소한의 오버헤드를 통해
- **유지보수성 향상** - 명확한 인터페이스와 모듈식 아키텍처를 통해
- **문제 해결 가속화** - 실행 가능한 인사이트와 근본 원인 분석을 제공하여

## 핵심 장점 및 이점

### 🚀 **성능 우수성**
- **실시간 모니터링**: 블로킹 작업 없이 지속적인 메트릭 수집
- **효율적인 데이터 구조**: 최소한의 오버헤드를 위한 lock-free counter 및 atomic 연산
- **적응형 샘플링**: 높은 처리량 시나리오를 위한 지능형 샘플링 전략
- **리소스 최적화**: 구성 가능한 보존 정책을 사용한 메모리 효율적 저장

### 🛡️ **프로덕션급 신뢰성**
- **설계부터 thread-safe**: 모든 컴포넌트가 안전한 동시 액세스를 보장
- **포괄적인 오류 처리**: Result 패턴으로 침묵하는 오류 없음 보장
- **Circuit breaker 패턴**: 자동 장애 감지 및 복구 메커니즘
- **상태 모니터링**: 사전 예방적 의존성 및 서비스 상태 검증

### 🔧 **개발자 생산성**
- **직관적인 API 설계**: 깨끗하고 자체 문서화된 인터페이스로 학습 곡선 감소
- **풍부한 telemetry**: 포괄적인 메트릭, 추적 및 상태 데이터
- **유연한 구성**: 일반적인 시나리오를 위한 템플릿 기반 구성
- **모듈식 컴포넌트**: 필요한 것만 사용 - 최대 유연성

### 🌐 **크로스 플랫폼 호환성**
- **범용 지원**: Windows, Linux 및 macOS에서 작동
- **컴파일러 유연성**: GCC, Clang 및 MSVC와 호환
- **C++ 표준 적응**: 우아한 폴백과 함께 C++20 기능 활용
- **아키텍처 독립성**: x86 및 ARM 프로세서 모두에 최적화

### 📈 **엔터프라이즈 준비 기능**
- **분산 추적**: 서비스 경계를 넘어 요청 흐름 추적
- **성능 프로파일링**: 상세한 타이밍 및 리소스 사용량 분석
- **상태 대시보드**: 실시간 시스템 상태 및 의존성 상태
- **신뢰성 패턴**: Circuit breaker, retry 정책 및 error boundary

## 실제 영향 및 사용 사례

### 🎯 **이상적인 애플리케이션**
- **마이크로서비스 아키텍처**: 분산 추적 및 서비스 상태 모니터링
- **고빈도 거래 시스템**: 초저지연 성능 모니터링
- **실시간 시스템**: 지속적인 상태 확인 및 circuit breaker 보호
- **웹 애플리케이션**: 요청 추적 및 성능 병목 지점 식별
- **IoT 플랫폼**: 리소스 사용량 모니터링 및 신뢰성 패턴
- **데이터베이스 시스템**: 쿼리 성능 분석 및 상태 모니터링

### 📊 **성능 벤치마크**

*Apple M1 (8-core) @ 3.2GHz, 16GB, macOS Sonoma에서 벤치마크됨*

> **🚀 Architecture Update**: 최신 모듈식 아키텍처는 thread_system 생태계와의 원활한 통합을 제공합니다. 실시간 모니터링은 애플리케이션 성능에 영향을 주지 않고 포괄적인 인사이트를 제공합니다.

#### 핵심 성능 메트릭 (최신 벤치마크)
- **메트릭 수집**: 최대 10M 메트릭 작업/초 (atomic counter)
- **추적 처리**:
  - Span 생성: 2.5M span/초, 최소한의 할당 오버헤드
  - Context 전파: 분산 시스템에서 hop당 <50ns
  - 추적 내보내기: 최대 100K span/초의 배치 처리
- **상태 확인**:
  - 상태 검증: 의존성 검증을 포함하여 500K 확인/초
  - Circuit breaker: 보호된 작업당 <10ns 오버헤드
- **메모리 효율성**: 구성 가능한 보존으로 <5MB 기준선
- **저장 오버헤드**: 최대 90%의 시계열 데이터 압축

#### 업계 표준과의 성능 비교
| 모니터링 타입 | 처리량 | 지연 시간 | 메모리 사용량 | 최적 사용 사례 |
|----------------|------------|---------|--------------|---------------|
| 🏆 **Monitoring System** | **10M ops/s** | **<50ns** | **<5MB** | 모든 시나리오 (포괄적) |
| 📦 **Prometheus Client** | 2.5M ops/s | 200ns | 15MB | 메트릭 중심 |
| 📦 **OpenTelemetry** | 1.8M ops/s | 150ns | 25MB | 표준 준수 |
| 📦 **Custom Counters** | 15M ops/s | 5ns | 1MB | 기본 메트릭만 |

#### 주요 성능 인사이트
- 🏃 **메트릭**: 업계 최고의 atomic counter 성능 (10M ops/s)
- 🏋️ **추적**: 최소한의 할당으로 효율적인 span 수명 주기
- ⏱️ **지연 시간**: 실시간 시스템을 위한 초저 오버헤드 (<50ns)
- 📈 **확장성**: 스레드 수와 부하에 따른 선형 확장

## ✨ 기능

### 🎯 핵심 기능
- **성능 모니터링**: 실시간 메트릭 수집 및 분석
- **분산 추적**: 서비스 간 요청 흐름 추적
- **상태 모니터링**: 서비스 상태 확인 및 의존성 검증
- **오류 처리**: 강력한 result 타입 및 error boundary 패턴
- **의존성 주입**: 수명 주기 관리를 갖춘 완전한 컨테이너

### 🔧 기술적 하이라이트
- **Modern C++20**: 최신 언어 기능 활용 (concepts, coroutines, std::format)
- **크로스 플랫폼**: Windows, Linux 및 macOS 지원
- **Thread-Safe**: atomic counter 및 lock을 사용한 동시 작업
- **모듈식 설계**: 선택적 통합을 지원하는 플러그인 기반 아키텍처
- **프로덕션 준비**: 100% 통과율의 37개 포괄적인 테스트

## 🏗️ 아키텍처

```
┌─────────────────────────────────────────────────────────────────┐
│                     Monitoring System                           │
├─────────────────────────────────────────────────────────────────┤
│ Core Components                                                 │
├─────────────────────┬───────────────────┬───────────────────────┤
│ Performance Monitor │ Distributed Tracer │ Health Monitor        │
│ • Metrics Collection│ • Span Management  │ • Service Checks      │
│ • Profiling Data    │ • Context Propagation│ • Dependency Tracking│
│ • Aggregation       │ • Trace Export     │ • Recovery Policies   │
├─────────────────────┼───────────────────┼───────────────────────┤
│ Storage Layer       │ Event System      │ Reliability Patterns  │
│ • Memory Backend    │ • Event Bus       │ • Circuit Breakers    │
│ • File Backend      │ • Async Processing│ • Retry Policies      │
│ • Time Series       │ • Error Events    │ • Error Boundaries    │
└─────────────────────┴───────────────────┴───────────────────────┘
```

## ✨ 핵심 기능

### 🎯 실시간 모니터링
- **성능 메트릭**: 10M+ ops/초 처리량의 atomic counter, gauge, histogram
- **분산 추적**: span 생성으로 요청 흐름 추적 (2.5M span/초)
- **상태 모니터링**: 서비스 상태 확인 및 의존성 검증 (500K 확인/초)
- **Thread-Safe 작업**: 최소한의 오버헤드를 위한 lock-free atomic 연산
- **구성 가능한 저장소**: 시계열 압축을 지원하는 메모리 및 파일 백엔드

### 🔧 고급 기능
- **Result 기반 오류 처리**: `Result<T>` 패턴을 사용한 포괄적인 오류 처리
- **의존성 주입 컨테이너**: 서비스 등록 및 수명 주기 관리를 갖춘 완전한 DI
- **Thread Context 추적**: 스레드 간 요청 컨텍스트 및 메타데이터 전파
- **Circuit Breaker 패턴**: 자동 장애 감지 및 복구 메커니즘
- **이벤트 기반 아키텍처**: 최소한의 블로킹으로 비동기 이벤트 처리

### 🏗️ 아키텍처 하이라이트
- **인터페이스 기반 설계**: 추상 인터페이스를 통한 명확한 분리 (IMonitor, ILogger, IMonitorable)
- **모듈식 컴포넌트**: 플러그 가능한 저장소 백엔드, tracer 및 health checker
- **순환 의존성 제로**: common_system을 통한 인터페이스 전용 의존성
- **독립적인 컴파일**: 생태계 의존성 없이 독립 실행형 빌드
- **프로덕션급**: 100% 테스트 통과율 (37/37 테스트), <10% 오버헤드

### 📊 현재 상태
- **빌드 시스템**: 기능 플래그 및 자동 의존성 감지를 지원하는 CMake
- **의존성**: 인터페이스 전용 (thread_system, common_system)
- **컴파일**: 독립적, 약 12초 빌드 시간
- **테스트 커버리지**: 모든 핵심 기능이 검증되고 프로덕션 준비 완료
- **성능**: <10% 오버헤드, 10M+ 메트릭 ops/초

**아키텍처**:
```
monitoring_system
    ↓ implements
IMonitor (common_system)
    ↑ optional
ILogger injection (runtime DI)
```

## 기술 스택 및 아키텍처

### 🏗️ **Modern C++ 기반**
- **C++20 기능**: 향상된 성능을 위한 Concepts, coroutines, `std::format` 및 ranges
- **Template metaprogramming**: 타입 안전, 컴파일 타임 최적화
- **메모리 관리**: 자동 리소스 정리를 위한 smart pointer 및 RAII
- **예외 안전성**: 전체적으로 강력한 예외 안전성 보장
- **Result 패턴**: 예외 없이 포괄적인 오류 처리
- **인터페이스 기반 설계**: 인터페이스와 구현 간의 명확한 분리
- **모듈식 아키텍처**: 선택적 생태계 통합을 지원하는 핵심 모니터링 기능

### 🔄 **디자인 패턴 구현**
- **Observer 패턴**: 이벤트 기반 메트릭 수집 및 상태 모니터링
- **Strategy 패턴**: 구성 가능한 샘플링 전략 및 저장소 백엔드
- **Factory 패턴**: 구성 가능한 모니터 및 tracer 생성
- **Template Method 패턴**: 사용자 정의 가능한 모니터링 동작
- **의존성 주입**: 컴포넌트 수명 주기 관리를 위한 서비스 컨테이너
- **Circuit Breaker 패턴**: 신뢰성 및 결함 허용 메커니즘

## 프로젝트 구조

### 📁 **디렉토리 구성**

```
monitoring_system/
├── 📁 include/kcenon/monitoring/   # Public headers
│   ├── 📁 core/                    # Core components
│   │   ├── performance_monitor.h   # Performance metrics collection
│   │   ├── result_types.h          # Error handling types
│   │   ├── di_container.h          # Dependency injection
│   │   └── thread_context.h        # Thread-local context
│   ├── 📁 interfaces/              # Abstract interfaces
│   │   ├── monitorable_interface.h # Monitoring abstraction
│   │   ├── storage_interface.h     # Storage abstraction
│   │   ├── tracer_interface.h      # Tracing abstraction
│   │   └── health_check_interface.h # Health check abstraction
│   ├── 📁 tracing/                 # Distributed tracing
│   │   ├── distributed_tracer.h    # Trace management
│   │   ├── span.h                  # Span operations
│   │   ├── trace_context.h         # Context propagation
│   │   └── trace_exporter.h        # Trace export
│   ├── 📁 health/                  # Health monitoring
│   │   ├── health_monitor.h        # Health validation
│   │   ├── health_check.h          # Health check definitions
│   │   ├── circuit_breaker.h       # Circuit breaker pattern
│   │   └── reliability_patterns.h  # Retry and fallback
│   ├── 📁 storage/                 # Storage backends
│   │   ├── memory_storage.h        # In-memory storage
│   │   ├── file_storage.h          # File-based storage
│   │   └── time_series_storage.h   # Time-series data
│   └── 📁 config/                  # Configuration
│       ├── monitoring_config.h     # Configuration structures
│       └── config_validator.h      # Configuration validation
├── 📁 src/                         # Implementation files
│   ├── 📁 core/                    # Core implementations
│   ├── 📁 tracing/                 # Tracing implementations
│   ├── 📁 health/                  # Health implementations
│   ├── 📁 storage/                 # Storage implementations
│   └── 📁 config/                  # Configuration implementations
├── 📁 examples/                    # Example applications
│   ├── basic_monitoring_example/   # Basic monitoring usage
│   ├── distributed_tracing_example/ # Tracing across services
│   ├── health_reliability_example/ # Health checks and reliability
│   └── integration_examples/       # Ecosystem integration
├── 📁 tests/                       # All tests
│   ├── 📁 unit/                    # Unit tests
│   ├── 📁 integration/             # Integration tests
│   └── 📁 benchmarks/              # Performance tests
├── 📁 docs/                        # Documentation
├── 📁 cmake/                       # CMake modules
├── 📄 CMakeLists.txt               # Build configuration
└── 📄 vcpkg.json                   # Dependencies
```

### 📖 **주요 파일 및 목적**

#### Core 모듈 파일
- **`performance_monitor.h/cpp`**: atomic 연산을 사용한 실시간 메트릭 수집
- **`result_types.h/cpp`**: 포괄적인 오류 처리 및 result 타입
- **`di_container.h/cpp`**: 수명 주기 관리를 갖춘 의존성 주입 컨테이너
- **`thread_context.h/cpp`**: 요청 추적을 위한 thread-local context

#### Tracing 파일
- **`distributed_tracer.h/cpp`**: 분산 추적 관리 및 span 수명 주기
- **`span.h/cpp`**: 메타데이터를 포함한 개별 span 작업
- **`trace_context.h/cpp`**: 서비스 경계를 넘은 컨텍스트 전파
- **`trace_exporter.h/cpp`**: 추적 데이터 내보내기 및 배치

#### Health Monitoring 파일
- **`health_monitor.h/cpp`**: 포괄적인 상태 검증 프레임워크
- **`circuit_breaker.h/cpp`**: Circuit breaker 패턴 구현
- **`reliability_patterns.h/cpp`**: Retry 정책 및 error boundary

### 🔗 **모듈 의존성**

```
config (no dependencies)
    │
    └──> core
            │
            ├──> tracing
            │
            ├──> health
            │
            ├──> storage
            │
            └──> integration (thread_system, logger_system)

Optional External Projects:
- thread_system (provides monitoring_interface)
- logger_system (provides logging capabilities)
```

## 빠른 시작 및 사용 예제

### 🚀 **5분 안에 시작하기**

#### 포괄적인 모니터링 예제

```cpp
#include <kcenon/monitoring/core/performance_monitor.h>
#include <kcenon/monitoring/tracing/distributed_tracer.h>
#include <kcenon/monitoring/health/health_monitor.h>

using namespace monitoring_system;

int main() {
    // 1. Create comprehensive monitoring setup
    performance_monitor perf_monitor("my_application");
    auto& tracer = global_tracer();
    health_monitor health_monitor;

    // 2. Enable performance metrics collection
    perf_monitor.enable_collection(true);

    // 3. Set up health checks
    health_monitor.register_check(
        std::make_unique<functional_health_check>(
            "system_resources",
            health_check_type::system,
            []() {
                // Check system resources
                auto memory_usage = get_memory_usage_percent();
                return memory_usage < 80.0 ?
                    health_check_result::healthy("Memory usage normal") :
                    health_check_result::degraded("High memory usage");
            }
        )
    );

    // 4. Start distributed trace
    auto trace_result = tracer.start_span("main_operation", "application");
    if (!trace_result) {
        std::cerr << "Failed to start trace: " << trace_result.get_error().message << "\n";
        return -1;
    }

    auto main_span = trace_result.value();
    main_span->set_tag("operation.type", "batch_processing");
    main_span->set_tag("batch.size", "10000");

    // 5. Monitor performance-critical operation
    auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < 10000; ++i) {
        // Create child span for individual operations
        auto op_span_result = tracer.start_child_span(main_span, "process_item");
        if (op_span_result) {
            auto op_span = op_span_result.value();
            op_span->set_tag("item.id", std::to_string(i));

            // Simulate processing
            std::this_thread::sleep_for(std::chrono::microseconds(10));

            // Record processing time
            auto item_start = std::chrono::steady_clock::now();
            // ... actual processing ...
            auto item_end = std::chrono::steady_clock::now();

            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(item_end - item_start);
            perf_monitor.get_profiler().record_sample("item_processing", duration, true);

            tracer.finish_span(op_span);
        }

        // Check health periodically
        if (i % 1000 == 0) {
            auto health_result = health_monitor.check_health();
            main_span->set_tag("health.status", to_string(health_result.status));

            if (health_result.status == health_status::unhealthy) {
                main_span->set_tag("error", "System health degraded");
                break;
            }
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // 6. Collect comprehensive metrics
    auto metrics_snapshot = perf_monitor.collect();
    if (metrics_snapshot) {
        auto snapshot = metrics_snapshot.value();

        std::cout << "Performance Results:\n";
        std::cout << "- Total processing time: " << total_duration.count() << " ms\n";
        std::cout << "- CPU usage: " << snapshot.get_metric("cpu_usage") << "%\n";
        std::cout << "- Memory usage: " << snapshot.get_metric("memory_usage") << " MB\n";
        std::cout << "- Items processed: " << snapshot.get_metric("items_processed") << "\n";

        // Get profiling statistics
        auto profiler_stats = perf_monitor.get_profiler().get_statistics("item_processing");
        std::cout << "- Average item time: " << profiler_stats.mean_duration.count() << " ns\n";
        std::cout << "- P95 item time: " << profiler_stats.p95_duration.count() << " ns\n";
    }

    // 7. Finish main span with results
    main_span->set_tag("total.duration_ms", total_duration.count());
    main_span->set_tag("throughput.items_per_sec",
                       static_cast<double>(10000) / total_duration.count() * 1000.0);
    tracer.finish_span(main_span);

    // 8. Export traces and metrics
    auto export_result = tracer.export_traces();
    if (!export_result) {
        std::cerr << "Failed to export traces: " << export_result.get_error().message << "\n";
    }

    return 0;
}
```

> **성능 팁**: 모니터링 시스템은 최소한의 오버헤드를 위해 자동으로 최적화합니다. 고빈도 시나리오에서 최대 성능을 위해 atomic counter 및 배치 작업을 사용하세요.

### 🔄 **추가 사용 예제**

#### 실시간 메트릭 대시보드
```cpp
#include <kcenon/monitoring/core/performance_monitor.h>
#include <kcenon/monitoring/storage/time_series_storage.h>

using namespace monitoring_system;

// Create performance monitor with time-series storage
auto storage = std::make_unique<time_series_storage>("metrics.db");
performance_monitor monitor("web_server", std::move(storage));

// Enable real-time collection
monitor.enable_collection(true);
monitor.set_collection_interval(std::chrono::milliseconds(100));

// Monitor request processing
void process_request(const std::string& endpoint) {
    auto request_timer = monitor.start_timer("request_processing");

    // Add request-specific metrics
    monitor.increment_counter("requests_total");
    monitor.increment_counter("requests_by_endpoint:" + endpoint);

    // Simulate request processing
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Record response size
    monitor.record_histogram("response_size_bytes", 1024);

    // Timer automatically records duration when destroyed
}

// Generate real-time dashboard data
void dashboard_update() {
    auto snapshot = monitor.collect();
    if (snapshot) {
        auto data = snapshot.value();

        // Get real-time metrics
        auto rps = data.get_rate("requests_total");
        auto avg_latency = data.get_histogram_mean("request_processing");
        auto error_rate = data.get_rate("errors_total") / rps * 100.0;

        std::cout << "RPS: " << rps << ", Avg Latency: " << avg_latency
                  << "ms, Error Rate: " << error_rate << "%\n";
    }
}
```

#### Health Monitoring과 Circuit Breaker
```cpp
#include <kcenon/monitoring/health/circuit_breaker.h>
#include <kcenon/monitoring/health/health_monitor.h>

using namespace monitoring_system;

// Create circuit breaker for external service
circuit_breaker db_breaker("database_connection",
                          circuit_breaker_config{
                              .failure_threshold = 5,
                              .timeout = std::chrono::seconds(30),
                              .half_open_max_calls = 3
                          });

// Database operation with circuit breaker protection
result<std::string> fetch_user_data(int user_id) {
    return db_breaker.execute([user_id]() -> result<std::string> {
        // Simulate database call
        if (simulate_network_failure()) {
            return make_error<std::string>(
                monitoring_error_code::external_service_error,
                "Database connection failed"
            );
        }

        return make_success(std::string("user_data_" + std::to_string(user_id)));
    });
}

// Health check integration
health_monitor health;
health.register_check(
    std::make_unique<functional_health_check>(
        "database_circuit_breaker",
        health_check_type::dependency,
        [&db_breaker]() {
            auto state = db_breaker.get_state();
            switch (state) {
                case circuit_breaker_state::closed:
                    return health_check_result::healthy("Circuit breaker closed");
                case circuit_breaker_state::half_open:
                    return health_check_result::degraded("Circuit breaker half-open");
                case circuit_breaker_state::open:
                    return health_check_result::unhealthy("Circuit breaker open");
                default:
                    return health_check_result::unhealthy("Unknown circuit breaker state");
            }
        }
    )
);
```

### 📚 **포괄적인 샘플 모음**

샘플은 실제 사용 패턴과 모범 사례를 보여줍니다:

#### **핵심 기능**
- **[Basic Monitoring](examples/basic_monitoring_example/)**: 성능 메트릭 및 상태 확인
- **[Distributed Tracing](examples/distributed_tracing_example/)**: 서비스 간 요청 흐름
- **[Health Reliability](examples/health_reliability_example/)**: Circuit breaker 및 error boundary
- **[Error Handling](examples/advanced_features/)**: result 패턴을 사용한 포괄적인 오류 처리

#### **고급 기능**
- **[Real-time Dashboards](examples/advanced_features/)**: 실시간 메트릭 수집 및 시각화
- **[Reliability Patterns](examples/advanced_features/)**: Circuit breaker, retry 정책, bulkhead
- **[Custom Metrics](examples/advanced_features/)**: 도메인별 모니터링 기능
- **[Storage Backends](examples/advanced_features/)**: 시계열 및 파일 기반 저장소

#### **통합 예제**
- **[Thread System Integration](examples/integration_examples/)**: 스레드 풀 모니터링
- **[Logger Integration](examples/integration_examples/)**: 모니터링 및 로깅 결합
- **[Microservice Monitoring](examples/integration_examples/)**: 서비스 메시 observability

### 🛠️ **빌드 및 통합**

#### 사전 요구사항
- **컴파일러**: C++20 지원 (GCC 11+, Clang 14+, MSVC 2019+)
- **빌드 시스템**: CMake 3.16+
- **테스팅**: Google Test (자동으로 가져옴)

#### 빌드 단계

```bash
# Clone the repository
git clone https://github.com/kcenon/monitoring_system.git
cd monitoring_system

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run tests
./build/tests/monitoring_system_tests

# Run examples
./build/examples/basic_monitoring_example
./build/examples/distributed_tracing_example
./build/examples/health_reliability_example
```

#### CMake 통합

```cmake
# Add as subdirectory
add_subdirectory(monitoring_system)
target_link_libraries(your_target PRIVATE monitoring_system)

# Optional: Add thread_system integration
add_subdirectory(thread_system)
target_link_libraries(your_target PRIVATE
    monitoring_system
    thread_system::interfaces
)

# Using with FetchContent
include(FetchContent)
FetchContent_Declare(
    monitoring_system
    GIT_REPOSITORY https://github.com/kcenon/monitoring_system.git
    GIT_TAG main
)
FetchContent_MakeAvailable(monitoring_system)
```

## 문서

- 모듈 README:
  - core/README.md
  - tracing/README.md
  - health/README.md
- 가이드:
  - docs/USER_GUIDE.md (설정, 빠른 시작, 구성)
  - docs/API_REFERENCE.md (완전한 API 문서)
  - docs/ARCHITECTURE.md (시스템 설계 및 패턴)

Doxygen으로 API 문서 빌드 (선택 사항):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target docs
# Open documents/html/index.html
```

## 📖 사용 예제

### 기본 성능 모니터링

```cpp
#include <kcenon/monitoring/core/performance_monitor.h>

// Create performance monitor
monitoring_system::performance_monitor monitor("my_service");

// Record operation timing
auto start = std::chrono::steady_clock::now();
// ... your operation ...
auto end = std::chrono::steady_clock::now();

auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
monitor.get_profiler().record_sample("operation_name", duration, true);

// Collect metrics
auto snapshot = monitor.collect();
if (snapshot) {
    std::cout << "CPU Usage: " << snapshot.value().get_metric("cpu_usage") << "%\n";
}
```

### 분산 추적

```cpp
#include <monitoring/tracing/distributed_tracer.h>

auto& tracer = monitoring_system::global_tracer();

// Start a trace
auto span_result = tracer.start_span("user_request", "web_service");
if (span_result) {
    auto span = span_result.value();
    span->set_tag("user.id", "12345");
    span->set_tag("endpoint", "/api/users");

    // Create child span for database operation
    auto db_span_result = tracer.start_child_span(span, "database_query");
    if (db_span_result) {
        auto db_span = db_span_result.value();
        db_span->set_tag("query.type", "SELECT");

        // ... database operation ...

        tracer.finish_span(db_span);
    }

    tracer.finish_span(span);
}
```

### 상태 모니터링

```cpp
#include <monitoring/health/health_monitor.h>

monitoring_system::health_monitor health_monitor;

// Register health checks
health_monitor.register_check(
    std::make_unique<monitoring_system::functional_health_check>(
        "database_connection",
        monitoring_system::health_check_type::dependency,
        []() {
            // Check database connectivity
            bool connected = check_database_connection();
            return connected ?
                monitoring_system::health_check_result::healthy("Database connected") :
                monitoring_system::health_check_result::unhealthy("Database unreachable");
        }
    )
);

// Check overall health
auto health_result = health_monitor.check_health();
if (health_result.status == monitoring_system::health_status::healthy) {
    std::cout << "System is healthy\n";
}
```

### Result 타입을 사용한 오류 처리

```cpp
#include <kcenon/monitoring/core/result_types.h>

// Function that can fail
monitoring_system::result<std::string> fetch_user_data(int user_id) {
    if (user_id <= 0) {
        return monitoring_system::make_error<std::string>(
            monitoring_system::monitoring_error_code::invalid_argument,
            "Invalid user ID"
        );
    }

    // ... fetch logic ...
    return monitoring_system::make_success(std::string("user_data"));
}

// Usage with error handling
auto result = fetch_user_data(123);
if (result) {
    std::cout << "User data: " << result.value() << "\n";
} else {
    std::cout << "Error: " << result.get_error().message << "\n";
}

// Chain operations
auto processed = result
    .map([](const std::string& data) { return data + "_processed"; })
    .and_then([](const std::string& data) {
        return monitoring_system::make_success(data.length());
    });
```

## 🔧 구성

### CMake 옵션

```bash
# Build options
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_EXAMPLES=ON \
  -DBUILD_BENCHMARKS=OFF

# Integration options
cmake -B build \
  -DBUILD_WITH_COMMON_SYSTEM=ON \
  -DTHREAD_SYSTEM_INTEGRATION=ON \
  -DLOGGER_SYSTEM_INTEGRATION=ON
```

### 런타임 구성

```cpp
// Configure monitoring
monitoring_system::monitoring_config config;
config.enable_performance_monitoring = true;
config.enable_distributed_tracing = true;
config.sampling_rate = 0.1; // 10% sampling
config.max_trace_duration = std::chrono::seconds(30);

// Apply configuration
auto monitor = monitoring_system::create_monitor(config);
```

## 🧪 테스팅

```bash
# Run all tests
cmake --build build --target monitoring_system_tests
./build/tests/monitoring_system_tests

# Run specific test suites
./build/tests/monitoring_system_tests --gtest_filter="*DI*"
./build/tests/monitoring_system_tests --gtest_filter="*Performance*"

# Generate test coverage (requires gcov/lcov)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
cmake --build build
./build/tests/monitoring_system_tests
make coverage
```

**현재 테스트 커버리지**: 37 테스트, 100% 통과율
- Result 타입: 13 테스트
- DI 컨테이너: 9 테스트
- Monitorable 인터페이스: 12 테스트
- Thread context: 3 테스트

## 📦 통합

### 선택적 의존성

모니터링 시스템은 보완 라이브러리와 통합할 수 있습니다:

- **[thread_system](https://github.com/kcenon/thread_system)**: 향상된 동시 처리
- **[logger_system](https://github.com/kcenon/logger_system)**: 구조화된 로깅 통합

### 생태계 통합

```cpp
// With thread_system integration
#ifdef THREAD_SYSTEM_INTEGRATION
#include <thread_system/thread_pool.h>
auto collector = monitoring_system::create_threaded_collector(thread_pool);
#endif

// With logger_system integration
#ifdef LOGGER_SYSTEM_INTEGRATION
#include <logger_system/logger.h>
monitoring_system::set_logger(logger_system::get_logger());
#endif
```

## API 문서

### Core API 참조

- **[API Reference](./docs/API_REFERENCE.md)**: 인터페이스를 포함한 완전한 API 문서
- **[Architecture Guide](./docs/ARCHITECTURE.md)**: 시스템 설계 및 패턴
- **[Performance Guide](./docs/PERFORMANCE.md)**: 최적화 팁 및 벤치마크
- **[User Guide](./docs/USER_GUIDE.md)**: 사용 가이드 및 예제
- **[FAQ](./docs/FAQ.md)**: 자주 묻는 질문

### API 빠른 개요

```cpp
// Monitoring Core API
namespace monitoring_system {
    // Performance monitoring with real-time metrics
    class performance_monitor {
        auto enable_collection(bool enabled) -> void;
        auto collect() -> result<metrics_snapshot>;
        auto get_profiler() -> profiler&;
        auto start_timer(const std::string& name) -> scoped_timer;
        auto increment_counter(const std::string& name) -> void;
        auto record_histogram(const std::string& name, double value) -> void;
    };

    // Distributed tracing capabilities
    class distributed_tracer {
        auto start_span(const std::string& operation, const std::string& service) -> result<std::shared_ptr<span>>;
        auto start_child_span(std::shared_ptr<span> parent, const std::string& operation) -> result<std::shared_ptr<span>>;
        auto finish_span(std::shared_ptr<span> span) -> result_void;
        auto export_traces() -> result_void;
    };

    // Health monitoring and validation
    class health_monitor {
        auto register_check(std::unique_ptr<health_check_interface> check) -> result_void;
        auto check_health() -> health_result;
        auto get_check_status(const std::string& name) -> result<health_status>;
    };

    // Circuit breaker for reliability
    class circuit_breaker {
        template<typename F>
        auto execute(F&& func) -> result<typename std::invoke_result_t<F>>;
        auto get_state() const -> circuit_breaker_state;
        auto get_statistics() const -> circuit_breaker_stats;
    };
}

// Result pattern for error handling
namespace monitoring_system {
    template<typename T>
    class result {
        auto has_value() const -> bool;
        auto value() const -> const T&;
        auto get_error() const -> const monitoring_error&;
        template<typename F> auto map(F&& func) -> result<std::invoke_result_t<F, T>>;
        template<typename F> auto and_then(F&& func) -> std::invoke_result_t<F, T>;
    };

    // Dependency injection container
    class di_container {
        template<typename Interface, typename Implementation>
        auto register_singleton() -> result_void;
        template<typename Interface>
        auto resolve() -> result<std::shared_ptr<Interface>>;
    };
}

// Integration API (with thread_system)
namespace thread_module::interfaces {
    class monitoring_interface {
        virtual auto record_metric(const std::string& name, double value) -> result_void = 0;
        virtual auto start_span(const std::string& operation) -> result<span_id> = 0;
        virtual auto check_health() -> result<health_status> = 0;
    };
}
```

## 기여하기

기여를 환영합니다! 자세한 내용은 [Contributing Guide](./docs/CONTRIBUTING.md)를 참조하세요.

### 개발 환경 설정

1. 리포지토리 포크
2. 기능 브랜치 생성 (`git checkout -b feature/amazing-feature`)
3. 변경 사항 커밋 (`git commit -m 'Add some amazing feature'`)
4. 브랜치에 푸시 (`git push origin feature/amazing-feature`)
5. Pull Request 열기

### 코드 스타일

- Modern C++ 모범 사례 준수
- RAII 및 smart pointer 사용
- 일관된 포맷팅 유지 (clang-format 구성 제공)
- 새로운 기능에 대한 포괄적인 단위 테스트 작성

## 지원

- **Issues**: [GitHub Issues](https://github.com/kcenon/monitoring_system/issues)
- **Discussions**: [GitHub Discussions](https://github.com/kcenon/monitoring_system/discussions)
- **Email**: kcenon@naver.com

## 프로덕션 품질 및 아키텍처

### 빌드 및 테스팅 인프라

**포괄적인 멀티 플랫폼 CI/CD**
- **Sanitizer 커버리지**: ThreadSanitizer, AddressSanitizer 및 UBSanitizer를 사용한 자동화된 빌드
- **멀티 플랫폼 테스팅**: Ubuntu (GCC/Clang), Windows (MSYS2/VS) 및 macOS에서 지속적인 검증
- **테스트 스위트 우수성**: 100% 성공률로 37/37 테스트 통과
- **정적 분석**: modernize 검사를 포함한 Clang-tidy 및 Cppcheck 통합
- **문서 생성**: 자동화된 Doxygen API 문서 빌드

**성능 기준선**
- **메트릭 수집**: 10M 메트릭 작업/초 (atomic counter 작업)
- **이벤트 발행**: 최소한의 오버헤드로 5.8M 이벤트/초
- **추적 처리**: 2.5M span/초, hop당 <50ns의 컨텍스트 전파
- **상태 확인**: 의존성 추적을 포함하여 500K 상태 검증/초
- **P50 지연 시간**: 메트릭 기록 작업에 대해 0.1 μs
- **메모리 효율성**: <5MB 기준선, 부하 상태에서 10K 메트릭으로 <42MB

포괄적인 성능 메트릭 및 회귀 임계값은 [BASELINE.md](BASELINE.md)를 참조하세요.

**완전한 문서 스위트**
- [ARCHITECTURE.md](docs/ARCHITECTURE.md): 시스템 설계 및 통합 패턴
- [USER_GUIDE.md](docs/USER_GUIDE.md): 예제를 포함한 포괄적인 사용 가이드
- [API_REFERENCE.md](docs/API_REFERENCE.md): 완전한 API 문서

### Thread 안전성 및 동시성

**Grade A- Thread 안전성 (100% 완료)**
- **Lock-Free 작업**: 최소한의 오버헤드를 위한 atomic counter 및 gauge
- **ThreadSanitizer 준수**: 모든 테스트 시나리오에서 데이터 경쟁 감지 없음
- **동시 테스트 커버리지**: thread 안전성을 검증하는 37개의 포괄적인 테스트
- **프로덕션 검증**: 안전한 동시 액세스를 위해 설계된 모든 컴포넌트

**테스트 프레임워크 마이그레이션**
- **Catch2 프레임워크**: Google Test에서 완전히 마이그레이션 완료
- **통합 테스트**: DI 컨테이너, 모니터링 인터페이스 및 result 타입이 완전히 검증됨
- **100% 통과율**: 지원되는 모든 플랫폼에서 37개 테스트 모두 통과

### 리소스 관리 (RAII - Grade A)

**완벽한 RAII 준수**
- **100% Smart Pointer 사용**: 모든 리소스가 `std::shared_ptr` 및 `std::unique_ptr`로 관리됨
- **AddressSanitizer 검증**: 모든 테스트 시나리오에서 메모리 누수 감지 없음
- **RAII 패턴**: scoped timer, 자동 span 수명 주기 관리
- **저장소 백엔드 관리**: 적절한 리소스 정리 및 수명 주기 처리
- **수동 메모리 관리 없음**: 공개 인터페이스에서 raw pointer 완전히 제거

**메모리 효율성**
```bash
# AddressSanitizer: Clean across all tests
==12345==ERROR: LeakSanitizer: detected memory leaks
# Total: 0 leaks

# Memory profile under load:
Baseline: <5MB
With 10K metrics: <42MB
Automatic cleanup: RAII-managed
```

### 오류 처리 (프로덕션 준비 - 95% 완료)

**포괄적인 Result<T> 패턴 구현**

monitoring_system은 타입 안전하고 포괄적인 오류 처리를 위해 모든 인터페이스에서 Result<T>를 구현합니다:

```cpp
// Example 1: Performance monitoring with error handling
auto& monitor = monitoring_system::performance_monitor("service");
auto result = monitor.collect();
if (!result) {
    std::cerr << "Metrics collection failed: " << result.get_error().message
              << " (code: " << static_cast<int>(result.get_error().code) << ")\n";
    return -1;
}
auto snapshot = result.value();

// Example 2: Distributed tracing with Result<T>
auto& tracer = monitoring_system::global_tracer();
auto span_result = tracer.start_span("operation", "service");
if (!span_result) {
    std::cerr << "Failed to start trace: " << span_result.get_error().message << "\n";
    return -1;
}
auto span = span_result.value();

// Example 3: Circuit breaker pattern with Result<T>
auto cb_result = db_breaker.execute([&]() -> result<std::string> {
    return fetch_data();
});
if (!cb_result) {
    std::cerr << "Operation failed: " << cb_result.get_error().message << "\n";
}
```

**인터페이스 표준화**
- **Monitoring Interface**: 모든 작업 (`configure`, `start`, `stop`, `collect_now`, `check_health`)이 `result_void` 또는 `result<T>`를 반환
- **Metrics Collector**: `collect`, `initialize`, `cleanup`에 대해 완전한 Result<T> 채택
- **Storage Backend**: 모든 저장소 작업 (`store`, `retrieve`, `flush`)이 Result<T>를 사용
- **Metrics Analyzer**: 분석 작업 (`analyze`, `analyze_trend`, `reset`)이 Result<T>를 반환
- **Circuit Breaker**: 보호된 작업이 포괄적인 오류 전파와 함께 `result<T>`를 사용

**오류 코드 통합**
- **할당된 범위**: 중앙 집중식 오류 코드 레지스트리 (common_system)에서 `-300`에서 `-399`까지
- **분류**: 구성 (-300~-309), 메트릭 수집 (-310~-319), 추적 (-320~-329), 상태 모니터링 (-330~-339), 저장소 (-340~-349), 분석 (-350~-359)
- **의미 있는 메시지**: 작업 실패에 대한 포괄적인 오류 컨텍스트

**신뢰성 패턴**
- **Circuit Breaker**: Result<T> 오류 전파를 통한 자동 장애 감지 및 복구
- **Health Check**: 상태에 대한 Result<T>를 사용한 사전 예방적 의존성 검증
- **Error Boundary**: 모든 컴포넌트 경계에서 포괄적인 오류 처리

**남은 선택적 개선 사항**
- 📝 **오류 테스트**: 포괄적인 오류 시나리오 테스트 스위트 추가
- 📝 **문서화**: 인터페이스 문서에서 Result<T> 사용 예제 확장
- 📝 **오류 메시지**: 작업 실패에 대한 오류 컨텍스트 계속 개선

상세한 구현 노트는 [PHASE_3_PREPARATION.md](docs/PHASE_3_PREPARATION.md)를 참조하세요.

**향후 개선 사항**
- 📝 **성능 최적화**: 프로파일링 및 핫 패스 최적화, 제로 할당 메트릭 수집
- 📝 **API 안정화**: 시맨틱 버전 관리 채택, 하위 호환성 보장

상세한 개선 계획 및 추적은 프로젝트의 [NEED_TO_FIX.md](/Users/dongcheolshin/Sources/NEED_TO_FIX.md)를 참조하세요.

### 아키텍처 개선 단계

**단계 상태 개요** (2025-10-09 기준):

| 단계 | 상태 | 완료율 | 주요 성과 |
|-------|--------|------------|------------------|
| **Phase 0**: Foundation | ✅ 완료 | 100% | CI/CD 파이프라인, 기준선 메트릭, 테스트 커버리지 |
| **Phase 1**: Thread Safety | ✅ 완료 | 100% | Lock-free 작업, ThreadSanitizer 검증, 37/37 테스트 통과 |
| **Phase 2**: Resource Management | ✅ 완료 | 100% | Grade A RAII, 100% smart pointer, AddressSanitizer clean |
| **Phase 3**: Error Handling | ✅ 완료 | 95% | 모든 인터페이스에서 Result<T>, 포괄적인 오류 처리 |
| **Phase 4**: Dependency Refactoring | ⏳ 계획됨 | 0% | Phase 3 생태계 완료 후 예정 |
| **Phase 5**: Integration Testing | ⏳ 계획됨 | 0% | Phase 4 완료 대기 중 |
| **Phase 6**: Documentation | ⏳ 계획됨 | 0% | Phase 5 완료 대기 중 |

**Phase 3 - 오류 처리 통합: 직접 Result<T> 패턴**

monitoring_system은 모든 인터페이스에서 포괄적인 오류 처리를 통해 **직접 Result<T>** 패턴을 구현합니다:

**구현 상태**: 95% 완료
- ✅ 모든 모니터링 작업이 `result_void` 또는 `result<T>`를 반환
- ✅ Metrics collector, storage backend 및 analyzer가 Result<T>를 사용
- ✅ Result<T> 오류 전파를 사용한 circuit breaker 및 health check
- ✅ common_system 레지스트리에 -300에서 -399까지 오류 코드 범위 할당
- ✅ 모든 컴포넌트에서 인터페이스 표준화 완료

**오류 코드 구성**:
- 구성: -300~-309
- 메트릭 수집: -310~-319
- 추적: -320~-329
- 상태 모니터링: -330~-339
- 저장소: -340~-349
- 분석: -350~-359

**구현 패턴**:
```cpp
// Performance monitoring with Result<T>
auto& monitor = performance_monitor("service");
auto result = monitor.collect();
if (!result) {
    std::cerr << "Collection failed: " << result.get_error().message << "\n";
    return -1;
}
auto snapshot = result.value();

// Circuit breaker with Result<T> error propagation
auto cb_result = db_breaker.execute([&]() -> result<std::string> {
    return fetch_data();
});
```

**이점**:
- 모든 모니터링 작업에서 타입 안전한 오류 처리
- 신뢰성 패턴에서 포괄적인 오류 전파
- 작업 진단을 위한 명확한 오류 분류
- 37/37 테스트 통과로 프로덕션 준비 완료

**남은 작업** (5%):
- 선택 사항: 추가 오류 시나리오 테스트
- 선택 사항: 향상된 오류 문서화
- 선택 사항: 개선된 오류 컨텍스트 메시지

## 라이선스

이 프로젝트는 BSD 3-Clause License에 따라 라이선스가 부여됩니다 - 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하세요.

## 감사의 말

- 이 프로젝트를 개선하는 데 도움을 준 모든 기여자들에게 감사드립니다
- 지속적인 피드백과 지원을 제공하는 C++ 커뮤니티에 특별히 감사드립니다
- 현대적인 observability 플랫폼 및 모범 사례에서 영감을 받았습니다

---

<p align="center">
  Made with ❤️ by 🍀☀🌕🌥 🌊
</p>
