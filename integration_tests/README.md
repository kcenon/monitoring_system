# Monitoring System Integration Tests

This directory contains comprehensive integration tests for the monitoring_system, following the same pattern as common_system, thread_system, and logger_system.

## Overview

The integration test suite provides end-to-end testing of monitoring_system functionality, including:

- Metric collection and aggregation
- Performance monitoring and profiling
- System resource tracking
- Error handling and recovery
- Concurrent operations
- Performance benchmarks

## Test Organization

### Test Suites

1. **scenarios/** - Functional integration tests
   - `metrics_collection_test.cpp` - Metric registration, collection, and management (15 tests)
   - `monitoring_integration_test.cpp` - System integration and monitoring workflows (12 tests)

2. **performance/** - Performance and scalability tests
   - `monitoring_performance_test.cpp` - Throughput, latency, and scalability benchmarks (10 tests)

3. **failures/** - Error handling and edge cases
   - `error_handling_test.cpp` - Error conditions and recovery scenarios (12 tests)

### Framework

- `framework/system_fixture.h` - Base test fixtures for monitoring tests
- `framework/test_helpers.h` - Common utilities and helper functions

## Building and Running Tests

### Build Integration Tests

```bash
# Configure with integration tests enabled
cmake -B build -DMONITORING_BUILD_INTEGRATION_TESTS=ON

# Build
cmake --build build

# Run all integration tests
cd build
ctest -R integration

# Or run directly
./integration_tests/monitoring_integration_tests
```

### Run Specific Test Suites

```bash
# Run only scenario tests
./integration_tests/monitoring_integration_tests --gtest_filter="MetricsCollection*"
./integration_tests/monitoring_integration_tests --gtest_filter="MonitoringIntegration*"

# Run only performance tests
./integration_tests/monitoring_integration_tests --gtest_filter="*Performance*"

# Run only error handling tests
./integration_tests/monitoring_integration_tests --gtest_filter="ErrorHandling*"
```

### Coverage Reporting

```bash
# Build with coverage enabled
cmake -B build -DMONITORING_BUILD_INTEGRATION_TESTS=ON -DENABLE_COVERAGE=ON
cmake --build build

# Run tests and generate coverage
cd build
ctest
make integration_coverage

# View coverage report
open coverage/integration_html/index.html
```

## Performance Baselines

The integration tests validate the following performance targets:

| Metric | Target | Test |
|--------|--------|------|
| Collection Throughput | > 10,000 metrics/sec | MetricCollectionThroughput |
| Update Latency P50 | < 1 microsecond | LatencyMeasurementsP50 |
| Update Latency P95 | < 10 microseconds | LatencyMeasurementsP95 |
| Memory per Metric | < 1KB | MemoryOverheadPerMetric |
| Aggregation Latency | < 100 microseconds (1000 metrics) | AggregationPerformance |

## Test Categories

### Metrics Collection Tests (15 tests)

1. Metric registration and initialization
2. Counter operations
3. Gauge operations
4. Histogram operations
5. Multiple metric instances
6. Metric label/tag management
7. Time-series data collection
8. Metric aggregation
9. Concurrent metric updates
10. Metric type validation
11. Metric batch processing
12. Metric memory footprint
13. Metric value conversions
14. Metric timestamp management
15. Metric hash function

### Monitoring Integration Tests (12 tests)

1. System health monitoring
2. Resource usage tracking - CPU
3. Resource usage tracking - Memory
4. Alert threshold configuration
5. Alert triggering and notification
6. Multi-component monitoring
7. Custom metric exporters
8. Monitoring data persistence
9. IMonitor interface integration
10. Health check integration
11. Metrics snapshot retrieval
12. Monitor reset functionality

### Performance Tests (10 tests)

1. Metric collection throughput
2. Latency measurements - P50
3. Latency measurements - P95
4. Memory overhead per metric
5. Scalability with metric count
6. Concurrent collection performance
7. Aggregation performance
8. Export performance
9. Batch processing performance
10. Memory allocation performance

### Error Handling Tests (12 tests)

1. Invalid metric types
2. Duplicate metric registration
3. Missing metric errors
4. Storage failures
5. Export failures and retry
6. Resource exhaustion - Too many metrics
7. Corrupted monitoring data
8. Alert notification failures
9. Concurrent access errors
10. Invalid configuration errors
11. Memory allocation failures
12. Error code conversion

## Total: 49 Integration Tests

## CI/CD Integration

Integration tests are automatically run on:
- Ubuntu (Debug and Release)
- macOS (Debug and Release)

Coverage reports are generated for Debug builds and uploaded to Codecov.

## Writing New Tests

### Using the Test Fixtures

```cpp
#include "system_fixture.h"
#include "test_helpers.h"

class MyTest : public MonitoringSystemFixture {};

TEST_F(MyTest, TestSomething) {
    ASSERT_TRUE(StartMonitoring());

    // Your test code here
    ASSERT_TRUE(RecordSample("operation", std::chrono::microseconds(100)));

    auto metrics = GetPerformanceMetrics("operation");
    ASSERT_TRUE(metrics.has_value());
    EXPECT_EQ(metrics->call_count, 1);
}
```

### Helper Functions

- `CreateMonitor(name)` - Create a performance monitor
- `StartMonitoring()` - Initialize and start monitoring
- `RecordSample(name, duration)` - Record a performance sample
- `GetPerformanceMetrics(name)` - Get metrics for an operation
- `GetMetricValue(name)` - Get metric value from snapshot
- `WaitForCollection(timeout)` - Wait for metric collection
- `CreateMetric(name, type, value)` - Create a metric
- `GenerateMetricBatch(count)` - Generate test metrics

## Dependencies

- Google Test (>= 1.12.0)
- monitoring_system library
- C++20 compiler
- CMake >= 3.20

## Maintenance

When adding new features to monitoring_system:
1. Add corresponding integration tests
2. Update performance baselines if needed
3. Update this README with new test descriptions
4. Ensure CI/CD passes

## Coverage Goals

- Minimum coverage: 80%
- Target coverage: 90%
- Include edge cases and error scenarios

## Contact

For questions or issues with integration tests, please refer to the main monitoring_system documentation.
