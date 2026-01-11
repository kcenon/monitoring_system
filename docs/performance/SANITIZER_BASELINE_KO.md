# monitoring_system Sanitizer 기준선

> **Language:** [English](SANITIZER_BASELINE.md) | **한국어**

**단계**: 0 - 기반 및 도구
**작업**: 0.1 - CI/CD 파이프라인 개선
**생성일**: 2025-10-07
**상태**: 기준선 설정

---

## 요약

본 문서는 monitoring_system의 기준선 sanitizer 경고를 기록합니다. Phase 0 동안 sanitizer 테스트가 구성되어 실행 중이지만, 기준선을 설정하기 위해 경고가 허용됩니다. Phase 1에서는 sanitizer가 식별한 스레드 안전성 문제를 해결할 예정입니다.

---

## Sanitizer 구성

### 활성화된 Sanitizer

1. **ThreadSanitizer (TSan)**
   - 데이터 경합 및 스레드 안전성 문제 탐지
   - Flags: `-fsanitize=thread`
   - Environment: `TSAN_OPTIONS=halt_on_error=0 second_deadlock_stack=1`

2. **AddressSanitizer (ASan)**
   - 메모리 오류 탐지 (use-after-free, buffer overflow 등)
   - Flags: `-fsanitize=address`
   - Environment: `ASAN_OPTIONS=halt_on_error=0 detect_leaks=1`

3. **UndefinedBehaviorSanitizer (UBSan)**
   - 정의되지 않은 동작 탐지
   - Flags: `-fsanitize=undefined`
   - Environment: `UBSAN_OPTIONS=halt_on_error=0 print_stacktrace=1`

---

## 기준선 경고

### ThreadSanitizer 경고

**상태**: ⏳ 측정 예정

| 컴포넌트 | 경고 수 | 심각도 | Phase 1 목표 |
|-----------|---------|--------|---------------|
| performance_monitor | TBD | TBD | 0 warnings |
| event_bus | TBD | TBD | 0 warnings |
| metric_collector | TBD | TBD | 0 warnings |
| monitoring_manager | TBD | TBD | 0 warnings |

**예상되는 일반적인 문제**:
- 메트릭 저장소에 대한 동시 접근
- 이벤트 버스 publish/subscribe 경합 조건
- 동기화 없는 수집기 데이터 구조 접근

### AddressSanitizer 경고

**상태**: ⏳ 측정 예정

| 컴포넌트 | 경고 수 | 심각도 | Phase 2 목표 |
|-----------|---------|--------|---------------|
| Memory allocations | TBD | TBD | 0 warnings |
| Metric storage | TBD | TBD | 0 warnings |
| Event handlers | TBD | TBD | 0 warnings |

### UndefinedBehaviorSanitizer 경고

**상태**: ⏳ 측정 예정

| 컴포넌트 | 경고 수 | 심각도 | Phase 1 목표 |
|-----------|---------|--------|---------------|
| Type conversions | TBD | TBD | 0 warnings |
| Arithmetic operations | TBD | TBD | 0 warnings |

---

## 로컬에서 Sanitizer 실행 방법

### ThreadSanitizer

```bash
cd monitoring_system
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" \
  -DBUILD_TESTS=ON \
  -DMONITORING_STANDALONE=ON

cmake --build build
cd build && ctest --output-on-failure
```

### AddressSanitizer

```bash
cd monitoring_system
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
  -DBUILD_TESTS=ON \
  -DMONITORING_STANDALONE=ON

cmake --build build
cd build && ctest --output-on-failure
```

### UndefinedBehaviorSanitizer

```bash
cd monitoring_system
cmake -B build -S . \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined" \
  -DBUILD_TESTS=ON \
  -DMONITORING_STANDALONE=ON

cmake --build build
cd build && ctest --output-on-failure
```

---

## CI/CD 통합

Sanitizer는 다음 상황에서 자동으로 실행됩니다:
- main/phase-* 브랜치에 대한 모든 푸시
- 모든 풀 리퀘스트
- 수동 워크플로우 디스패치

구성은 `.github/workflows/sanitizers.yml`를 참조하세요.

---

## Sanitizer용 성능 테스트 조정

Sanitizer(특히 AddressSanitizer)는 상당한 런타임 오버헤드를 추가하며, 일반적으로 2-10배의 속도 저하가 발생합니다. 거짓 양성 테스트 실패를 방지하기 위해, 타이밍에 민감한 테스트는 sanitizer 환경에서 실행될 때 임계값을 자동으로 조정합니다.

### 구현된 조정 사항

| 테스트 | 일반 임계값 | ASAN 임계값 | 조정 배수 |
|------|------------|------------|-----------|
| BurstLoadTest (평균 지연) | 5,000ms | 10,000ms | 2.0x |
| BurstLoadTest (최대 지연) | 10,000ms | 20,000ms | 2.0x |

### 감지 메커니즘

테스트는 컴파일러별 매크로를 사용하여 sanitizer 환경을 감지합니다:

```cpp
#if defined(__SANITIZE_ADDRESS__) || \
    (defined(__has_feature) && __has_feature(address_sanitizer))
    #define RUNNING_WITH_ASAN 1
#else
    #define RUNNING_WITH_ASAN 0
#endif

constexpr double SANITIZER_OVERHEAD_FACTOR = RUNNING_WITH_ASAN ? 2.0 : 1.0;
```

이를 통해 비-sanitizer 빌드에서는 엄격한 요구 사항을 유지하면서 테스트가 동적으로 임계값을 조정할 수 있습니다.

---

## Phase 1 작업 항목

기준선 측정에 기반하여 Phase 1에서는 다음을 해결할 예정입니다:

1. **스레드 안전성** (높은 우선순위)
   - 메트릭 저장소를 보호하기 위한 뮤텍스 추가
   - 스레드 안전 이벤트 버스 작업 구현
   - 수집기 데이터 접근에 동기화 추가

2. **메모리 관리** (중간 우선순위)
   - 이벤트 핸들러 정리에서 메모리 누수 수정
   - 적절한 메트릭 생명주기 관리 보장
   - 예외 안전 리소스 관리 추가

3. **정의되지 않은 동작** (낮은 우선순위)
   - 타입 변환 문제 수정
   - 산술 오버플로우 문제 해결

---

**마지막 업데이트**: 2025-10-07
**다음 검토**: 첫 번째 sanitizer CI 실행 후
**담당자**: 개발 팀

---

*Last Updated: 2025-10-20*
