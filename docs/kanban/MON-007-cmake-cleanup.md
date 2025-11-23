# MON-007: CMake Option Cleanup and Consistency

**Priority**: MEDIUM
**Est. Duration**: 5h
**Status**: TODO
**Dependencies**: None

---

## Summary

CMake 옵션 네이밍 일관성 개선 및 backward compatibility 로직 정리.

## Current Issues

### 1. 중복 옵션
```cmake
# Generic BUILD_* vs MONITORING_BUILD_*
option(MONITORING_BUILD_TESTS "Build unit tests" ON)
# + backward compatibility code for BUILD_TESTS
if(DEFINED BUILD_TESTS)
    ...
endif()
```

### 2. 불명확한 옵션
```cmake
option(BUILD_WITH_COMMON_SYSTEM "Enable common_system integration" ON)
# vs
option(MONITORING_USE_THREAD_SYSTEM "Enable thread_system integration" OFF)
# 네이밍 불일치: BUILD_WITH_ vs MONITORING_USE_
```

### 3. 과도한 Compatibility 코드
Lines 34-65의 backward compatibility 로직이 CMakeLists.txt 가독성 저하

## Proposed Changes

### Option Naming Convention
```cmake
# All options prefixed with MONITORING_
option(MONITORING_BUILD_TESTS "Build unit tests" ON)
option(MONITORING_BUILD_INTEGRATION_TESTS "Build integration tests" ON)
option(MONITORING_BUILD_EXAMPLES "Build example programs" ON)
option(MONITORING_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(MONITORING_WITH_COMMON_SYSTEM "Enable common_system" ON)
option(MONITORING_WITH_THREAD_SYSTEM "Enable thread_system" OFF)
option(MONITORING_WITH_LOGGER_SYSTEM "Enable logger_system" OFF)
option(MONITORING_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(MONITORING_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(MONITORING_ENABLE_UBSAN "Enable UBSanitizer" OFF)
option(MONITORING_ENABLE_COVERAGE "Enable coverage" OFF)
```

### Backward Compatibility Module
```cmake
# cmake/MonitoringLegacyOptions.cmake
# 모든 backward compatibility 로직을 별도 파일로 분리
include(MonitoringLegacyOptions OPTIONAL)
```

## Acceptance Criteria

1. 모든 옵션 `MONITORING_` prefix 사용
2. Backward compatibility 코드 분리 또는 제거
3. CMake configure 출력 정리
4. 기존 빌드 스크립트 동작 유지

## Files to Modify

- `CMakeLists.txt`
- `cmake/MonitoringLegacyOptions.cmake` (신규, 선택)
- `.github/workflows/ci.yml` (옵션명 변경 시)
