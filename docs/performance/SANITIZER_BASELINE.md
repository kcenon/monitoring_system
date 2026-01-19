# monitoring_system Sanitizer Baseline

> **Language:** **English** | [한국어](SANITIZER_BASELINE.kr.md)

**Phase**: 0 - Foundation and Tooling
**Task**: 0.1 - CI/CD Pipeline Enhancement
**Date Created**: 2025-10-07
**Status**: Baseline Establishment

---

## Executive Summary

This document records the baseline sanitizer warnings for monitoring_system. During Phase 0, sanitizer tests are configured and running, but warnings are allowed to establish a baseline. Phase 1 will address thread safety issues identified by sanitizers.

---

## Sanitizer Configuration

### Enabled Sanitizers

1. **ThreadSanitizer (TSan)**
   - Detects data races and thread safety issues
   - Flags: `-fsanitize=thread`
   - Environment: `TSAN_OPTIONS=halt_on_error=0 second_deadlock_stack=1`

2. **AddressSanitizer (ASan)**
   - Detects memory errors (use-after-free, buffer overflow, etc.)
   - Flags: `-fsanitize=address`
   - Environment: `ASAN_OPTIONS=halt_on_error=0 detect_leaks=1`

3. **UndefinedBehaviorSanitizer (UBSan)**
   - Detects undefined behavior
   - Flags: `-fsanitize=undefined`
   - Environment: `UBSAN_OPTIONS=halt_on_error=0 print_stacktrace=1`

---

## Baseline Warnings

### ThreadSanitizer Warnings

**Status**: ⏳ To be measured

| Component | Warning Count | Severity | Phase 1 Target |
|-----------|---------------|----------|----------------|
| performance_monitor | TBD | TBD | 0 warnings |
| event_bus | TBD | TBD | 0 warnings |
| metric_collector | TBD | TBD | 0 warnings |
| monitoring_manager | TBD | TBD | 0 warnings |

**Common Issues Expected**:
- Concurrent access to metric storage
- Event bus publish/subscribe race conditions
- Collector data structure access without synchronization

### AddressSanitizer Warnings

**Status**: ⏳ To be measured

| Component | Warning Count | Severity | Phase 2 Target |
|-----------|---------------|----------|----------------|
| Memory allocations | TBD | TBD | 0 warnings |
| Metric storage | TBD | TBD | 0 warnings |
| Event handlers | TBD | TBD | 0 warnings |

### UndefinedBehaviorSanitizer Warnings

**Status**: ⏳ To be measured

| Component | Warning Count | Severity | Phase 1 Target |
|-----------|---------------|----------|----------------|
| Type conversions | TBD | TBD | 0 warnings |
| Arithmetic operations | TBD | TBD | 0 warnings |

---

## How to Run Sanitizers Locally

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

## CI/CD Integration

Sanitizers run automatically on:
- Every push to main/phase-* branches
- Every pull request
- Manual workflow dispatch

See `.github/workflows/sanitizers.yml` for configuration.

---

## Performance Test Adjustments for Sanitizers

Sanitizers (especially AddressSanitizer) add significant runtime overhead, typically 2-10x slowdown. To prevent false test failures, timing-sensitive tests automatically adjust their thresholds when running under sanitizers.

### Implemented Adjustments

| Test | Normal Threshold | Sanitizer Threshold | Adjustment Factor |
|------|-----------------|---------------------|-------------------|
| BurstLoadTest (avg latency) | 5,000ms | 10,000ms | 2.0x (ASAN) |
| BurstLoadTest (max latency) | 10,000ms | 20,000ms | 2.0x (ASAN) |
| LockfreeQueueConcurrentAccess (push success rate) | 60% | 40% | 0.67x (TSAN) |

### Detection Mechanism

Tests detect sanitizer environments using compiler-specific macros:

```cpp
// GCC uses __SANITIZE_ADDRESS__, Clang uses __has_feature
#ifdef __SANITIZE_ADDRESS__
    #define RUNNING_WITH_ASAN 1
#elif defined(__has_feature)
    #if __has_feature(address_sanitizer)
        #define RUNNING_WITH_ASAN 1
    #else
        #define RUNNING_WITH_ASAN 0
    #endif
#else
    #define RUNNING_WITH_ASAN 0
#endif

constexpr double SANITIZER_OVERHEAD_FACTOR = RUNNING_WITH_ASAN ? 2.0 : 1.0;
```

This allows tests to dynamically adjust thresholds while maintaining strict requirements for non-sanitizer builds.

---

## Phase 1 Action Items

Based on baseline measurements, Phase 1 will address:

1. **Thread Safety** (High Priority)
   - Add mutexes to protect metric storage
   - Implement thread-safe event bus operations
   - Add synchronization to collector data access

2. **Memory Management** (Medium Priority)
   - Fix memory leaks in event handler cleanup
   - Ensure proper metric lifecycle management
   - Add exception-safe resource management

3. **Undefined Behavior** (Low Priority)
   - Fix any type conversion issues
   - Address arithmetic overflow concerns

---

**Last Updated**: 2025-10-07
**Next Review**: After first sanitizer CI run
**Owner**: Development Team
