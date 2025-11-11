# Thread-Local Collector Design

**Date**: 2025-11-09
**Sprint**: 3-4 (Lock-Free Path)
**Status**: Design Phase

## Problem Statement

Current `performance_profiler::record_sample()` implementation uses locks:
- `shared_mutex` for profile lookup (hot path)
- Per-profile mutex for sample recording

**Performance Impact**:
- Current: 42.3 ns/op (single-threaded), 413 ns/op (4 threads concurrent)
- Lock contention increases with thread count
- Scalability limited by mutex operations

**Goal**: Achieve >5M ops/sec with 8+ threads through lock-free metric collection.

---

## Design Overview

### Architecture

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  Thread 1   │     │  Thread 2   │     │  Thread N   │
│             │     │             │     │             │
│ TLS Buffer  │     │ TLS Buffer  │     │ TLS Buffer  │
│ (lock-free) │     │ (lock-free) │     │ (lock-free) │
└──────┬──────┘     └──────┬──────┘     └──────┬──────┘
       │                   │                   │
       │  Batch Flush      │  Batch Flush      │  Batch Flush
       │  (when full)      │  (when full)      │  (when full)
       │                   │                   │
       └───────────────────┴───────────────────┘
                           │
                           ▼
                  ┌────────────────────┐
                  │ Central Collector  │
                  │   (with lock)      │
                  └────────────────────┘
                           │
                           ▼
                  ┌────────────────────┐
                  │ Aggregated Metrics │
                  └────────────────────┘
```

### Key Components

1. **Thread-Local Buffer** (`thread_local_buffer`)
   - Per-thread storage for metric samples
   - Fixed-size circular buffer (default: 256 samples)
   - No locks required for writes (single-writer guarantee)

2. **Central Collector** (`central_collector`)
   - Receives batches from thread-local buffers
   - Uses mutex for thread-safe aggregation
   - Single point of contention (but infrequent)

3. **Flush Strategy**
   - **Threshold-based**: Flush when buffer reaches capacity
   - **Time-based** (optional): Periodic flush every N milliseconds
   - **Shutdown**: Final flush on thread termination

---

## Implementation Plan

### Phase 1: Thread-Local Buffer

```cpp
// include/kcenon/monitoring/core/thread_local_buffer.h

namespace kcenon::monitoring {

struct metric_sample {
    std::string operation_name;
    std::chrono::nanoseconds duration;
    bool success;
    std::chrono::steady_clock::time_point timestamp;
};

class thread_local_buffer {
public:
    static constexpr size_t DEFAULT_CAPACITY = 256;

    thread_local_buffer(size_t capacity = DEFAULT_CAPACITY);
    ~thread_local_buffer();

    /// Record a metric sample (lock-free)
    /// @return true if recorded, false if buffer full (triggers flush)
    bool record(const metric_sample& sample);

    /// Flush buffered samples to central collector
    /// @return Number of samples flushed
    size_t flush();

    /// Get current buffer utilization
    size_t size() const { return write_index_; }

    /// Check if buffer is full
    bool is_full() const { return write_index_ >= capacity_; }

private:
    std::vector<metric_sample> buffer_;
    size_t capacity_;
    size_t write_index_{0};  // Single-writer, no atomic needed
};

} // namespace kcenon::monitoring
```

### Phase 2: Central Collector

```cpp
// include/kcenon/monitoring/core/central_collector.h

namespace kcenon::monitoring {

class central_collector {
public:
    /// Receive a batch of samples from a thread-local buffer
    /// @thread_safety Thread-safe (uses mutex internally)
    void receive_batch(const std::vector<metric_sample>& samples);

    /// Get aggregated metrics for an operation
    result<performance_profile> get_profile(const std::string& operation_name);

    /// Get all profiles
    std::unordered_map<std::string, performance_profile> get_all_profiles();

    /// Clear all collected data
    void clear();

private:
    std::shared_mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<profile_data>> profiles_;
    size_t max_profiles_{10000};  // LRU eviction threshold
};

} // namespace kcenon::monitoring
```

### Phase 3: Integration with performance_profiler

```cpp
// Modified performance_profiler::record_sample

result<bool> performance_profiler::record_sample(
    const std::string& operation_name,
    std::chrono::nanoseconds duration,
    bool success) {

    if (!enabled_) {
        return result<bool>(true);
    }

    // Get or create thread-local buffer
    static thread_local thread_local_buffer buffer;

    metric_sample sample{
        .operation_name = operation_name,
        .duration = duration,
        .success = success,
        .timestamp = std::chrono::steady_clock::now()
    };

    // Record to thread-local buffer (lock-free!)
    if (!buffer.record(sample)) {
        // Buffer full, flush to central collector
        buffer.flush();
        // Retry recording
        buffer.record(sample);
    }

    return result<bool>(true);
}
```

---

## Performance Analysis

### Expected Improvements

| Metric | Current (Sprint 2) | Target (Sprint 3-4) | Improvement |
|--------|-------------------|---------------------|-------------|
| Single-threaded | 42.3 ns/op | 10-15 ns/op | ~3x faster |
| 4 threads | 413 ns/op (real) | <100 ns/op (real) | ~4x faster |
| 8 threads | Unknown | <150 ns/op (real) | New capability |
| Throughput (8 cores) | Unknown | >5M ops/sec | Scalable |

### Breakdown

**Hot Path (Thread-Local Recording)**:
- No mutex acquisition: -30 ns
- Simple array write: ~5 ns
- String copy to buffer: ~10 ns
- **Total: ~15 ns**

**Cold Path (Flush)**:
- Occurs every 256 samples
- Amortized cost: ~5-10 ns per sample
- Single mutex for batch: negligible impact

**Net Improvement**: 42.3 ns → 15 ns ≈ **2.8x faster**

---

## Memory Overhead

- **Per Thread**: 256 samples × ~64 bytes = 16 KB
- **100 Threads**: 1.6 MB total
- **Acceptable** for most applications

---

## Trade-offs

### Pros
✅ Lock-free hot path (single-threaded writes)
✅ Excellent scalability (no contention)
✅ Batching reduces central collector pressure
✅ Simple implementation (no lock-free data structures needed)

### Cons
❌ Memory overhead per thread
❌ Slightly delayed metric visibility (until flush)
❌ Potential sample loss if thread exits before flush

### Mitigations
- Make buffer size configurable
- Implement thread destructor flush
- Add periodic time-based flush option

---

## Testing Strategy

### Unit Tests

1. **Thread-Local Buffer**
   - Record samples up to capacity
   - Flush behavior
   - Overflow handling

2. **Central Collector**
   - Concurrent batch reception
   - Profile aggregation correctness
   - LRU eviction

3. **Integration**
   - End-to-end metric recording
   - Multi-threaded stress test
   - Flush on thread termination

### Performance Tests

```cpp
// Before: Current implementation
BM_RecordMetric/threads:8  413 ns (real), 921 ns (CPU)

// After: Thread-local collector
BM_RecordMetric_TLS/threads:8  <150 ns (real), <100 ns (CPU)

// Regression check: Ensure single-threaded doesn't degrade
BM_RecordMetric_TLS/threads:1  <20 ns (real)
```

---

## Migration Path

### Phase 1 (Sprint 3): Infrastructure
- Implement `thread_local_buffer`
- Implement `central_collector`
- Unit tests

### Phase 2 (Sprint 4): Integration
- Integrate with `performance_profiler`
- Add configuration option (`use_thread_local_collection`)
- Performance benchmarks

### Phase 3: Rollout
- Default to thread-local collection if performance verified
- Keep legacy path as fallback option

---

## Configuration

```cpp
struct collector_config {
    bool use_thread_local{true};           // Enable TLS collection
    size_t buffer_capacity{256};           // Samples per thread
    std::optional<std::chrono::milliseconds> flush_interval;  // Periodic flush
};
```

---

## Acceptance Criteria

- [x] Design document approved
- [ ] Thread-local buffer implemented and tested
- [ ] Central collector implemented and tested
- [ ] Integration complete with performance_profiler
- [ ] Performance benchmarks show >30% improvement
- [ ] All existing tests pass
- [ ] ThreadSanitizer clean (no data races)

---

**Next Steps**:
1. Review and approve design
2. Implement `thread_local_buffer` (Week 1)
3. Implement `central_collector` (Week 1-2)
4. Integration and testing (Week 2-3)
5. Performance validation (Week 3-4)
