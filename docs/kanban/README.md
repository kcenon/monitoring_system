# Monitoring System Kanban Board

개선 작업 추적을 위한 칸반 보드.

**Last Updated**: 2025-11-23

---

## Ticket Status

| Priority | Total | Done | In Progress | Pending |
|----------|-------|------|-------------|---------|
| 🔴 HIGH | 4 | 4 | 0 | 0 |
| 🟡 MEDIUM | 3 | 3 | 0 | 0 |
| **Total** | **7** | **7** | **0** | **0** |

---

## 🔴 HIGH Priority - Immediate Action Required

| ID | Title | Est. | Dependencies | Status |
|----|-------|------|--------------|--------|
| [MON-001](MON-001-circuit-breaker.md) | Complete CircuitBreaker Implementation | 8h | - | DONE |
| [MON-002](MON-002-test-activation.md) | Activate 24 Disabled Tests | 12h | - | DONE |
| [MON-003](MON-003-known-issues.md) | Write KNOWN_ISSUES.md Document | 4h | - | DONE |
| [MON-004](MON-004-platform-metrics.md) | Implement Linux/Windows Platform Metrics | 16h | - | DONE |

### Why HIGH Priority?

- **MON-001**: CircuitBreaker가 STUB 상태로 `[[deprecated]]` 처리됨. 프로덕션 사용 불가.
- **MON-002**: 24/29 테스트 비활성화. 코드 품질 검증 불가능.
- **MON-003**: STUB 구현들의 문서화 부재. 사용자 혼란 유발.
- **MON-004**: macOS만 지원. Linux/Windows 메트릭 미구현.

---

## 🟡 MEDIUM Priority - Important Improvements

| ID | Title | Est. | Dependencies | Status |
|----|-------|------|--------------|--------|
| [MON-005](MON-005-trace-exporters.md) | Implement Jaeger/Zipkin HTTP Transport | 12h | - | DONE |
| [MON-006](MON-006-integration-tests.md) | Complete Integration Test Suite | 10h | MON-002 | DONE |
| [MON-007](MON-007-cmake-cleanup.md) | CMake Option Cleanup | 5h | - | DONE |

---

## Execution Plan

### Week 1: Foundation
```
MON-001 (CircuitBreaker) ─────────────────────► 8h
MON-003 (KNOWN_ISSUES.md) ────► 4h
MON-007 (CMake Cleanup) ──────► 5h
```

### Week 2: Tests & Platform
```
MON-002 (Test Activation) ────────────────────► 12h
MON-004 (Platform Metrics) [Start] ───────────► 8h
```

### Week 3: Platform & Integration
```
MON-004 (Platform Metrics) [Complete] ────────► 8h
MON-005 (Trace Exporters) ────────────────────► 12h
```

### Week 4: Finalization
```
MON-006 (Integration Tests) ──────────────────► 10h
```

---

## Dependency Graph

```
                    ┌─────────────────────────────────────┐
                    │         IMMEDIATE START              │
                    │  (No Dependencies)                   │
                    │                                      │
                    │  MON-001  MON-003  MON-004  MON-007  │
                    │  MON-002  MON-005                    │
                    └──────────────┬──────────────────────┘
                                   │
                    ┌──────────────▼──────────────┐
                    │      AFTER MON-002          │
                    │                             │
                    │         MON-006             │
                    │   (Integration Tests)       │
                    └─────────────────────────────┘
```

---

## Key Metrics

| Metric | Current | Target |
|--------|---------|--------|
| Active Tests | 5/29 | 29/29 |
| STUB Components | 5 | 0 |
| Platform Support | macOS only | macOS + Linux + Windows |
| Test Coverage | ~87% | 90%+ |

---

## Archived/Deferred Tickets

기존 20개 티켓 중 아래 항목은 통합되었거나 우선순위 낮음으로 보류:

| Original ID | Reason |
|-------------|--------|
| MON-008 | Plugin System - 현재 필요성 낮음 |
| MON-009~020 | 낮은 우선순위 또는 상위 티켓에 통합 |

---

## Status Definitions

- **TODO**: 시작 전
- **IN_PROGRESS**: 작업 중
- **REVIEW**: 코드 리뷰 대기
- **DONE**: 완료

---

**Total Estimated Duration**: ~67 hours (~2 weeks, single developer)
