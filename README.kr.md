[![CI](https://github.com/kcenon/monitoring_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/ci.yml)
[![Code Coverage](https://github.com/kcenon/monitoring_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/coverage.yml)
[![Static Analysis](https://github.com/kcenon/monitoring_system/actions/workflows/static-analysis.yml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/static-analysis.yml)
[![Documentation](https://github.com/kcenon/monitoring_system/actions/workflows/build-Doxygen.yaml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/build-Doxygen.yaml)

# Monitoring System Project

> **Language:** [English](README.md) | **한국어**

## 목차

- [개요](#개요)
- [요구사항](#요구사항)
- [생태계 통합](#-생태계-통합)
- [빠른 시작](#빠른-시작)
- [핵심 기능](#핵심-기능)
- [성능 하이라이트](#성능-하이라이트)
- [아키텍처 개요](#아키텍처-개요)
- [문서](#문서)
- [CMake 통합](#cmake-통합)
- [구성](#구성)
- [테스팅](#테스팅)
- [프로덕션 품질](#프로덕션-품질)
- [기여하기](#지원-및-기여)
- [라이선스](#라이선스)

## 개요

고성능 애플리케이션을 위한 포괄적인 모니터링, 분산 추적 및 신뢰성 기능을 제공하는 개발 중인 C++17 관찰성 플랫폼입니다. 원활한 생태계 통합을 위한 모듈식 인터페이스 기반 아키텍처로 구축되었습니다.

**핵심 가치 제안**:
- **성능 우수성**: 10M+ 메트릭 작업/초, <50ns 컨텍스트 전파
- **고품질 신뢰성**: 스레드 안전 설계, 포괄적인 오류 처리, circuit breaker
- **개발자 생산성**: 직관적인 API, 풍부한 텔레메트리, 모듈식 컴포넌트
- **엔터프라이즈 준비**: 분산 추적, 상태 모니터링, 신뢰성 패턴

**최신 상태**: ✅ 모든 CI/CD 파이프라인 정상, 37/37 테스트 통과 (100% 통과율)

---

## 요구사항

| 의존성 | 버전 | 필수 | 설명 |
|--------|------|------|------|
| C++20 컴파일러 | GCC 13+ / Clang 17+ / MSVC 2022+ / Apple Clang 14+ | 예 | thread_system 의존성으로 인한 높은 요구사항 |
| CMake | 3.20+ | 예 | 빌드 시스템 |
| [common_system](https://github.com/kcenon/common_system) | latest | 예 | 공통 인터페이스 (IMonitor, Result<T>) |
| [thread_system](https://github.com/kcenon/thread_system) | latest | 예 | 스레드 풀 및 비동기 작업 |
| [logger_system](https://github.com/kcenon/logger_system) | latest | 선택 | 로깅 기능 |

### 의존성 구조

```
monitoring_system
├── common_system (필수)
├── thread_system (필수)
│   └── common_system
└── logger_system (선택)
    └── common_system
```

### 의존성과 함께 빌드

```bash
# 모든 의존성 클론
git clone https://github.com/kcenon/common_system.git
git clone https://github.com/kcenon/thread_system.git
git clone https://github.com/kcenon/logger_system.git
git clone https://github.com/kcenon/monitoring_system.git

# monitoring_system 빌드
cd monitoring_system
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

📖 **[Quick Start Guide →](docs/guides/QUICK_START.md)** | **[빠른 시작 가이드 →](docs/guides/QUICK_START_KO.md)**

---

## 🔗 생태계 통합

명확한 인터페이스 경계를 가진 모듈식 C++ 생태계의 일부:

**의존성**:
- **[common_system](https://github.com/kcenon/common_system)**: 핵심 인터페이스 (IMonitor, ILogger, Result<T>)
- **[thread_system](https://github.com/kcenon/thread_system)**: 스레딩 프리미티브 (필수)
- **[logger_system](https://github.com/kcenon/logger_system)**: 로깅 기능 (선택)

**통합 패턴**:
```
common_system (interfaces) ← monitoring_system implements IMonitor
                          ↖ optional: inject ILogger at runtime
```

**이점**: 인터페이스 전용 의존성, 독립 컴파일, 런타임 DI, 명확한 분리

📖 [완전한 생태계 통합 가이드 →](../ECOSYSTEM.md)

---

## 빠른 시작

### 설치

```bash
# 저장소 복제
git clone https://github.com/kcenon/monitoring_system.git
cd monitoring_system

# 구성 및 빌드
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 테스트 실행
./build/tests/monitoring_system_tests

# 예제 실행
./build/examples/basic_monitoring_example
```

### 기본 사용법

```cpp
#include <kcenon/monitoring/core/performance_monitor.h>
#include <kcenon/monitoring/tracing/distributed_tracer.h>
#include <kcenon/monitoring/health/health_monitor.h>

using namespace monitoring_system;

int main() {
    // 1. 모니터링 컴포넌트 생성
    performance_monitor monitor("my_service");
    auto& tracer = global_tracer();
    health_monitor health;

    // 2. 메트릭 수집 활성화
    monitor.enable_collection(true);

    // 3. 분산 추적 시작
    auto span_result = tracer.start_span("main_operation", "service");
    if (!span_result) {
        std::cerr << "추적 시작 실패: " << span_result.error().message << "\n";
        return -1;
    }
    auto span = span_result.value();
    span->set_tag("operation.type", "batch_processing");

    // 4. 작업 모니터링
    auto timer = monitor.start_timer("processing");
    for (int i = 0; i < 1000; ++i) {
        monitor.increment_counter("items_processed");
        // ... 처리 로직 ...
    }

    // 5. 메트릭 수집
    auto snapshot = monitor.collect();
    if (snapshot) {
        std::cout << "CPU: " << snapshot.value().get_metric("cpu_usage") << "%\n";
        std::cout << "처리됨: " << snapshot.value().get_metric("items_processed") << "\n";
    }

    // 6. 추적 완료
    tracer.finish_span(span);
    tracer.export_traces();

    return 0;
}
```

📖 [완전한 사용자 가이드 →](docs/guides/USER_GUIDE.md)

---

## 핵심 기능

- **성능 모니터링**: 실시간 메트릭 (카운터, 게이지, 히스토그램) - 10M+ ops/초
- **분산 추적**: 서비스 간 요청 흐름 추적 - 2.5M span/초
- **상태 모니터링**: 서비스 상태 확인 및 의존성 검증 - 500K 확인/초
- **오류 처리**: 타입 안전 오류 관리를 위한 강력한 Result<T> 패턴
- **의존성 주입**: 수명 주기 관리를 갖춘 완전한 DI 컨테이너
- **Circuit Breaker**: 자동 장애 감지 및 복구
- **스토리지 백엔드**: 메모리, 파일 및 시계열 스토리지 옵션
- **스레드 안전**: atomic 카운터 및 lock을 사용한 동시 작업

📚 [세부 기능 →](docs/FEATURES.md)

---

## 성능 하이라이트

*Apple M1 (8-core) @ 3.2GHz, 16GB RAM, macOS Sonoma에서 벤치마크*

| 작업 | 처리량 | 지연 시간 (P95) | 메모리 |
|------|--------|----------------|--------|
| **카운터 작업** | 10.5M ops/sec | 120 ns | <1MB |
| **Span 생성** | 2.5M spans/sec | 580 ns | 384 bytes/span |
| **상태 확인** | 520K checks/sec | 2.85 μs | <3MB |
| **컨텍스트 전파** | 15M ops/sec | <50 ns | Thread-local |

**플랫폼**: Apple M1 @ 3.2GHz

### 업계 비교

| 솔루션 | 카운터 Ops/sec | 메모리 | 기능 |
|--------|----------------|--------|------|
| **Monitoring System** | 10.5M | <5MB | 완전한 관찰성 |
| Prometheus Client | 2.5M | 15MB | 메트릭만 |
| OpenTelemetry | 1.8M | 25MB | 복잡한 API |

⚡ [전체 벤치마크 →](docs/BENCHMARKS.md)

---

## 아키텍처 개요

```
┌─────────────────────────────────────────────────────────────────┐
│                     Monitoring System                           │
├─────────────────────────────────────────────────────────────────┤
│ Core Components                                                 │
├─────────────────────┬───────────────────┬───────────────────────┤
│ Performance Monitor │ Distributed Tracer │ Health Monitor        │
│ • 메트릭 수집       │ • Span 관리        │ • 서비스 확인         │
│ • 프로파일링 데이터 │ • 컨텍스트 전파    │ • 의존성 추적         │
│ • 집계              │ • 추적 내보내기    │ • 복구 정책           │
└─────────────────────┴───────────────────┴───────────────────────┘
```

**주요 특징**:
- **인터페이스 중심 설계**: 추상 인터페이스를 통한 명확한 분리
- **모듈식 컴포넌트**: 플러그 가능한 스토리지, tracer, health checker
- **순환 의존성 제로**: common_system을 통한 인터페이스 전용 의존성
- **프로덕션 등급**: 100% 테스트 통과율, <10% 오버헤드

🏗️ [아키텍처 가이드 →](docs/01-ARCHITECTURE.md)

---

## 문서

### 시작하기
- 📖 [사용자 가이드](docs/guides/USER_GUIDE.md) - 포괄적인 사용 가이드
- 🚀 [빠른 시작 예제](examples/) - 작동하는 코드 예제
- 🔧 [통합 가이드](docs/guides/INTEGRATION.md) - 생태계 통합

### 핵심 문서
- 📘 [API 참조](docs/02-API_REFERENCE.md) - 완전한 API 문서
- 📚 [기능](docs/FEATURES.md) - 상세한 기능 설명
- ⚡ [벤치마크](docs/BENCHMARKS.md) - 성능 메트릭 및 비교
- 🏗️ [아키텍처](docs/01-ARCHITECTURE.md) - 시스템 설계 및 패턴
- 📦 [프로젝트 구조](docs/PROJECT_STRUCTURE.md) - 파일 구성

### 고급 주제
- ✅ [모범 사례](docs/guides/BEST_PRACTICES.md) - 사용 권장 사항
- 🔍 [문제 해결](docs/guides/TROUBLESHOOTING.md) - 일반적인 문제 및 해결책
- 📋 [FAQ](docs/guides/FAQ.md) - 자주 묻는 질문
- 🔄 [마이그레이션 가이드](docs/guides/MIGRATION_GUIDE.md) - 버전 마이그레이션

### 개발
- 🤝 [기여하기](docs/contributing/CONTRIBUTING.md) - 기여 지침
- 🏆 [프로덕션 품질](docs/PRODUCTION_QUALITY.md) - CI/CD 및 품질 메트릭
- 📊 [성능 기준선](docs/performance/BASELINE.md) - 회귀 임계값

---

## CMake 통합

### 하위 디렉토리로 추가

```cmake
# 모니터링 시스템 추가
add_subdirectory(monitoring_system)
target_link_libraries(your_target PRIVATE monitoring_system)

# 선택: 생태계 통합 추가
add_subdirectory(thread_system)
add_subdirectory(logger_system)

target_link_libraries(your_target PRIVATE
    monitoring_system
    thread_system::interfaces
    logger_system
)
```

### FetchContent 사용

```cmake
include(FetchContent)

FetchContent_Declare(
    monitoring_system
    GIT_REPOSITORY https://github.com/kcenon/monitoring_system.git
    GIT_TAG v0.1.0 # Pin to a specific release tag; do NOT use main
)

FetchContent_MakeAvailable(monitoring_system)

target_link_libraries(your_target PRIVATE monitoring_system)
```

### 빌드 옵션

```bash
# 테스트 및 예제 포함 빌드
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_EXAMPLES=ON \
  -DBUILD_BENCHMARKS=OFF

# 생태계 통합 활성화
cmake -B build \
  -DBUILD_WITH_COMMON_SYSTEM=ON \
  -DTHREAD_SYSTEM_INTEGRATION=ON \
  -DLOGGER_SYSTEM_INTEGRATION=ON
```

---

## 구성

### 런타임 구성

```cpp
// 모니터링 구성
monitoring_config config;
config.enable_performance_monitoring = true;
config.enable_distributed_tracing = true;
config.sampling_rate = 0.1;  // 10% 샘플링
config.max_trace_duration = std::chrono::seconds(30);

// 스토리지 구성
auto storage = std::make_unique<memory_storage>(memory_storage_config{
    .max_entries = 10000,
    .retention_period = std::chrono::hours(1)
});

// 구성으로 모니터 생성
auto monitor = create_monitor(config, std::move(storage));
```

---

## 테스팅

```bash
# 모든 테스트 실행
cmake --build build --target monitoring_system_tests
./build/tests/monitoring_system_tests

# 특정 테스트 스위트 실행
./build/tests/monitoring_system_tests --gtest_filter="*DI*"
./build/tests/monitoring_system_tests --gtest_filter="*Performance*"

# 커버리지 리포트 생성
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
cmake --build build
./build/tests/monitoring_system_tests
make coverage
```

**테스트 상태**: ✅ 37/37 테스트 통과 (100% 통과율)

**테스트 커버리지**:
- 라인 커버리지: 87.3%
- 함수 커버리지: 92.1%
- 분기 커버리지: 78.5%

---

## 프로덕션 품질

### 품질 등급

| 측면 | 등급 | 상태 |
|------|------|------|
| **스레드 안전성** | A- | ✅ TSan clean, 0 데이터 경합 |
| **리소스 관리** | A | ✅ ASan clean, 0 누수 |
| **오류 처리** | A- | ✅ Result<T> 패턴, 95% 완료 |
| **테스트 커버리지** | A | ✅ 37/37 테스트, 100% 통과율 |
| **CI/CD** | A | ✅ 멀티 플랫폼, 모두 정상 |

### CI/CD 검증

**테스트된 플랫폼**:
- Linux (Ubuntu 22.04): GCC 11, Clang 14
- macOS (macOS 12): Apple Clang 14
- Windows (Server 2022): MSVC 2022, MSYS2

**Sanitizer**:
- ✅ AddressSanitizer: 0 누수, 0 오류
- ✅ ThreadSanitizer: 0 데이터 경합
- ✅ UndefinedBehaviorSanitizer: 0 문제

**정적 분석**:
- ✅ clang-tidy: 0 경고
- ✅ cppcheck: 0 경고
- ✅ cpplint: 0 문제

🏆 [프로덕션 품질 메트릭 →](docs/PRODUCTION_QUALITY.md)

---

## 실제 사용 사례

**이상적인 애플리케이션**:
- **마이크로서비스**: 분산 추적 및 서비스 상태 모니터링
- **고빈도 거래**: 초저지연 성능 모니터링
- **실시간 시스템**: 지속적인 상태 확인 및 circuit breaker 보호
- **웹 애플리케이션**: 요청 추적 및 병목 지점 식별
- **IoT 플랫폼**: 리소스 사용량 모니터링 및 신뢰성 패턴

---

## 생태계 통합

이 모니터링 시스템은 다른 KCENON 시스템과 원활하게 통합됩니다:

```cpp
// thread_system 통합
#include <thread_system/thread_pool.h>
auto collector = create_threaded_collector(thread_pool);

// logger_system 통합
#include <logger_system/logger.h>
monitoring_system::set_logger(logger_system::get_logger());
```

🌐 [생태계 통합 가이드 →](../ECOSYSTEM.md)

---

## 지원 및 기여

### 도움 받기
- 💬 [GitHub Discussions](https://github.com/kcenon/monitoring_system/discussions) - 질문하기
- 🐛 [이슈 트래커](https://github.com/kcenon/monitoring_system/issues) - 버그 보고
- 📧 이메일: kcenon@naver.com

### 기여하기
기여를 환영합니다! 자세한 내용은 [기여 가이드](docs/contributing/CONTRIBUTING.md)를 참조하세요.

**빠른 시작**:
1. 저장소 포크
2. 기능 브랜치 생성 (`git checkout -b feature/amazing-feature`)
3. 변경 사항 커밋 (`git commit -m 'Add some amazing feature'`)
4. 브랜치에 푸시 (`git push origin feature/amazing-feature`)
5. Pull Request 열기

---

## 라이선스

이 프로젝트는 BSD 3-Clause License에 따라 라이선스가 부여됩니다 - 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하세요.

---

## 감사의 말

- 이 프로젝트를 개선하는 데 도움을 준 모든 기여자들에게 감사드립니다
- 지속적인 피드백과 지원을 제공하는 C++ 커뮤니티에 특별히 감사드립니다
- 현대적인 관찰성 플랫폼 및 모범 사례에서 영감을 받았습니다

---

<p align="center">
  Made with ❤️ by 🍀☀🌕🌥 🌊
</p>
