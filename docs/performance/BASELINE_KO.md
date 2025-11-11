# monitoring_system 성능 기준선

> **Language:** [English](BASELINE.md) | **한국어**

**Phase**: 0 - Foundation and Tooling
**Task**: 0.2 - Baseline Performance Benchmarking
**작성일**: 2025-10-07
**상태**: 인프라 완료 - 측정 대기 중

---

## 요약

이 문서는 monitoring_system의 성능 기준선을 기록하며, 모니터링 작업이 모니터링되는 시스템에 미치는 오버헤드에 중점을 둡니다. 주요 목표는 모니터링이 < 1% 오버헤드를 추가하도록 하는 것입니다.

**기준선 측정 상태**: ⏳ 대기 중
- 인프라 완료 (벤치마크 구현됨)
- 측정 준비 완료
- CI workflow 구성 완료

---

## 목표 Metrics

### 주요 성공 기준

| 카테고리 | Metric | 목표 | 허용 범위 |
|----------|--------|--------|------------|
| Metric Collection | Collection 오버헤드 | < 1% | < 5% |
| Metric Collection | Recording 지연 시간 | < 100ns | < 1μs |
| Event Bus | Publication 지연 시간 | < 10μs | < 100μs |
| Event Bus | 처리량 | > 100k events/s | > 50k events/s |
| System Collector | Collection 지연 시간 | < 1ms | < 10ms |
| System Collector | CPU 오버헤드 | < 0.5% | < 2% |

---

## 기준선 Metrics

### 1. Metric Collection 성능

| 테스트 케이스 | 목표 | 측정값 | 상태 |
|-----------|--------|----------|--------|
| 단일 metric 기록 | < 100ns | TBD | ⏳ |
| 다중 metrics (5개) | < 500ns | TBD | ⏳ |
| 모니터링 없음 대비 Collection 오버헤드 | < 1% | TBD | ⏳ |
| 동시 metric 기록 (4 threads) | TBD | TBD | ⏳ |
| Metric 검색 (1000 metrics) | < 1ms | TBD | ⏳ |

### 2. Event Bus 성능

| 테스트 케이스 | 목표 | 측정값 | 상태 |
|-----------|--------|----------|--------|
| 단순 event publication | < 10μs | TBD | ⏳ |
| 복잡한 event publication | < 50μs | TBD | ⏳ |
| Event 전달 (1 subscriber) | < 100μs | TBD | ⏳ |
| Event 전달 (10 subscribers) | TBD | TBD | ⏳ |
| Event bus 처리량 | > 100k/s | TBD | ⏳ |
| 동시 publication (8 threads) | TBD | TBD | ⏳ |

### 3. System Resource Collection

| 테스트 케이스 | 목표 | 측정값 | 상태 |
|-----------|--------|----------|--------|
| CPU/Memory collection 지연 시간 | < 1ms | TBD | ⏳ |
| 워크로드 오버헤드가 있는 Collection | < 1% | TBD | ⏳ |
| 동시 collection (4 threads) | TBD | TBD | ⏳ |
| Collection 처리량 | > 1000/s | TBD | ⏳ |
| 지속적 collection (5s) | TBD | TBD | ⏳ |

---

## 벤치마크 실행 방법

```bash
cd monitoring_system
cmake -B build -S . -DMONITORING_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/benchmarks/monitoring_benchmarks
```

---

**최종 업데이트**: 2025-10-07
**상태**: 인프라 완료
**다음 작업**: Google Benchmark 설치 및 측정 실행

---

*Last Updated: 2025-10-20*
