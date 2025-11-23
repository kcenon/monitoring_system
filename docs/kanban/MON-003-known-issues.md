# MON-003: Write KNOWN_ISSUES.md Document

**Priority**: HIGH
**Est. Duration**: 4h
**Status**: TODO
**Dependencies**: None

---

## Summary

프로젝트의 현재 제한사항, stub 구현, 알려진 버그를 문서화하는 KNOWN_ISSUES.md 작성.

## Motivation

- `circuit_breaker.h`에 `[[deprecated]]` 어트리뷰트가 있지만 참조할 문서 없음
- 사용자가 어떤 기능이 production-ready인지 알 수 없음
- stub 구현들이 산재해 있어 추적 어려움

## Document Structure

```markdown
# Known Issues and Limitations

## Version: 2.0.0

### STUB Implementations (Not Production Ready)

| Component | File | Status | Notes |
|-----------|------|--------|-------|
| CircuitBreaker | reliability/circuit_breaker.h | STUB | MON-001 |
| Jaeger Exporter | exporters/trace_exporters.h | STUB | send_* 미구현 |
| Zipkin Exporter | exporters/trace_exporters.h | STUB | send_* 미구현 |
| Linux Metrics | platform/linux_metrics.cpp | STUB | - |
| Windows Metrics | platform/windows_metrics.cpp | STUB | - |

### Platform Support

| Platform | Status |
|----------|--------|
| macOS | Supported |
| Linux | Partial (metrics stub) |
| Windows | Partial (metrics stub) |

### Known Bugs
- [링크 또는 설명]

### Test Coverage
- Current: ~87%
- 24/29 tests disabled

### Breaking Changes from v1.x
- [해당사항 있으면 기술]
```

## Files to Create

- `docs/KNOWN_ISSUES.md`

## Acceptance Criteria

1. 모든 STUB 구현이 문서화됨
2. 플랫폼 지원 현황 명시
3. 테스트 커버리지 현황 포함
4. 버전별 변경사항 기록
