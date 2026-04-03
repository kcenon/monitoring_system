---
doc_id: "MON-PROJ-003"
doc_title: "Monitoring System - 프로젝트 구조"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "monitoring_system"
category: "PROJ"
---

# Monitoring System - 프로젝트 구조

> **SSOT**: This document is the single source of truth for **Monitoring System - 프로젝트 구조**.

**언어:** [English](PROJECT_STRUCTURE.md) | **한국어**

**버전**: 0.3.0
**최종 업데이트**: 2026-01-22

> **참고**: Issue #389에서 collector 통합 리팩토링이 완료되었습니다. 최신 구조는 [영어 버전](PROJECT_STRUCTURE.md)을 참조하세요.

---

## 목차

- [개요](#개요)
- [디렉토리 구조](#디렉토리-구조)
- [코어 모듈](#코어-모듈)
- [파일 설명](#파일-설명)
- [빌드 아티팩트](#빌드-아티팩트)
- [모듈 의존성](#모듈-의존성)

---

## 개요

monitoring system은 명확한 관심사 분리와 모듈형 인터페이스 기반 아키텍처를 따릅니다. 이 문서는 프로젝트 구조와 파일 구성에 대한 포괄적인 가이드를 제공합니다.

---

## 디렉토리 구조

```
monitoring_system/
├── 📁 include/kcenon/monitoring/   # 공개 헤더 (API 서피스)
│   ├── 📁 core/                    # 코어 모니터링 컴포넌트
│   │   ├── performance_monitor.h   # 성능 메트릭 수집
│   │   ├── result_types.h          # 오류 처리 타입
│   │   ├── di_container.h          # 의존성 주입 컨테이너
│   │   └── thread_context.h        # 스레드 로컬 컨텍스트 추적
│   ├── 📁 interfaces/              # 추상 인터페이스
│   │   ├── monitorable_interface.h # 모니터링 추상화
│   │   ├── storage_interface.h     # 스토리지 백엔드 추상화
│   │   ├── tracer_interface.h      # 분산 추적 추상화
│   │   └── health_check_interface.h # 헬스 체크 추상화
│   ├── 📁 tracing/                 # 분산 추적 컴포넌트
│   │   ├── distributed_tracer.h    # 추적 관리 및 조정
│   │   ├── span.h                  # Span 연산 및 라이프사이클
│   │   ├── trace_context.h         # 컨텍스트 전파 메커니즘
│   │   └── trace_exporter.h        # 추적 내보내기 및 배칭
│   ├── 📁 health/                  # 헬스 모니터링 컴포넌트
│   │   ├── health_monitor.h        # 헬스 검증 프레임워크
│   │   ├── health_check.h          # 헬스 체크 정의
│   │   ├── circuit_breaker.h       # 서킷 브레이커 패턴
│   │   └── reliability_patterns.h  # 재시도 및 폴백 패턴
│   ├── 📁 storage/                 # 스토리지 백엔드 구현
│   │   ├── memory_storage.h        # 인메모리 스토리지 백엔드
│   │   ├── file_storage.h          # 파일 기반 영속 스토리지
│   │   └── time_series_storage.h   # 시계열 최적화 스토리지
│   └── 📁 config/                  # 설정 관리
│       ├── monitoring_config.h     # 설정 구조체
│       └── config_validator.h      # 설정 검증
├── 📁 src/                         # 구현 파일
│   ├── 📁 core/                    # 코어 구현
│   │   ├── performance_monitor.cpp
│   │   ├── result_types.cpp
│   │   ├── di_container.cpp
│   │   └── thread_context.cpp
│   ├── 📁 tracing/                 # 추적 구현
│   │   ├── distributed_tracer.cpp
│   │   ├── span.cpp
│   │   ├── trace_context.cpp
│   │   └── trace_exporter.cpp
│   ├── 📁 health/                  # 헬스 구현
│   │   ├── health_monitor.cpp
│   │   ├── health_check.cpp
│   │   ├── circuit_breaker.cpp
│   │   └── reliability_patterns.cpp
│   ├── 📁 storage/                 # 스토리지 구현
│   │   ├── memory_storage.cpp
│   │   ├── file_storage.cpp
│   │   └── time_series_storage.cpp
│   └── 📁 config/                  # 설정 구현
│       ├── monitoring_config.cpp
│       └── config_validator.cpp
├── 📁 examples/                    # 예제 애플리케이션
│   ├── 📁 basic_monitoring_example/
│   │   ├── main.cpp
│   │   ├── README.md
│   │   └── CMakeLists.txt
│   ├── 📁 distributed_tracing_example/
│   │   ├── main.cpp
│   │   ├── README.md
│   │   └── CMakeLists.txt
│   ├── 📁 health_reliability_example/
│   │   ├── main.cpp
│   │   ├── README.md
│   │   └── CMakeLists.txt
│   └── 📁 integration_examples/
│       ├── with_thread_system.cpp
│       ├── with_logger_system.cpp
│       ├── README.md
│       └── CMakeLists.txt
├── 📁 tests/                       # 모든 테스트 스위트
│   ├── 📁 unit/                    # 유닛 테스트
│   │   ├── test_result_types.cpp
│   │   ├── test_di_container.cpp
│   │   ├── test_performance_monitor.cpp
│   │   ├── test_tracer.cpp
│   │   ├── test_health_monitor.cpp
│   │   └── test_storage.cpp
│   ├── 📁 integration/             # 통합 테스트
│   │   ├── test_monitoring_integration.cpp
│   │   ├── test_thread_system_integration.cpp
│   │   └── test_logger_integration.cpp
│   ├── 📁 benchmarks/              # 성능 벤치마크
│   │   ├── bench_metrics.cpp
│   │   ├── bench_tracing.cpp
│   │   ├── bench_health.cpp
│   │   └── bench_storage.cpp
│   └── CMakeLists.txt
├── 📁 docs/                        # 문서
│   ├── 📁 guides/                  # 사용자 가이드
│   │   ├── USER_GUIDE.md
│   │   ├── INTEGRATION.md
│   │   ├── BEST_PRACTICES.md
│   │   ├── TROUBLESHOOTING.md
│   │   ├── FAQ.md
│   │   └── MIGRATION_GUIDE.md
│   ├── 📁 advanced/                # 고급 주제
│   │   ├── CUSTOM_STORAGE.md
│   │   ├── CUSTOM_METRICS.md
│   │   └── PERFORMANCE_TUNING.md
│   ├── 📁 performance/             # 성능 문서
│   │   ├── BASELINE.md
│   │   └── BENCHMARKS.md
│   ├── 📁 contributing/            # 기여 가이드라인
│   │   ├── CONTRIBUTING.md
│   │   ├── CODE_STYLE.md
│   │   └── DEVELOPMENT_SETUP.md
│   ├── 01-ARCHITECTURE.md
│   ├── 02-API_REFERENCE.md
│   ├── FEATURES.md
│   ├── BENCHMARKS.md
│   ├── PROJECT_STRUCTURE.md
│   ├── PRODUCTION_QUALITY.md
│   ├── CHANGELOG.md
│   └── README.md
├── 📁 cmake/                       # CMake 모듈
│   ├── CompilerWarnings.cmake
│   ├── Sanitizers.cmake
│   ├── StaticAnalysis.cmake
│   └── Dependencies.cmake
├── 📁 .github/                     # GitHub 설정
│   ├── 📁 workflows/               # CI/CD 워크플로우
│   │   ├── ci.yml
│   │   ├── coverage.yml
│   │   ├── static-analysis.yml
│   │   └── build-doxygen.yaml
│   └── 📁 ISSUE_TEMPLATE/
│       ├── bug_report.md
│       └── feature_request.md
├── 📄 CMakeLists.txt
├── 📄 vcpkg.json
├── 📄 .clang-format
├── 📄 .clang-tidy
├── 📄 .gitignore
├── 📄 LICENSE
├── 📄 README.md
├── 📄 README.kr.md
└── 📄 BASELINE.md
```

---

## 코어 모듈

### Core 모듈 (`include/kcenon/monitoring/core/`)

**목적**: 기본 모니터링 기능 및 인프라

**주요 컴포넌트**:

| 파일 | 목적 | 주요 클래스/함수 | 의존성 |
|------|---------|----------------------|--------------|
| `performance_monitor.h` | 성능 메트릭 수집 | `performance_monitor`, `metrics_snapshot` | result_types.h |
| `result_types.h` | 오류 처리 타입 | `result<T>`, `monitoring_error` | 없음 |
| `di_container.h` | 의존성 주입 | `di_container`, 서비스 등록 | result_types.h |
| `thread_context.h` | 스레드 로컬 컨텍스트 | `thread_context`, 컨텍스트 전파 | 없음 |

### Interfaces 모듈 (`include/kcenon/monitoring/interfaces/`)

**목적**: 확장성과 테스트 용이성을 위한 추상 인터페이스

**주요 인터페이스**:

| 파일 | 목적 | 주요 메서드 | 구현체 |
|------|---------|-------------|-----------------|
| `monitorable_interface.h` | 모니터링 기능 | `configure()`, `start()`, `stop()`, `collect_now()` | performance_monitor |
| `storage_interface.h` | 스토리지 백엔드 | `store()`, `retrieve()`, `flush()` | memory_storage, file_storage, time_series_storage |
| `tracer_interface.h` | 분산 추적 | `start_span()`, `finish_span()`, `export_traces()` | distributed_tracer |
| `health_check_interface.h` | 헬스 검증 | `check()`, `get_status()` | functional_health_check, 커스텀 체크 |

### Tracing 모듈 (`include/kcenon/monitoring/tracing/`)

**목적**: 분산 요청 추적 및 컨텍스트 전파

**주요 컴포넌트**:

| 파일 | 목적 | 주요 클래스/함수 | 스레드 안전 |
|------|---------|----------------------|-------------|
| `distributed_tracer.h` | 추적 조정 | `distributed_tracer`, `global_tracer()` | ✅ 예 |
| `span.h` | Span 라이프사이클 | `span`, 태그 관리 | ✅ 예 |
| `trace_context.h` | 컨텍스트 전파 | `trace_context`, `get_current_context()` | ✅ 예 (스레드 로컬) |
| `trace_exporter.h` | 추적 내보내기 | `trace_exporter`, 배치 처리 | ✅ 예 |

### Health 모듈 (`include/kcenon/monitoring/health/`)

**목적**: 헬스 모니터링 및 신뢰성 패턴

**주요 컴포넌트**:

| 파일 | 목적 | 주요 클래스/함수 | 사용 사례 |
|------|---------|----------------------|----------|
| `health_monitor.h` | 헬스 검증 | `health_monitor`, 체크 등록 | 서비스 헬스 |
| `health_check.h` | 헬스 체크 정의 | `health_check_result`, 상태 타입 | 커스텀 체크 |
| `circuit_breaker.h` | 서킷 브레이커 패턴 | `circuit_breaker`, 상태 관리 | 장애 허용 |
| `reliability_patterns.h` | 재시도/폴백 | `retry_policy`, `error_boundary` | 복원력 |

### Storage 모듈 (`include/kcenon/monitoring/storage/`)

**목적**: 메트릭 및 추적 스토리지 백엔드

**주요 컴포넌트**:

| 파일 | 목적 | 성능 | 영속성 | 최적 용도 |
|------|---------|-------------|-------------|----------|
| `memory_storage.h` | 인메모리 스토리지 | 8.5M ops/sec | 아니오 | 실시간, 짧은 보존 |
| `file_storage.h` | 파일 기반 스토리지 | 2.1M ops/sec | 예 | 장기 보존, 감사 |
| `time_series_storage.h` | 시계열 최적화 | 1.8M ops/sec | 예 | 히스토리 분석, 압축 |

---

## 파일 설명

### 코어 구현 파일

#### `src/core/performance_monitor.cpp`

**목적**: 실시간 성능 메트릭 수집

**주요 기능**:
- 원자적 카운터 연산 (10M+ ops/sec)
- 게이지 추적
- 설정 가능한 버킷을 가진 히스토그램 기록
- RAII를 사용한 타이머 유틸리티
- 스레드 안전 메트릭 수집

**공개 API**:
```cpp
class performance_monitor {
    auto enable_collection(bool enabled) -> void;
    auto collect() -> result<metrics_snapshot>;
    auto increment_counter(const std::string& name) -> void;
    auto set_gauge(const std::string& name, double value) -> void;
    auto record_histogram(const std::string& name, double value) -> void;
    auto start_timer(const std::string& name) -> scoped_timer;
};
```

#### `src/core/di_container.cpp`

**목적**: 의존성 주입 및 라이프사이클 관리

**주요 기능**:
- 싱글톤 등록
- 트랜지언트 등록
- 팩토리 등록
- 자동 의존성 해결
- 스레드 안전 서비스 접근

**공개 API**:
```cpp
class di_container {
    template<typename Interface, typename Implementation>
    auto register_singleton() -> result_void;

    template<typename Interface>
    auto resolve() -> result<std::shared_ptr<Interface>>;
};
```

### 추적 구현 파일

#### `src/tracing/distributed_tracer.cpp`

**목적**: 분산 추적 관리

**주요 기능**:
- Span 라이프사이클 관리 (2.5M spans/sec)
- 컨텍스트 전파 (<50ns 오버헤드)
- 추적 내보내기 및 배칭
- 스레드 안전 연산

**구현 세부사항**:
- 원자적 연산을 사용한 락프리 span 생성
- 스레드 로컬 컨텍스트 저장소
- 배치 내보내기 최적화 (최적 배치 크기: 100-500)

### 헬스 구현 파일

#### `src/health/circuit_breaker.cpp`

**목적**: 서킷 브레이커 패턴 구현

**주요 기능**:
- 상태 관리 (Closed, Open, Half-Open)
- 실패 임계값 추적
- 자동 복구 테스트
- 통계 수집

**성능**:
- Closed 상태: 12M ops/sec
- Open 상태: 25M ops/sec (빠른 실패)
- Half-open 상태: 8M ops/sec

---

## 빌드 아티팩트

### 빌드 디렉토리 구조

```
build/
├── 📁 lib/                         # 라이브러리
│   └── libmonitoring_system.a      # 정적 라이브러리
├── 📁 bin/                         # 실행 파일
│   ├── basic_monitoring_example
│   ├── distributed_tracing_example
│   └── health_reliability_example
├── 📁 tests/                       # 테스트 실행 파일
│   ├── monitoring_system_tests
│   ├── integration_tests
│   └── benchmarks
└── 📁 docs/                        # 생성된 문서
    └── 📁 html/
        └── index.html
```

### CMake 타겟

| 타겟 | 유형 | 출력 | 목적 |
|--------|------|--------|---------|
| `monitoring_system` | 라이브러리 | `libmonitoring_system.a` | 메인 라이브러리 |
| `monitoring_system_tests` | 실행 파일 | `monitoring_system_tests` | 유닛 테스트 |
| `integration_tests` | 실행 파일 | `integration_tests` | 통합 테스트 |
| `benchmarks` | 실행 파일 | `benchmarks` | 성능 테스트 |
| `basic_monitoring_example` | 실행 파일 | `basic_monitoring_example` | 예제 앱 |
| `docs` | 커스텀 | `docs/html/` | 문서 |

---

## 모듈 의존성

### 내부 의존성

```
┌─────────────────────────────────────────────────────────────┐
│                     monitoring_system                       │
└─────────────────────────────────────────────────────────────┘
                           │
           ┌───────────────┼───────────────┐
           │               │               │
           ▼               ▼               ▼
    ┌──────────┐    ┌──────────┐    ┌──────────┐
    │   core   │    │ tracing  │    │  health  │
    └────┬─────┘    └────┬─────┘    └────┬─────┘
         │               │               │
         │          ┌────┴────┐          │
         │          │         │          │
         ▼          ▼         ▼          ▼
    ┌────────────────────────────────────────┐
    │           interfaces                   │
    └────────────────────────────────────────┘
                     │
                     ▼
            ┌─────────────────┐
            │    storage      │
            └─────────────────┘
```

### 모듈 의존성 매트릭스

| 모듈 | 의존 대상 | 사용처 |
|--------|-----------|---------|
| **config** | 없음 | core, tracing, health, storage |
| **interfaces** | config | core, tracing, health, storage |
| **core** | interfaces | tracing, health |
| **tracing** | core, interfaces | health |
| **health** | core, interfaces | N/A |
| **storage** | interfaces | core, tracing, health |

### 외부 의존성

| 의존성 | 버전 | 목적 | 필수 |
|------------|---------|---------|----------|
| **common_system** | 최신 | 코어 인터페이스 (IMonitor, ILogger, Result<T>) | 예 |
| **thread_system** | 최신 | 스레딩 프리미티브, monitoring_interface | 예 |
| **logger_system** | 최신 | 로깅 기능 | 아니오 (선택적) |
| **Google Test** | 1.12+ | 유닛 테스팅 프레임워크 | 아니오 (테스트만) |
| **Google Benchmark** | 1.7+ | 성능 벤치마킹 | 아니오 (벤치마크만) |
| **Catch2** | 3.0+ | 테스팅 프레임워크 (마이그레이션 중) | 아니오 (테스트만) |

### 컴파일 순서

1. **config** - 의존성 없음
2. **interfaces** - config에 의존
3. **core** - interfaces에 의존
4. **storage** - interfaces에 의존
5. **tracing** - core, interfaces에 의존
6. **health** - core, interfaces에 의존

**총 빌드 시간**: ~12초 (Release 모드, Apple M1)

---

## 테스트 구성

### 유닛 테스트 (`tests/unit/`)

| 테스트 파일 | 테스트 수 | 커버리지 | 목적 |
|-----------|-------|----------|---------|
| `test_result_types.cpp` | 13 | Result<T> 패턴 | 오류 처리 검증 |
| `test_di_container.cpp` | 9 | DI 컨테이너 | 서비스 등록/해결 |
| `test_performance_monitor.cpp` | 8 | 성능 모니터 | 메트릭 수집 |
| `test_tracer.cpp` | 5 | 분산 추적기 | Span 라이프사이클 |
| `test_health_monitor.cpp` | 4 | 헬스 모니터 | 헬스 체크 |
| `test_storage.cpp` | 6 | 스토리지 백엔드 | 데이터 영속성 |

**총계**: 37개 테스트, 100% 통과율

### 통합 테스트 (`tests/integration/`)

| 테스트 파일 | 테스트 수 | 목적 |
|-----------|-------|---------|
| `test_monitoring_integration.cpp` | 전체 스택 | 엔드투엔드 모니터링 |
| `test_thread_system_integration.cpp` | 스레드 통합 | Thread 시스템 호환성 |
| `test_logger_integration.cpp` | 로거 통합 | 로깅 통합 |

### 벤치마크 테스트 (`tests/benchmarks/`)

| 벤치마크 파일 | 벤치마크 수 | 목적 |
|----------------|------------|---------|
| `bench_metrics.cpp` | 카운터, 게이지, 히스토그램 | 메트릭 성능 |
| `bench_tracing.cpp` | Span 생성, 내보내기 | 추적 성능 |
| `bench_health.cpp` | 헬스 체크, 서킷 브레이커 | 헬스 모니터링 성능 |
| `bench_storage.cpp` | 스토리지 백엔드 | 스토리지 성능 |

---

## 참고 문서

- [아키텍처 가이드](01-ARCHITECTURE.md) / [아키텍처 (한국어)](ARCHITECTURE.kr.md) - 시스템 설계 및 패턴
- [API 레퍼런스](02-API_REFERENCE.md) / [API 레퍼런스 (한국어)](API_REFERENCE.kr.md) - 완전한 API 문서
- [기능](FEATURES.md) / [기능 (한국어)](FEATURES.kr.md) - 상세 기능 문서
- [벤치마크](BENCHMARKS.md) / [벤치마크 (한국어)](BENCHMARKS.kr.md) - 성능 메트릭
- [사용자 가이드](guides/USER_GUIDE.md) - 사용 예제

---

**최종 업데이트**: 2025-11-28
**버전**: 0.1.0

---

Made with ❤️ by 🍀☀🌕🌥 🌊
