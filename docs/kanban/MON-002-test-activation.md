# MON-002: Activate 24 Disabled Tests

**Priority**: HIGH
**Est. Duration**: 12h
**Status**: IN_PROGRESS
**Dependencies**: None
**Progress**: CMakeLists.txt updated with detailed documentation of disabled tests and required API changes

---

## Summary

`tests/CMakeLists.txt`에서 24개 테스트 파일이 주석 처리되어 비활성화 상태. 테스트 커버리지 신뢰성 확보를 위해 활성화 필요.

## Current State

**활성화된 테스트 (5개):**
- test_result_types.cpp
- test_di_container.cpp
- test_monitorable_interface.cpp
- test_thread_context_simple.cpp
- test_lock_free_collector.cpp

**비활성화된 테스트 (24개):**
```cmake
# test_distributed_tracing.cpp
# test_performance_monitoring.cpp
# test_adaptive_monitoring.cpp
# test_health_monitoring.cpp
# test_metric_storage.cpp
# test_stream_aggregation.cpp
# test_buffering_strategies.cpp
# test_optimization.cpp
# test_fault_tolerance.cpp
# test_error_boundaries.cpp
# test_resource_management.cpp
# test_data_consistency.cpp
# test_opentelemetry_adapter.cpp
# test_trace_exporters.cpp
# test_metric_exporters.cpp
# test_storage_backends.cpp
# test_integration_e2e.cpp
# test_stress_performance.cpp
# (+ 6개 추가)
```

## Task Breakdown

### Phase 1: 컴파일 오류 수정 (4h)
각 테스트 파일이 컴파일되는지 확인하고 API 변경사항에 맞게 수정

| Test File | Est. | Notes |
|-----------|------|-------|
| test_distributed_tracing.cpp | 30m | trace_span API 확인 |
| test_performance_monitoring.cpp | 30m | - |
| test_adaptive_monitoring.cpp | 30m | - |
| test_health_monitoring.cpp | 30m | - |
| test_metric_storage.cpp | 30m | - |
| test_stream_aggregation.cpp | 30m | - |
| test_buffering_strategies.cpp | 30m | - |
| test_optimization.cpp | 30m | - |

### Phase 2: 런타임 오류 수정 (4h)
테스트 실행 후 실패하는 케이스 수정

### Phase 3: API 정렬 (4h)
- `test_integration_e2e.cpp` - "API alignment needed" 주석 해결
- 기타 stub 의존 테스트들 확인

## Acceptance Criteria

1. 모든 29개 테스트가 CMakeLists.txt에 포함
2. `ctest` 실행 시 모든 테스트 통과
3. ASAN/TSAN 빌드에서도 통과

## Files to Modify

- `tests/CMakeLists.txt`
- 각 비활성화된 테스트 파일 (필요시 수정)

## Notes

- 일부 테스트는 MON-001 (CircuitBreaker) 완료 후 활성화 가능
- stub 구현에 의존하는 테스트는 mock으로 대체 검토
