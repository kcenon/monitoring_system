# Performance Profiling Guide for monitoring_system

**Date**: 2025-11-09
**Sprint**: Sprint 2 - Hot Path Optimization

## Purpose

This document outlines the profiling approach for identifying performance bottlenecks in the monitoring_system hot paths, particularly in metric collection operations.

## Profiling Tools

### macOS

**Instruments (Xcode)**
- Time Profiler: Identify CPU hotspots
- System Trace: Analyze thread scheduling and context switches
- Allocations: Track memory allocation patterns

**Usage**:
```bash
# Build with release + debug symbols
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..
make

# Run with Instruments
instruments -t "Time Profiler" -D profiling_results.trace ./build/bin/monitoring_benchmarks
```

### Linux

**perf**
```bash
# Record performance data
perf record -g ./build/bin/monitoring_benchmarks

# Analyze results
perf report

# Find hotspots
perf top
```

**Valgrind (Callgrind)**
```bash
valgrind --tool=callgrind --callgrind-out-file=callgrind.out ./build/bin/monitoring_benchmarks

# Visualize with KCachegrind
kcachegrind callgrind.out
```

## Hot Path Analysis Target

Based on IMPROVEMENT_PLAN.md analysis, the primary hot path is metric collection:

### Target Code Path

```cpp
// src/core/performance_monitor.cpp:30-81
performance_monitor::start_operation(operation_name)
  → std::shared_lock<std::shared_mutex> read_lock(profiles_mutex_);
  → profiles_.find(operation_name);
  → (if not found) upgrade to write lock
  → std::unique_lock<std::shared_mutex> write_lock(profiles_mutex_);
```

### Expected Bottlenecks

1. **Lock Contention**: `shared_mutex` under high concurrency
2. **Hash Map Lookups**: `profiles_.find()` on every operation
3. **Memory Allocation**: Creating new `performance_profile` objects
4. **Cache Misses**: Accessing scattered profile data

## Profiling Procedure

### Step 1: Baseline Measurement

Already completed in Sprint 1 (see docs/PERFORMANCE_BASELINE.md):
- Single-threaded recording: 292 ns (~3.43M ops/sec)
- Concurrent recording (4 threads): 1,094 ns real / 921 ns CPU

### Step 2: Instrument Hot Paths

```cpp
// Example: Add timestamps for fine-grained profiling
auto t1 = std::chrono::high_resolution_clock::now();
std::shared_lock<std::shared_mutex> read_lock(profiles_mutex_);
auto t2 = std::chrono::high_resolution_clock::now();
auto lock_time = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count();
// Log or accumulate lock_time
```

### Step 3: Run Under Profiler

```bash
# macOS Instruments
instruments -t "Time Profiler" -D profile.trace ./build/bin/monitoring_benchmarks \
  --benchmark_filter=BM_RecordMetric_Concurrent

# Linux perf
perf record -g --call-graph dwarf ./build/bin/monitoring_benchmarks \
  --benchmark_filter=BM_RecordMetric_Concurrent
perf report --stdio
```

### Step 4: Analyze Results

Key metrics to examine:
- **% CPU time** in mutex operations
- **Call count** of `profiles_.find()`
- **Cache miss rate** (perf stat -e cache-misses)
- **Thread contention** (Context Switch Instrument)

## Optimization Strategy

Based on profiling results, implement optimizations in order:

### Priority 1: Lock Contention
- Implement thread-local metric buffers (Task 3.1 in Sprint 3)
- Reduce critical section size
- Consider lock-free alternatives

### Priority 2: Lookup Performance
- Cache frequently accessed profiles
- Pre-allocate profile objects
- Implement LRU eviction (Task 2.3)

### Priority 3: Memory Efficiency
- Object pooling for profiles
- Reduce allocations in hot path
- Align data structures to cache lines

## Success Criteria

Target improvements (to be verified in Task 2.4):
- **Lock contention**: < 5% of total time
- **Single-thread throughput**: > 5M ops/sec (vs current 3.43M)
- **Concurrent throughput**: > 10M ops/sec total (4 threads)
- **Latency**: < 200ns per operation (vs current 292ns)

## Next Steps

1. Run profiling with real workload patterns
2. Implement identified optimizations (Tasks 2.2, 2.3)
3. Verify improvements with benchmarks (Task 2.4)
4. Document findings and update baseline

## References

- Sprint 1 Baseline: docs/PERFORMANCE_BASELINE.md
- IMPROVEMENT_PLAN.md: Sprint 2 - Hot Path Optimization
- Lock-Free Design (Sprint 3-4): Thread-local collector approach
