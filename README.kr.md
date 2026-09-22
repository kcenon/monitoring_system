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
- [API 안정성 (v1.0)](#api-안정성-v10)
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

**최신 상태**: 모든 CI/CD 파이프라인 정상, 1,118 테스트 케이스 (55 스위트) 통과 (100% 통과율)

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

- 1,118 테스트 케이스 (55 스위트) 통과 (100% 통과율)
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

## API 안정성 (v1.0)

v1.0.0부터 공개 API는 [Semantic Versioning](https://semver.org/)에 따라 **동결**됩니다:

- **패치 릴리스** (1.0.x): 버그 수정만, API 변경 없음.
- **마이너 릴리스** (1.x.0): 하위 호환 가능한 추가; 기존 코드는 계속 컴파일됨.
- **메이저 릴리스** (2.0.0): 호환되지 않는 변경을 위해 예약되며, 이전 마이너 시리즈에서 폐기 주기가 진행됨.

### 안정화된 공개 인터페이스

| 컴포넌트 | 헤더 | 보증 |
|----------|------|------|
| `performance_monitor` | `core/performance_monitor.h` | 안정 |
| `distributed_tracer` | `tracing/distributed_tracer.h` | 안정 |
| `central_collector` | `core/central_collector.h` | 안정 |
| `metric_factory` | `factory/metric_factory.h` | 안정 |
| `health_monitor` | `health/health_monitor.h` | 안정 |
| `circuit_breaker` | `reliability/circuit_breaker.h` | 안정 |
| `ring_buffer` | `utils/ring_buffer.h` | 안정 |
| `metric_storage` | `utils/metric_storage.h` | 안정 |
| `time_series_buffer` | `utils/time_series_buffer.h` | 안정 |

### 생성 API

공개 유틸리티 타입은 이제 잘못된 인수에 대해 예외를 던지는 대신 `Result<T>`를 반환하는 `create()` 정적 팩토리 메서드를 제공합니다. 예외를 던지는 생성자는 **폐기 예정**이며 향후 메이저 릴리스에서 제거될 예정입니다.

```cpp
// 권장 (v1.0+)
ring_buffer_config cfg;
cfg.capacity = 1024;
auto result = ring_buffer<double>::create(cfg);
if (result.is_err()) { /* 오류 처리 */ }

// 폐기 예정 — 아직 동작하지만 v2.0에서 제거 예정
ring_buffer<double> buf(cfg); // 예외 발생 가능
```

### CMake 타겟

안정적인 CMake 내보내기 타겟은 `monitoring_system::monitoring_system`입니다.

---

## 빌드 구성과 기능 선택

Monitoring System의 필수 의존성은 `common_system`과 `thread_system`입니다.
로깅 구현은 공통 인터페이스를 통한 실행 시점의 연결을 사용할 수 있으며,
`logger_system`을 빌드 시점에 연결하는 선택은 별도의 옵션으로 관리합니다.
HTTP 전송을 위한 `network_system` 통합과 gRPC 전송도 각각 구분해야 합니다.
한 전송 기능을 활성화했다고 다른 전송 방식까지 함께 검증되는 것은 아닙니다.
현재 의존성 검색과 대체 경로는 [의존성 설정](cmake/dependencies.cmake)에 있습니다.

형제 저장소를 나란히 배치하는 소스 빌드에서는 각 저장소가 어느 커밋인지
기록하는 것이 중요합니다. 작업 디렉토리의 이름만 같고 버전이 다른 경우에는
헤더와 설치 라이브러리가 서로 다른 조합으로 선택될 수 있습니다.
설치 패키지를 소비하는 경우에는 구성 로그에서 실제로 찾은 패키지 위치와
연결 대상을 확인하세요. 선택적 의존성을 찾지 못해 기능이 비활성화되었다면
그 빌드를 해당 통합 기능의 성공 사례로 기록하면 안 됩니다.

### 공통 의존성 옵션 이름

다음 CMake 이름은 기존 옵션과 같은 의존성 선택을 표현합니다.
기존 이름은 호환 별칭으로 사용할 수 있고, 서로 다른 값을 동시에 지정하면
새 이름이 우선하며 구성 단계에서 경고가 표시됩니다.

| 새 이름 | 기존 이름 | 기본 의미 |
| --- | --- | --- |
| `KCENON_WITH_COMMON_SYSTEM` | `MONITORING_WITH_COMMON_SYSTEM` | 공통 인터페이스 |
| `KCENON_WITH_THREAD_SYSTEM` | `MONITORING_WITH_THREAD_SYSTEM` | 필수 스레드 통합 |
| `KCENON_WITH_LOGGER_SYSTEM` | `MONITORING_WITH_LOGGER_SYSTEM` | 선택적 로깅 구현 |
| `KCENON_WITH_NETWORK_SYSTEM` | `MONITORING_WITH_NETWORK_SYSTEM` | 선택적 HTTP 전송 |

이 변경은 의존성의 필수 여부나 기존 기본값을 바꾸지 않습니다.
기존 캐시에서 새 이름을 직접 지정한 경우에는 그 값이 이후 구성에서도 우선합니다.
기존 이름으로 다시 제어하려면 새 이름의 캐시 항목을 먼저 제거하세요.
gRPC, 플러그인, 테스트 및 모듈 옵션은 별도의 기능 옵션이며 이 이름 변경에 포함되지 않습니다.
상세한 호환 규칙은 [일관성 검사 안내](ci/README.md)를 참고하세요.

## 수집기와 런타임 구성

수집기는 운영체제나 프로세스에서 관측한 값을 공통 메트릭 모델로 전달합니다.
팩토리는 등록된 수집기를 생성하는 진입점이며, 특정 수집기의 사용 가능 여부는
플랫폼과 빌드 옵션, 실행 권한에 따라 달라질 수 있습니다.
애플리케이션은 원하는 수집기가 실제로 등록되었는지와 생성 결과를 확인해야 합니다.
플러그인을 사용한다면 로딩 실패, 설정 오류와 종료 순서도 함께 처리하세요.
팩토리 구현과 기본 수집기 등록은 [팩토리 헤더](include/kcenon/monitoring/factory/)에서 확인할 수 있습니다.

수집 주기와 내보내기 주기는 서로 다른 설정입니다.
수집 빈도를 높이면 더 세밀한 관측이 가능하지만 실행 비용과 저장량도 달라집니다.
느린 내보내기 대상이나 일시적인 네트워크 오류가 발생했을 때,
버퍼링, 재시도와 데이터 폐기에 어떤 정책을 적용할지는 사용 중인 exporter의
설정과 애플리케이션 요구에 맞추어 검토해야 합니다.
실행 환경의 실제 부하를 측정하지 않고 특정 지연 시간이나 처리량을 가정하지 마세요.

## 추적, 상태 확인과 신뢰성 패턴

분산 추적에서는 작업의 시작과 종료뿐 아니라 컨텍스트가 전달되는 경계를
분명하게 정해야 합니다. 다른 스레드로 작업을 넘기거나 비동기 콜백을 등록할 때
현재 컨텍스트의 수명과 부모/자식 관계를 확인하세요.
추적 데이터를 exporter에 전달하는 것과 외부 수집 서버가 이를 받아 저장하는 것은
서로 다른 검증 단계입니다. 로컬 인코딩 시험의 성공을 실제 서버와의
종단 간 통합 시험 결과로 설명하지 않아야 합니다.

상태 확인은 개별 구성 요소와 의존성의 상태를 표현하는 인터페이스를 제공합니다.
복구와 재시도 정책은 오류의 종류와 호출자의 요구에 맞게 구성해야 합니다.
회로 차단기나 재시도 도구를 사용하는 것만으로 외부 서비스의 가용성이
보장되는 것은 아닙니다. 실제 장애 상황에서 상태 전이, 시간 제한,
재시도 횟수와 애플리케이션에 전달되는 실패 결과를 확인하세요.
구체적인 인터페이스는 [상태 확인](include/kcenon/monitoring/health/)과
[신뢰성 구성 요소](include/kcenon/monitoring/reliability/)에 있습니다.

## 모듈 사용과 소비자 빌드

일반적인 헤더 사용 방식과 실험적 C++20 모듈 방식은 빌드 조건이 다릅니다.
`MONITORING_ENABLE_MODULES`를 선택하기 전에 컴파일러뿐 아니라 CMake와
의존성 스캐너가 해당 구성을 지원하는지 확인하세요.
라이브러리와 소비자가 같은 C++ 표준 모드를 사용하는지도 점검해야 합니다.
지원되지 않는 환경에서 헤더 방식으로 되돌아간 빌드는 실제 import의 검증 결과가 아닙니다.
모듈을 사용하는 예제는 생성된 모듈 대상에 연결하여 별도로 컴파일해야 합니다.

프로젝트를 `add_subdirectory`로 포함하는 경우에는 부모의 설정이
하위 의존성의 기본값에 의해 의도치 않게 바뀌지 않는지 확인하세요.
설치 후 `find_package`를 사용하는 경로에서는 공개 헤더와 내보낸 대상의
의존성이 모두 설치 결과에 포함되어야 합니다.
소스 트리에서 빌드된다는 사실만으로 설치 패키지의 소비 경로까지 검증되지는 않습니다.
[최상위 CMake 정의](CMakeLists.txt)와 [옵션 정의](cmake/options.cmake)가 현재 동작의 기준입니다.

## 예제와 변경 검증

예제는 메트릭 수집, 추적, 상태 확인과 exporter 설정을 살펴보는 출발점입니다.
선택한 옵션에 따라 생성되는 실행 파일이 달라질 수 있으므로,
예제 기능을 검증할 때는 해당 옵션을 켠 상태의 구성 로그를 함께 기록하세요.
플랫폼별 수집기는 동일한 입력이나 타이밍을 가정할 수 없으며,
테스트의 실패를 분류할 때 실행 권한과 실제 운영체제 기능도 확인해야 합니다.
[예제 디렉토리](examples/)와 [테스트](tests/)에서 실행 가능한 구성을 확인할 수 있습니다.

저장소 간 일관성 검사는 버전과 소스 구조의 편차를 찾습니다.
초기 배포 단계의 advisory 검사는 실패를 경고로 보여 주고 원래 종료 코드를 보존합니다.
워크플로의 최종 상태가 성공이어도 내부 검사가 통과했다고 단정하면 안 됩니다.
실제 원본 보고서가 현재 개발 커밋에서 통과한 뒤에만 검사를 강제하도록 전환합니다.
이러한 검사는 컴파일, 동작 시험과 실제 exporter 서버 통합 시험을 보완하는 절차입니다.

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
