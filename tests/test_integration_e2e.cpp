/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system
All rights reserved.
*****************************************************************************/

/**
 * @file test_integration_e2e.cpp
 * @brief End-to-end integration tests for the monitoring system
 *
 * Tests complete workflows and interactions between all major components
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <random>
#include <filesystem>

#include <kcenon/monitoring/core/result_types.h>
#include <kcenon/monitoring/interfaces/monitoring_core.h>
#include <kcenon/monitoring/tracing/distributed_tracer.h>
#include <kcenon/monitoring/core/performance_monitor.h>
#include <kcenon/monitoring/adaptive/adaptive_monitor.h>
#include <kcenon/monitoring/health/health_monitor.h>
#include <kcenon/monitoring/reliability/circuit_breaker.h>
#include <kcenon/monitoring/reliability/retry_policy.h>
#include <kcenon/monitoring/reliability/fault_tolerance_manager.h>
#include <kcenon/monitoring/exporters/opentelemetry_adapter.h>
#include <kcenon/monitoring/exporters/trace_exporters.h>
#include <kcenon/monitoring/storage/storage_backends.h>

using namespace kcenon::monitoring;

class IntegrationE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp directory for test outputs
        test_dir_ = std::filesystem::temp_directory_path() / "monitoring_e2e_test";
        std::filesystem::create_directories(test_dir_);
    }

    void TearDown() override {
        // Cleanup test directory
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
    }

    std::filesystem::path test_dir_;
};

/**
 * Test 1: Storage Backend Integration
 * Multiple backends → Concurrent operations → Data consistency
 */
TEST_F(IntegrationE2ETest, StorageBackendIntegration) {
    // 1. Create multiple storage backends
    storage_config file_config;
    file_config.type = storage_backend_type::file_json;
    file_config.path = (test_dir_ / "metrics.json").string();
    file_config.max_capacity = 100;

    storage_config memory_config;
    memory_config.type = storage_backend_type::memory_buffer;
    memory_config.max_capacity = 100;

    auto file_backend = std::make_unique<file_storage_backend>(file_config);
    auto memory_backend = std::make_unique<memory_storage_backend>(memory_config);

    // 2. Create test data
    std::vector<metrics_snapshot> snapshots;
    for (int i = 0; i < 50; ++i) {
        metrics_snapshot snapshot;
        snapshot.add_metric("metric_" + std::to_string(i), i * 1.5);
        snapshots.push_back(snapshot);
    }

    // 3. Store data in both backends concurrently
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    // Thread for file backend
    threads.emplace_back([&file_backend, &snapshots, &success_count]() {
        for (const auto& snapshot : snapshots) {
            auto result = file_backend->store(snapshot);
            if (result.is_ok()) success_count++;
        }
    });

    // Thread for memory backend
    threads.emplace_back([&memory_backend, &snapshots, &success_count]() {
        for (const auto& snapshot : snapshots) {
            auto result = memory_backend->store(snapshot);
            if (result.is_ok()) success_count++;
        }
    });

    // 4. Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count, static_cast<int>(snapshots.size() * 2));

    // 5. Verify data consistency
    EXPECT_EQ(file_backend->size(), 50u);
    EXPECT_EQ(memory_backend->size(), 50u);

    // 6. Test retrieval
    auto file_result = file_backend->retrieve(0);
    auto memory_result = memory_backend->retrieve(0);

    EXPECT_TRUE(file_result.is_ok());
    EXPECT_TRUE(memory_result.is_ok());

    // 7. Test flush
    auto flush_file = file_backend->flush();
    auto flush_memory = memory_backend->flush();

    EXPECT_TRUE(flush_file.is_ok());
    EXPECT_TRUE(flush_memory.is_ok());
}

/**
 * Test 2: Distributed Tracing End-to-End
 * Span creation → Context propagation → Export
 */
TEST_F(IntegrationE2ETest, DistributedTracingE2E) {
    // 1. Setup tracing components
    distributed_tracer tracer;
    auto otel_adapter = create_opentelemetry_compatibility_layer("e2e_service", "1.0.0");

    // 2. Initialize OTEL adapter
    auto init_result = otel_adapter->initialize();
    ASSERT_TRUE(init_result.is_ok());

    // 3. Create parent span
    auto parent_span_result = tracer.start_span("parent_operation", "e2e_service");
    ASSERT_TRUE(parent_span_result.is_ok());
    auto parent_span = parent_span_result.value();

    // 4. Create child span with parent context
    auto child_span_result = tracer.start_child_span(*parent_span, "child_operation");
    ASSERT_TRUE(child_span_result.is_ok());
    auto child_span = child_span_result.value();

    // 5. Add tags directly (trace_span uses tags map)
    child_span->tags["user_id"] = "test_user";
    child_span->tags["request_id"] = "req_123";

    // 6. Set error status
    child_span->status = trace_span::status_code::error;
    child_span->status_message = "Simulated error for testing";

    // 7. Finish spans
    tracer.finish_span(child_span);
    tracer.finish_span(parent_span);

    // 8. Export spans through OTEL adapter
    std::vector<trace_span> spans;
    spans.push_back(*parent_span);
    spans.push_back(*child_span);

    auto export_result = otel_adapter->export_spans(spans);
    EXPECT_TRUE(export_result.is_ok());

    // 9. Verify stats
    auto stats = otel_adapter->get_stats();
    EXPECT_GT(stats.pending_spans, 0u);

    // 10. Flush
    auto flush_result = otel_adapter->flush();
    EXPECT_TRUE(flush_result.is_ok());
}

/**
 * Test 3: Health Monitoring with Fault Recovery
 * Health checks → Failure detection → Recovery → Verification
 */
TEST_F(IntegrationE2ETest, HealthMonitoringWithRecovery) {
    // 1. Setup health monitoring
    auto& health_mon = global_health_monitor();

    // Use retry_executor with retry_config for retry logic
    retry_config cfg;
    cfg.max_attempts = 3;
    cfg.initial_delay = std::chrono::milliseconds(10);
    cfg.backoff_multiplier = 2.0;
    retry_executor<bool> retry_exec("recovery_executor", cfg);

    // 2. Register health checks using health_check_builder
    std::atomic<bool> service_healthy{true};

    auto db_check = health_check_builder()
        .with_name("database")
        .with_type(health_check_type::liveness)
        .with_check([&service_healthy]() {
            if (service_healthy) {
                return health_check_result::healthy("Database connection OK");
            } else {
                return health_check_result::unhealthy("Database connection failed");
            }
        })
        .build();
    health_mon.register_check("database", db_check);

    auto cache_check = health_check_builder()
        .with_name("cache")
        .with_type(health_check_type::liveness)
        .with_check([]() {
            return health_check_result::healthy("Cache service running");
        })
        .build();
    health_mon.register_check("cache", cache_check);

    // 3. Initial health check - should be healthy
    auto initial_health = health_mon.check_health();
    EXPECT_TRUE(initial_health.is_healthy());

    // 4. Simulate failure
    service_healthy = false;

    // 5. Attempt recovery with retry logic
    int recovery_attempts = 0;
    auto recovery_result = retry_exec.execute(
        [&service_healthy, &recovery_attempts]() -> result<bool> {
            recovery_attempts++;
            if (recovery_attempts >= 2) {
                service_healthy = true;
                return make_success(true);
            }
            return make_error<bool>(monitoring_error_code::operation_failed,
                                    "Still recovering");
        }
    );

    EXPECT_TRUE(recovery_result.is_ok());
    EXPECT_GE(recovery_attempts, 2);

    // 6. Verify health restored
    auto final_health = health_mon.check_health();
    EXPECT_TRUE(final_health.is_healthy());
}

/**
 * Test 4: Performance Monitoring with Adaptive Collector
 * Monitoring → Load simulation → Adaptation → Verification
 */
TEST_F(IntegrationE2ETest, PerformanceAdaptiveMonitoring) {
    // 1. Setup performance monitoring
    auto perf_monitor = std::make_shared<performance_monitor>("perf_test");
    adaptive_monitor adapter;

    // 2. Configure adaptation
    adaptive_config config;
    config.strategy = adaptation_strategy::balanced;
    config.high_threshold = 70.0;
    config.memory_warning_threshold = 80.0;
    config.high_sampling_rate = 0.2;
    config.idle_sampling_rate = 1.0;

    // 3. Register collector with adaptive monitor
    auto reg_result = adapter.register_collector("perf_test", perf_monitor, config);
    EXPECT_TRUE(reg_result.is_ok());

    // 4. Start adaptive monitoring
    auto start_result = adapter.start();
    EXPECT_TRUE(start_result.is_ok());

    // 5. Record some metrics
    for (int i = 0; i < 100; ++i) {
        auto timer = perf_monitor->time_operation("test_op_" + std::to_string(i % 10));
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // 6. Get adaptation stats
    auto stats_result = adapter.get_collector_stats("perf_test");
    EXPECT_TRUE(stats_result.is_ok());

    // 7. Stop monitoring
    auto stop_result = adapter.stop();
    EXPECT_TRUE(stop_result.is_ok());
}

/**
 * Test 5: Circuit Breaker and Retry Mechanism
 * Failure injection → Circuit breaking → Recovery
 */
TEST_F(IntegrationE2ETest, CircuitBreakerAndRetry) {
    // 1. Setup resilience components with fault_tolerance_manager
    fault_tolerance_config ft_config;
    ft_config.enable_circuit_breaker = true;
    ft_config.enable_retry = true;
    ft_config.circuit_config.failure_threshold = 3;
    ft_config.circuit_config.reset_timeout = std::chrono::milliseconds(100);
    ft_config.retry_cfg.max_attempts = 5;
    ft_config.retry_cfg.initial_delay = std::chrono::milliseconds(10);

    fault_tolerance_manager<bool> ft_manager("test_manager", ft_config);

    // 2. Simulate component with intermittent failures
    std::atomic<int> call_count{0};
    std::atomic<bool> should_fail{true};

    auto unreliable_operation = [&call_count, &should_fail]() -> result<bool> {
        call_count++;

        // Fail first 3 calls, then succeed
        if (call_count <= 3 && should_fail) {
            return make_error<bool>(monitoring_error_code::operation_failed,
                                    "Simulated failure");
        }

        return make_success(true);
    };

    // 3. Test fault tolerance execution
    auto ft_result = ft_manager.execute(unreliable_operation);
    EXPECT_TRUE(ft_result.is_ok());

    // 4. Reset and test standalone circuit breaker
    call_count = 0;
    should_fail = true;

    circuit_breaker_config cb_config;
    cb_config.failure_threshold = 3;
    cb_config.reset_timeout = std::chrono::milliseconds(100);
    circuit_breaker<bool> breaker("test_breaker", cb_config);

    // Trigger circuit breaker with failures
    for (int i = 0; i < 3; ++i) {
        auto cb_result = breaker.execute(unreliable_operation);
        EXPECT_FALSE(cb_result.is_ok());
    }

    // Circuit should be open
    EXPECT_EQ(breaker.get_state(), circuit_state::open);

    // Further calls should fail fast (circuit open)
    auto open_result = breaker.execute(unreliable_operation);
    EXPECT_FALSE(open_result.is_ok());

    // 5. Wait for circuit recovery
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Allow success for recovery
    should_fail = false;
    call_count = 0;

    // Circuit should transition to half-open and then closed
    auto recovery_result = breaker.execute(unreliable_operation);
    EXPECT_TRUE(recovery_result.is_ok());

    // After several successes, circuit should be closed
    for (int i = 0; i < 5; ++i) {
        auto stable_result = breaker.execute(unreliable_operation);
        EXPECT_TRUE(stable_result.is_ok());
    }

    EXPECT_EQ(breaker.get_state(), circuit_state::closed);
}

/**
 * Test 6: Export Pipeline Integration
 * Trace and storage export verification
 */
TEST_F(IntegrationE2ETest, ExportPipelineIntegration) {
    // 1. Setup OTEL adapter
    auto otel_adapter = create_opentelemetry_compatibility_layer("export_test", "1.0.0");
    auto init_result = otel_adapter->initialize();
    ASSERT_TRUE(init_result.is_ok());

    // 2. Create sample traces
    std::vector<trace_span> test_spans;
    for (int i = 0; i < 10; ++i) {
        trace_span span;
        span.trace_id = "trace_" + std::to_string(i);
        span.span_id = "span_" + std::to_string(i);
        span.operation_name = "operation_" + std::to_string(i);
        span.start_time = std::chrono::system_clock::now();
        span.end_time = span.start_time + std::chrono::milliseconds(100);
        span.tags["index"] = std::to_string(i);
        test_spans.push_back(span);
    }

    // 3. Export spans
    auto export_result = otel_adapter->export_spans(test_spans);
    EXPECT_TRUE(export_result.is_ok());

    // 4. Verify export stats
    auto stats = otel_adapter->get_stats();
    EXPECT_EQ(stats.pending_spans, test_spans.size());

    // 5. Create sample metrics
    monitoring_data test_data("export_test");
    test_data.add_metric("cpu_usage", 75.0);
    test_data.add_metric("memory_usage", 60.0);
    test_data.add_metric("request_count", 1000.0);

    // 6. Export metrics
    auto metrics_result = otel_adapter->export_metrics(test_data);
    EXPECT_TRUE(metrics_result.is_ok());

    // 7. Verify combined stats
    stats = otel_adapter->get_stats();
    EXPECT_GT(stats.pending_metrics, 0u);

    // 8. Flush all pending data
    auto flush_result = otel_adapter->flush();
    EXPECT_TRUE(flush_result.is_ok());

    // 9. Verify flush completed
    stats = otel_adapter->get_stats();
    EXPECT_EQ(stats.pending_spans, 0u);
    EXPECT_EQ(stats.pending_metrics, 0u);
}

/**
 * Test 7: Full System Load Test
 * High volume → All components → Performance verification
 */
TEST_F(IntegrationE2ETest, FullSystemLoadTest) {
    // 1. Setup components
    distributed_tracer tracer;
    auto perf_monitor = std::make_shared<performance_monitor>("load_perf");
    auto& health_mon = global_health_monitor();

    // 2. Configure for high load
    const int num_operations = 1000;
    const int num_threads = 10;

    // 3. Generate load
    auto start_time = std::chrono::steady_clock::now();
    std::vector<std::thread> load_generators;
    std::atomic<int> total_operations{0};

    for (int t = 0; t < num_threads; ++t) {
        load_generators.emplace_back([&tracer, &total_operations, t]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<> dis(0.0, 100.0);

            for (int i = 0; i < 100; ++i) {
                // Create span
                auto span_result = tracer.start_span("load_test_" + std::to_string(t), "load_service");
                if (span_result.is_ok()) {
                    auto span = span_result.value();
                    span->tags["thread"] = std::to_string(t);
                    span->tags["value"] = std::to_string(dis(gen));
                    tracer.finish_span(span);
                    total_operations++;
                }

                // Small delay to prevent overwhelming
                if (i % 10 == 0) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        });
    }

    // 4. Monitor while load is running
    std::thread monitor_thread([&health_mon, &start_time]() {
        while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(5)) {
            auto health = health_mon.check_health();
            // System should remain operational under load
            EXPECT_TRUE(health.is_operational());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // 5. Wait for completion
    for (auto& t : load_generators) {
        t.join();
    }
    monitor_thread.join();

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // 6. Verify performance
    EXPECT_EQ(total_operations, num_operations);

    // Calculate throughput
    double throughput = (total_operations * 1000.0) / duration.count();
    std::cout << "Load test throughput: " << throughput << " ops/sec" << std::endl;

    // Should achieve reasonable throughput
    EXPECT_GT(throughput, 100.0); // At least 100 ops/sec
}

/**
 * Test 8: Cross-Component Integration
 * Multiple components working together
 */
TEST_F(IntegrationE2ETest, CrossComponentIntegration) {
    // 1. Create storage backend
    storage_config config;
    config.type = storage_backend_type::memory_buffer;
    config.max_capacity = 1000;
    auto storage = std::make_unique<memory_storage_backend>(config);

    // 2. Create tracer
    distributed_tracer tracer;

    // 3. Create performance monitor
    auto perf_monitor = std::make_shared<performance_monitor>("integration_perf");

    // 4. Create metrics snapshot and record performance
    metrics_snapshot snapshot;

    // Add performance metrics using scoped timer
    {
        auto timer = perf_monitor->time_operation("cpu_measurement");
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Get profiler metrics and add to snapshot
    auto metrics = perf_monitor->get_profiler().get_all_metrics();
    for (const auto& metric : metrics) {
        snapshot.add_metric(metric.operation_name + "_count",
                           static_cast<double>(metric.call_count));
    }

    // Add some direct metrics
    snapshot.add_metric("cpu_usage", 45.0);
    snapshot.add_metric("memory_usage", 60.0);

    // 5. Store snapshot
    auto store_result = storage->store(snapshot);
    EXPECT_TRUE(store_result.is_ok());

    // 6. Create trace span
    auto span_result = tracer.start_span("cross_component_test", "test_service");
    ASSERT_TRUE(span_result.is_ok());
    auto span = span_result.value();

    // 7. Add metrics to span as tags
    span->tags["cpu_usage"] = "45.0";
    span->tags["memory_usage"] = "60.0";

    tracer.finish_span(span);

    // 8. Verify storage
    EXPECT_EQ(storage->size(), 1u);

    auto retrieved = storage->retrieve(0);
    EXPECT_TRUE(retrieved.is_ok());

    // 9. Verify metrics in retrieved snapshot
    auto cpu_metric = retrieved.value().get_metric("cpu_usage");
    EXPECT_TRUE(cpu_metric.has_value());
    EXPECT_DOUBLE_EQ(cpu_metric.value(), 45.0);

    auto mem_metric = retrieved.value().get_metric("memory_usage");
    EXPECT_TRUE(mem_metric.has_value());
    EXPECT_DOUBLE_EQ(mem_metric.value(), 60.0);
}

// Note: main() function is provided by GTest framework or other test files
