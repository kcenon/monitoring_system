[![CI](https://github.com/kcenon/monitoring_system/actions/workflows/ci.yml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/ci.yml)
[![Code Coverage](https://github.com/kcenon/monitoring_system/actions/workflows/coverage.yml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/coverage.yml)
[![Static Analysis](https://github.com/kcenon/monitoring_system/actions/workflows/static-analysis.yml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/static-analysis.yml)
[![codecov](https://codecov.io/gh/kcenon/monitoring_system/branch/main/graph/badge.svg)](https://codecov.io/gh/kcenon/monitoring_system)
[![Documentation](https://github.com/kcenon/monitoring_system/actions/workflows/build-Doxygen.yaml/badge.svg)](https://github.com/kcenon/monitoring_system/actions/workflows/build-Doxygen.yaml)
[![License](https://img.shields.io/github/license/kcenon/monitoring_system)](https://github.com/kcenon/monitoring_system/blob/main/LICENSE)

# Monitoring System

> **Language:** [English](README.md) | **한국어**

고성능 애플리케이션을 위한 포괄적인 모니터링, 분산 추적 및 신뢰성 기능을 제공하는 현대적인 C++20 관측성 플랫폼입니다.

## 목차

- [개요](#개요)
- [주요 기능](#주요-기능)
- [요구사항](#요구사항)
- [빠른 시작](#빠른-시작)
- [설치](#설치)
- [아키텍처](#아키텍처)
- [핵심 개념](#핵심-개념)
- [API 개요](#api-개요)
- [예제](#예제)
- [성능](#성능)
- [생태계 통합](#생태계-통합)
- [기여하기](#기여하기)
- [라이선스](#라이선스)

---

## 개요

Monitoring System은 모듈식 인터페이스 기반 아키텍처로 구축된 포괄적인 관측성 플랫폼입니다.

**핵심 가치**:
- **성능 우수**: 초당 10M+ 메트릭 연산, <50ns 컨텍스트 전파
- **신뢰성 설계**: 스레드 안전 설계, 포괄적 오류 처리, 서킷 브레이커
- **개발자 생산성**: 직관적 API, 풍부한 텔레메트리, 모듈식 컴포넌트
- **엔터프라이즈 준비**: 분산 추적, 헬스 모니터링, 신뢰성 패턴

**최신 상태**: 모든 CI/CD 파이프라인 정상, 37/37 테스트 통과 (100% 통과율)

---

## 주요 기능

| 기능 | 설명 | 상태 |
|------|------|------|
| **성능 모니터** | 핵심 모니터링 (IMonitor 구현) | 안정 |
| **분산 추적** | W3C 스타일 trace_id/span_id 계층 | 안정 |
| **중앙 수집기** | 다중 메트릭 소스 집계 | 안정 |
| **Collector Factory** | 런타임 DI 기반 수집기 팩토리 | 안정 |
| **헬스 모니터링** | 의존성 그래프 기반 헬스 체크 | 안정 |
| **서킷 브레이커** | 장애 격리 및 자동 복구 | 안정 |
| **에러 바운더리** | 오류 전파 차단 | 안정 |
| **알림 파이프라인** | 트리거, 노티파이어, 매니저 | 안정 |
| **SIMD 집계** | AVX2/NEON 메트릭 집계 | 안정 |
| **플러그인 아키텍처** | 수집기 플러그인 로더 | 안정 |

---

## 요구사항

### 컴파일러 매트릭스

| 컴파일러 | 최소 버전 | 비고 |
|----------|----------|------|
| GCC | 13+ | thread_system 전이 의존성 |
| Clang | 17+ | thread_system 전이 의존성 |
| Apple Clang | 14+ | macOS 지원 |
| MSVC | 2022+ | C++20 기능 필수 |

> monitoring_system 자체는 C++20만 필요하지만, [thread_system](https://github.com/kcenon/thread_system)에 대한 전이 의존성으로 인해 최소 컴파일러 버전이 GCC 13+ / Clang 17+입니다.

### 빌드 도구 및 의존성

| 의존성 | 버전 | 필수 | 설명 |
|--------|------|------|------|
| CMake | 3.20+ | 예 | 빌드 시스템 |
| [common_system](https://github.com/kcenon/common_system) | latest | 예 | 공통 인터페이스 (IMonitor, Result<T>) |
| [thread_system](https://github.com/kcenon/thread_system) | latest | 예 | 스레드 풀 및 비동기 연산 |
| [logger_system](https://github.com/kcenon/logger_system) | latest | 아니오 | 로깅 기능 |

---

## 빠른 시작

```cpp
#include <kcenon/monitoring/core/performance_monitor.h>
#include <kcenon/monitoring/tracing/distributed_tracer.h>

int main() {
    // 성능 모니터 생성
    auto monitor = std::make_shared<kcenon::monitoring::performance_monitor>();

    // 메트릭 기록
    monitor->record_metric("request_count", 1.0);
    monitor->record_metric("response_time_ms", 42.5);

    // 분산 추적
    auto tracer = std::make_shared<kcenon::monitoring::distributed_tracer>();
    auto span = tracer->start_span("handle_request");
    // ... 작업 수행 ...
    span->end();

    return 0;
}
```

---

## 설치

### 의존성과 함께 빌드

```bash
# 의존성 클론 (형제 디렉토리에 클론)
git clone https://github.com/kcenon/common_system.git
git clone https://github.com/kcenon/thread_system.git
git clone https://github.com/kcenon/monitoring_system.git

cd monitoring_system
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### vcpkg를 통한 설치

```bash
vcpkg install kcenon-monitoring-system

# 로깅 통합 포함
vcpkg install kcenon-monitoring-system[logging]

# gRPC 전송 포함
vcpkg install kcenon-monitoring-system[grpc]
```

### 의존성 흐름

```
monitoring_system
+-- common_system (필수)
+-- thread_system (필수)
|   +-- common_system
+-- logger_system (선택)
    +-- common_system
```

---

## 아키텍처

### 모듈 구조

```
include/kcenon/monitoring/
  core/          - performance_monitor, central_collector, event_bus, error_codes
  interfaces/    - 순수 가상: metric_collector, metric_source, observable
  collectors/    - ~16개 수집기 (system, process, network, battery, GPU 등)
  factory/       - metric_factory (싱글톤), builtin_collectors
  tracing/       - distributed_tracer, trace_context (W3C 스타일)
  context/       - 스레드 로컬 컨텍스트 전파 (<50ns)
  alert/         - 알림 타입, 트리거, 파이프라인, 노티파이어
  health/        - 헬스 모니터, 의존성 그래프, 복합 헬스 체크
  reliability/   - circuit_breaker, error_boundary, retry_policy
  exporters/     - OTLP, Jaeger, Zipkin (HTTP/gRPC/UDP 전송)
  plugins/       - 플러그인 API, collector_plugin, plugin_loader
  storage/       - 스토리지 백엔드 (메모리, 파일, 시계열)
  optimization/  - Lock-free 큐, 메모리 풀, SIMD 집계기
```

---

## 핵심 개념

### Performance Monitor

`common_system::IMonitor` 인터페이스를 구현하는 핵심 모니터링 컴포넌트입니다. 메트릭 기록, 타이머, 카운터를 제공합니다.

### 분산 추적 (Distributed Tracing)

W3C 스타일의 분산 추적을 제공합니다:
- `trace_span`으로 trace_id/span_id/parent 관계 관리
- 스레드 로컬 컨텍스트 전파 (<50ns)
- Jaeger, Zipkin, OTLP로 내보내기 지원

### Collector Factory

런타임 DI 기반 수집기 팩토리 패턴입니다:

```cpp
auto& factory = kcenon::monitoring::metric_factory::instance();
factory.register_collector<system_resource_collector>("system");
auto collector = factory.create("system", config);
```

### 헬스 모니터링

의존성 그래프 기반 헬스 체크 시스템으로, 복합 헬스 상태를 모니터링합니다.

### 신뢰성 패턴 (Reliability Patterns)

- **서킷 브레이커**: 장애 격리 및 자동 복구
- **에러 바운더리**: 오류 전파 차단
- **재시도 정책**: 설정 가능한 재시도 전략
- **우아한 저하**: 부분 장애 시 기능 축소

---

## API 개요

| API | 헤더 | 설명 |
|-----|------|------|
| `performance_monitor` | `core/performance_monitor.h` | 핵심 모니터링 |
| `distributed_tracer` | `tracing/distributed_tracer.h` | 분산 추적 |
| `central_collector` | `core/central_collector.h` | 메트릭 집계 |
| `metric_factory` | `factory/metric_factory.h` | 수집기 팩토리 |
| `circuit_breaker` | `reliability/circuit_breaker.h` | 서킷 브레이커 |
| `error_boundary` | `reliability/error_boundary.h` | 에러 바운더리 |
| `health_monitor` | `health/health_monitor.h` | 헬스 모니터링 |

---

## 예제

| 예제 | 난이도 | 설명 |
|------|--------|------|
| basic_monitoring | 초급 | 기본 메트릭 수집 |
| distributed_tracing | 중급 | 분산 추적 설정 |
| reliability_patterns | 고급 | 서킷 브레이커 및 신뢰성 패턴 |
| custom_collector | 고급 | 커스텀 수집기 플러그인 |

---

## 성능

| 메트릭 | 값 | 비고 |
|--------|------|------|
| **메트릭 연산** | 10M+ ops/s | 핵심 메트릭 기록 |
| **컨텍스트 전파** | <50 ns | 스레드 로컬 |
| **SIMD 집계** | 고속 | AVX2/NEON |

### 품질 메트릭

- 37/37 테스트 통과 (100% 통과율)
- 모든 CI/CD 파이프라인 정상
- ThreadSanitizer / AddressSanitizer 클린
- 다중 플랫폼 지원

---

## 생태계 통합

### 의존성 계층

```
common_system    (Tier 0) [필수] -- IMonitor, ILogger, Result<T>
thread_system    (Tier 1) [필수] -- 스레드 풀, 비동기 연산
logger_system    (Tier 2) [선택] -- 런타임 DI를 통한 로깅
network_system   (Tier 4) [선택] -- 내보내기 HTTP 전송
```

### 통합 프로젝트

| 프로젝트 | monitoring_system 역할 |
|----------|----------------------|
| [common_system](https://github.com/kcenon/common_system) | 필수 의존성 |
| [thread_system](https://github.com/kcenon/thread_system) | 필수 의존성 |
| [database_system](https://github.com/kcenon/database_system) | 메트릭 수집 |
| [network_system](https://github.com/kcenon/network_system) | 내보내기 전송 |

### 플랫폼 지원

| 플랫폼 | 컴파일러 | 상태 |
|--------|----------|------|
| **Linux** | GCC 13+, Clang 17+ | 완전 지원 |
| **macOS** | Apple Clang 14+ | 완전 지원 |
| **Windows** | MSVC 2022+ | 완전 지원 |

---

## 기여하기

기여를 환영합니다! 자세한 내용은 [기여 가이드](docs/contributing/CONTRIBUTING.md)를 참조하세요.

1. 리포지토리 포크
2. 기능 브랜치 생성
3. 테스트와 함께 변경 사항 작성
4. 로컬에서 테스트 실행
5. Pull Request 열기

---

## 라이선스

이 프로젝트는 BSD 3-Clause 라이선스에 따라 배포됩니다 - 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하세요.

---

<p align="center">
  Made with ❤️ by 🍀☀🌕🌥 🌊
</p>
