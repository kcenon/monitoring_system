# Performance Baseline Measurements

**Date**: 2025-11-08
**System**: macOS (8-core, L1D: 64 KiB, L1I: 128 KiB, L2: 4096 KiB)
**Compiler**: Default system compiler
**Build Type**: Release

## Executive Summary

This document records the baseline performance measurements for the monitoring_system, establishing targets for optimization work and regression detection.

## Key Findings

### 1. Single-Threaded Performance

| Metric | Value | Target | Status |
|--------|-------|--------|--------|
| **Profiler Recording (Single)** | 292 ns | <1000 ns | ✅ **EXCEEDS** |
| **Operations/sec** | 3.43M ops/sec | >1M ops/sec | ✅ **EXCEEDS** |
| **Scoped Timer Overhead** | 326 ns | <500 ns | ✅ **EXCELLENT** |

### 2. Multi-Sample Performance

| Metric | Value | Notes |
|--------|-------|-------|
| **Multiple Recordings (5x)** | 1,514 ns | 302.8 ns per sample |
| **Throughput** | 3.31M ops/sec | Consistent with single |
| **Coefficient of Variation** | 1.09% | Excellent stability |

### 3. Concurrent Performance (4 Threads)

| Metric | Value | Notes |
|--------|-------|-------|
| **Real Time** | 1,094 ns | Wall-clock time |
| **CPU Time** | 921 ns | Actual CPU usage |
| **Throughput** | 1.09M ops/sec | Per-thread effective rate |
| **Coefficient of Variation** | 0.66% | Very stable |

**Analysis**: Lock contention visible but minimal. CPU time < real time indicates good parallelism.

### 4. Retrieval Performance

| Metric | Value | Throughput | Notes |
|--------|-------|------------|-------|
| **Single Profile Retrieval** | 39.8 μs | 25.1k ops/sec | Read operation |
| **All Profiles Retrieval** | 52.0 μs | 19.2k ops/sec | Bulk read |

**Analysis**: Retrieval is significantly slower than recording (expected for aggregation operations).

### 5. Overhead Benchmarks

| Component | Time | Throughput | Status |
|-----------|------|------------|--------|
| **Scoped Timer** | 326 ns | 3.07M ops/sec | ✅ Low overhead |
| **Event Bus (Placeholder)** | 5.71 ns | 175M ops/sec | ⚠️ Placeholder only |
| **Collector (Placeholder)** | 4,082 ns real / 1,449 ns CPU | 690k ops/sec | ⚠️ Placeholder only |

## Detailed Benchmark Results

### Recording Performance

```
BM_ProfilerRecording_Single_mean              292 ns    3.43M items/s
BM_ProfilerRecording_Single_median            292 ns    3.43M items/s
BM_ProfilerRecording_Single_stddev          0.163 ns    0.05% CV

BM_ProfilerRecording_Multiple_mean          1,514 ns    3.31M items/s
BM_ProfilerRecording_Multiple_median        1,505 ns    3.32M items/s
BM_ProfilerRecording_Multiple_stddev         16.5 ns    1.09% CV

BM_ProfilerRecording_Concurrent/4_mean      1,094 ns (real), 921 ns (CPU)
BM_ProfilerRecording_Concurrent/4_median    1,093 ns (real), 918 ns (CPU)
BM_ProfilerRecording_Concurrent/4_stddev     6.07 ns    0.66% CV
```

### Retrieval Performance

```
BM_ProfilerRetrieval_Single_mean           39,842 ns   25.1k items/s
BM_ProfilerRetrieval_Single_median         39,840 ns   25.1k items/s
BM_ProfilerRetrieval_Single_stddev           21.7 ns   0.05% CV

BM_ProfilerRetrieval_All_mean              51,954 ns   19.2k items/s
BM_ProfilerRetrieval_All_median            51,954 ns   19.2k items/s
BM_ProfilerRetrieval_All_stddev              86.8 ns   0.17% CV
```

### Timer Overhead

```
BM_ScopedTimer_Overhead_mean                  326 ns   3.07M items/s
BM_ScopedTimer_Overhead_median                326 ns   3.07M items/s
BM_ScopedTimer_Overhead_stddev              0.633 ns   0.19% CV
```

## Performance Goals Assessment

### Original Claims (from ARCHITECTURE.md)

| Claim | Measured | Status |
|-------|----------|--------|
| **10M+ operations/second** | 3.43M ops/sec | ⚠️ **Below claim** |
| **<10μs overhead per metric** | 292 ns (0.292 μs) | ✅ **Far exceeds** |

### Realistic Performance Targets

Based on these baseline measurements, we establish the following **realistic** performance goals:

| Metric | Current | Sprint 2 Target | Sprint 3-4 Target |
|--------|---------|-----------------|-------------------|
| **Single-thread recording** | 292 ns | <200 ns | <150 ns |
| **Throughput** | 3.43M ops/sec | >5M ops/sec | >7M ops/sec |
| **Concurrent (4 threads)** | 1.09M ops/sec | >2M ops/sec | >3M ops/sec |
| **Lock contention** | ~16% overhead | <10% | <5% |

**Note**: 10M ops/sec goal requires lock-free thread-local collection (Sprint 3-4 implementation).

## Performance Regression Thresholds

CI will fail if performance degrades beyond these thresholds:

| Benchmark | Max Acceptable | Action |
|-----------|----------------|--------|
| **Recording (single)** | 350 ns (20% regression) | Block merge |
| **Recording (concurrent)** | 1,200 ns (10% regression) | Block merge |
| **Scoped timer** | 400 ns (23% regression) | Block merge |
| **Retrieval** | 50 μs (25% regression) | Warning only |

## Next Steps

### Sprint 2: Hot Path Optimization
- [ ] Profile with perf/Instruments
- [ ] Identify lock contention points
- [ ] Implement LRU eviction for unbounded growth
- [ ] Target: 5M+ ops/sec single-threaded

### Sprint 3-4: Lock-Free Path
- [ ] Implement thread-local metric collection
- [ ] Batch flush to central collector
- [ ] Target: 7-10M ops/sec single-threaded
- [ ] Target: <5% lock contention

## Appendix: Environment Details

```
CPU: 8 cores @ 24 MHz (reported)
Cache:
  L1 Data: 64 KiB
  L1 Instruction: 128 KiB
  L2 Unified: 4096 KiB (x8)

Load Average: 1.21, 1.50, 1.57
OS: macOS
Build: CMake Release configuration
Compiler: System default

Benchmark Configuration:
- Repetitions: 3
- Output Format: JSON
- Thread Affinity: Not set (warning)
```

## References

1. Raw benchmark data: `benchmark_results.json`
2. Benchmark source: `benchmarks/metric_collection_bench.cpp`
3. Sprint planning: `IMPROVEMENT_PLAN.md` (Sprint 2)

---

**Last Updated**: 2025-11-08
**Responsibility**: Senior Developer (Performance Engineering)
