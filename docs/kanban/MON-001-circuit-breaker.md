# MON-001: Complete CircuitBreaker Implementation

**Priority**: HIGH
**Est. Duration**: 8h
**Status**: TODO
**Dependencies**: None

---

## Summary

`circuit_breaker.h`의 현재 구현은 **STUB** 상태로 `[[deprecated]]` 처리되어 있음. 실제 상태 전환 로직이 없어 프로덕션 사용 불가.

## Current State

```cpp
template<typename T = void>
class [[deprecated("STUB implementation. Do not use in production.")]] circuit_breaker {
    // execute()가 항상 main function 실행 - 상태 전환 없음
};
```

**문제점:**
- `get_state()`는 항상 `closed` 반환
- failure threshold 도달해도 `open` 상태로 전환되지 않음
- `half_open` 상태 로직 미구현
- 실패 횟수 카운팅 불완전

## Requirements

### 1. State Machine 구현
```
closed ─[failures >= threshold]─> open
open ─[timeout elapsed]─> half_open
half_open ─[success]─> closed
half_open ─[failure]─> open
```

### 2. 핵심 기능
- [ ] failure count 정확히 추적
- [ ] threshold 도달 시 `open` 상태 전환
- [ ] `open` 상태에서 요청 거부 (fallback 실행)
- [ ] reset_timeout 후 `half_open` 전환
- [ ] `half_open`에서 성공 시 `closed` 복귀
- [ ] Thread-safe state transitions

### 3. 메트릭스
- [ ] `rejected_calls` 정확히 카운팅
- [ ] `state_transitions` 추적
- [ ] 상태 변경 이벤트 발생

## Acceptance Criteria

1. `[[deprecated]]` 어트리뷰트 제거
2. 모든 상태 전환 동작 확인
3. Thread-safety 보장 (TSAN 통과)
4. 단위 테스트 커버리지 90%+

## Files to Modify

- `include/kcenon/monitoring/reliability/circuit_breaker.h`
- `tests/test_fault_tolerance.cpp` (활성화 후 테스트 추가)

## Technical Notes

- `std::atomic`으로 상태 관리
- `std::chrono::steady_clock`으로 timeout 측정
- Lock-free 구현 권장
