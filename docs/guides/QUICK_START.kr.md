# Monitoring System - 빠른 시작 가이드

> **Language:** [English](QUICK_START.md) | **한국어**

> **버전:** 0.1.0
> **최종 업데이트:** 2025-12-14
> **소요 시간:** 5분

Monitoring System을 5분 만에 시작하세요. 이 가이드에서는 설치, 기본 사용법 및 첫 메트릭 수집을 다룹니다.

---

## 목차

1. [사전 요구사항](#사전-요구사항)
2. [설치](#설치)
3. [첫 번째 모니터링 프로그램](#첫-번째-모니터링-프로그램)
4. [기본 메트릭](#기본-메트릭)
5. [다음 단계](#다음-단계)

---

## 사전 요구사항

### 필수 요구사항

| 의존성 | 버전 | 필수 | 설명 |
|--------|------|------|------|
| C++20 컴파일러 | GCC 11+ / Clang 14+ / MSVC 2022+ | 예 | C++20 기능 필요 |
| CMake | 3.20+ | 예 | 빌드 시스템 |
| [common_system](https://github.com/kcenon/common_system) | latest | 예 | 공통 인터페이스 및 Result<T> |
| [thread_system](https://github.com/kcenon/thread_system) | latest | 예 | 스레드 풀 및 비동기 작업 |
| [logger_system](https://github.com/kcenon/logger_system) | latest | 선택 | 로그 상관관계 |

### 플랫폼 지원

- Linux (Ubuntu 20.04+, Fedora 34+)
- macOS (Big Sur 11+)
- Windows (10/11 with Visual Studio 2019+)

---

## 설치

### 의존성 클론

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

### CMake FetchContent 사용 (권장)

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_app CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    monitoring_system
    GIT_REPOSITORY https://github.com/kcenon/monitoring_system.git
    GIT_TAG main
)

FetchContent_MakeAvailable(monitoring_system)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE kcenon::monitoring)
```

---

## 첫 번째 모니터링 프로그램

`main.cpp` 생성:

```cpp
#include <kcenon/monitoring/core/performance_monitor.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace monitoring_system;

int main() {
    // 1. 모니터링 컴포넌트 생성
    performance_monitor monitor("my_service");

    // 2. 메트릭 수집 활성화
    monitor.enable_collection(true);

    // 3. 메트릭 기록
    monitor.increment_counter("app_started_total");

    // 4. 활동 시뮬레이션
    for (int i = 0; i < 10; ++i) {
        // 요청 기록
        monitor.increment_counter("requests_total");

        // 타이머 측정
        auto timer = monitor.start_timer("request_duration");
        std::this_thread::sleep_for(std::chrono::milliseconds(10 + rand() % 50));
        // timer 소멸 시 자동으로 duration 기록
    }

    // 5. 메트릭 조회
    auto snapshot = monitor.collect();
    if (snapshot) {
        std::cout << "수집된 메트릭: " << snapshot.value().metrics.size() << "\n";
    }

    std::cout << "모니터링 데모 완료!\n";
    return 0;
}
```

**빌드 및 실행:**
```bash
cmake --build build
./build/bin/my_app
```

---

## 기본 메트릭

### Counter - 단조 증가 값

**용도:** 요청 수, 오류 수, 전송 바이트

```cpp
// 간단한 카운터
monitor.increment_counter("http_requests_total");

// 레이블과 함께
monitor.increment_counter("api_requests_total", {
    {"method", "POST"},
    {"status", "201"}
});
```

### Gauge - 특정 시점 값

**용도:** 메모리 사용량, 온도, 큐 크기, 활성 연결

```cpp
// 메모리 사용량
monitor.set_gauge("memory_usage_bytes", get_memory_usage());

// 활성 연결
monitor.set_gauge("active_connections", conn_pool.active_count());
```

### Histogram - 값의 분포

**용도:** 요청 지연시간, 응답 크기, 처리 시간

```cpp
// 작업 기간 측정
auto timer = monitor.start_timer("operation_duration_ms");
// ... 작업 수행 ...
// timer 소멸 시 자동 기록
```

---

## 다음 단계

### 자세히 알아보기

- **[아키텍처 가이드](../ARCHITECTURE_GUIDE.md)** - 시스템 설계 및 내부 구조
- **[API 레퍼런스](../API_REFERENCE.md)** - 전체 API 문서
- **[FAQ](FAQ.md)** - 자주 묻는 질문 (25개 이상의 Q&A)
- **[모범 사례](BEST_PRACTICES.md)** - 프로덕션 패턴
- **[문제 해결](TROUBLESHOOTING.kr.md)** - 일반적인 문제

### 예제

- **[튜토리얼](TUTORIAL.kr.md)** - 예제를 포함한 단계별 튜토리얼
- **[샘플](../../samples/)** - 예제 애플리케이션

---

## 빠른 참조

### 메트릭 타입

| 타입 | 용도 | 예시 |
|------|------|------|
| Counter | 누적 카운트 | `requests_total`, `errors_total` |
| Gauge | 특정 시점 값 | `memory_bytes`, `cpu_percent` |
| Histogram | 분포 | `request_duration_ms`, `response_size_bytes` |
| Summary | 백분위수 | `api_latency_ms` (p50, p95, p99) |

---

**축하합니다!** 빠른 시작 가이드를 완료했습니다.

이제 다음을 알게 되었습니다:
- Monitoring System 설치
- 메트릭 기록 (counter, gauge, histogram, summary)
- 분산 추적 구현
- 웹 대시보드 사용

자세한 내용은 영문 버전 [QUICK_START.md](QUICK_START.md)를 참조하세요.

---

**최종 업데이트:** 2025-12-14
**버전:** 0.1.0
