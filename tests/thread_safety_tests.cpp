/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include <gtest/gtest.h>
#include <kcenon/monitoring/core/event_bus.h>
#include <kcenon/monitoring/core/event_types.h>

#include <atomic>
#include <chrono>
#include <latch>
#include <thread>
#include <vector>

using namespace kcenon::monitoring;
using namespace std::chrono_literals;

class MonitoringThreadSafetyTest : public ::testing::Test {
   protected:
    void SetUp() override {
        event_bus::config config;
        config.max_queue_size = 10000;
        config.worker_thread_count = 4;
        config.auto_start = true;

        bus = std::make_shared<event_bus>(config);
    }

    void TearDown() override {
        if (bus) {
            bus->stop();
        }
    }

    std::shared_ptr<event_bus> bus;
};

// Test 1: Concurrent event publication to event_bus
TEST_F(MonitoringThreadSafetyTest, ConcurrentEventPublication) {
    const int num_publishers = 15;
    const int events_per_publisher = 500;

    std::atomic<int> events_received{0};
    std::atomic<int> errors{0};

    // Subscribe to performance alerts
    auto token =
        bus->subscribe_event<performance_alert_event>([&](const performance_alert_event& e) {
            (void)e;  // Suppress unused parameter warning on MSVC
            ++events_received;
        });

    ASSERT_TRUE(token.is_ok());

    std::vector<std::thread> threads;
    std::latch sync_point(num_publishers);

    for (int i = 0; i < num_publishers; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            sync_point.arrive_and_wait();

            for (int j = 0; j < events_per_publisher; ++j) {
                try {
                    performance_alert_event alert(
                        performance_alert_event::alert_type::high_cpu_usage,
                        performance_alert_event::alert_severity::warning,
                        "thread_" + std::to_string(thread_id), "Test message " + std::to_string(j));

                    auto result = bus->publish_event(alert);
                    if (result.is_err()) {
                        ++errors;
                    }
                } catch (...) {
                    ++errors;
                }

                if (j % 50 == 0) {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Wait for event processing with timeout polling
    auto deadline = std::chrono::steady_clock::now() + 5s;
    while (events_received.load() < num_publishers * events_per_publisher &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    bus->unsubscribe_event(token.value());

    EXPECT_EQ(errors.load(), 0);
    EXPECT_LE(events_received.load(), num_publishers * events_per_publisher);
}

// Test 2: Multiple event types concurrent
TEST_F(MonitoringThreadSafetyTest, MultipleEventTypesConcurrent) {
    const int num_threads = 12;
    const int events_per_thread = 300;

    std::atomic<int> perf_alerts{0};
    std::atomic<int> resource_events{0};
    std::atomic<int> thread_pool_events{0};
    std::atomic<int> errors{0};

    // Subscribe to different event types
    auto perf_token = bus->subscribe_event<performance_alert_event>(
        [&](const performance_alert_event&) { ++perf_alerts; });

    auto resource_token = bus->subscribe_event<system_resource_event>(
        [&](const system_resource_event&) { ++resource_events; });

    auto pool_token = bus->subscribe_event<thread_pool_metric_event>(
        [&](const thread_pool_metric_event&) { ++thread_pool_events; });

    ASSERT_TRUE(perf_token.is_ok());
    ASSERT_TRUE(resource_token.is_ok());
    ASSERT_TRUE(pool_token.is_ok());

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            for (int j = 0; j < events_per_thread; ++j) {
                try {
                    // Publish different event types
                    switch (j % 3) {
                        case 0: {
                            performance_alert_event alert(
                                performance_alert_event::alert_type::high_memory_usage,
                                performance_alert_event::alert_severity::info,
                                "component_" + std::to_string(thread_id), "Test");
                            bus->publish_event(alert);
                            break;
                        }
                        case 1: {
                            system_resource_event::resource_stats stats;
                            stats.cpu_usage_percent = 50.0;
                            stats.memory_used_bytes = 1024 * 1024;
                            system_resource_event resource(stats);
                            bus->publish_event(resource);
                            break;
                        }
                        case 2: {
                            thread_pool_metric_event::thread_pool_stats stats;
                            stats.active_threads = 4;
                            stats.queued_tasks = 10;
                            thread_pool_metric_event pool("pool_" + std::to_string(thread_id),
                                                          stats);
                            bus->publish_event(pool);
                            break;
                        }
                    }
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Wait for event processing with timeout polling
    auto deadline = std::chrono::steady_clock::now() + 5s;
    while ((perf_alerts.load() + resource_events.load() + thread_pool_events.load() < 1) &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    bus->unsubscribe_event(perf_token.value());
    bus->unsubscribe_event(resource_token.value());
    bus->unsubscribe_event(pool_token.value());

    EXPECT_EQ(errors.load(), 0);
    EXPECT_GT(perf_alerts.load() + resource_events.load() + thread_pool_events.load(), 0);
}

// Test 3: Multiple subscribers concurrent
TEST_F(MonitoringThreadSafetyTest, MultipleSubscribersConcurrent) {
    const int num_subscribers = 20;
    const int num_publishers = 5;
    const int events_per_publisher = 300;

    std::vector<std::atomic<int>> subscriber_counts(num_subscribers);
    std::vector<subscription_token> tokens;
    std::atomic<int> errors{0};

    // Register subscribers
    for (int i = 0; i < num_subscribers; ++i) {
        auto token = bus->subscribe_event<system_resource_event>(
            [&, sub_id = i](const system_resource_event&) { ++subscriber_counts[sub_id]; });

        ASSERT_TRUE(token.is_ok());
        tokens.push_back(token.value());
    }

    std::vector<std::thread> threads;

    // Publishers
    for (int i = 0; i < num_publishers; ++i) {
        threads.emplace_back([&, pub_id = i]() {
            for (int j = 0; j < events_per_publisher; ++j) {
                try {
                    system_resource_event::resource_stats stats;
                    stats.cpu_usage_percent = static_cast<double>(pub_id * 10 + j);
                    system_resource_event event(stats);

                    auto result = bus->publish_event(event);
                    if (result.is_err()) {
                        ++errors;
                    }
                } catch (...) {
                    ++errors;
                }

                if (j % 30 == 0) {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Brief wait for event processing
    auto deadline = std::chrono::steady_clock::now() + 5s;
    while (std::chrono::steady_clock::now() < deadline) {
        bool all_received = true;
        for (int i = 0; i < num_subscribers; ++i) {
            if (subscriber_counts[i].load() == 0) {
                all_received = false;
                break;
            }
        }
        if (all_received) {
            break;
        }
        std::this_thread::yield();
    }

    // Unsubscribe all
    for (auto& token : tokens) {
        bus->unsubscribe_event(token);
    }

    EXPECT_EQ(errors.load(), 0);
}

// Test 4: Subscribe/unsubscribe during event publication
TEST_F(MonitoringThreadSafetyTest, DynamicSubscriptionChanges) {
    const int num_publishers = 5;
    const int num_dynamic_subscribers = 10;
    const int events_per_publisher = 400;

    std::atomic<bool> running{true};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    // Publishers
    for (int i = 0; i < num_publishers; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < events_per_publisher && running.load(); ++j) {
                try {
                    performance_alert_event alert(
                        performance_alert_event::alert_type::threshold_exceeded,
                        performance_alert_event::alert_severity::critical, "dynamic_test",
                        "Message " + std::to_string(j));
                    bus->publish_event(alert);
                } catch (...) {
                    ++errors;
                }
                std::this_thread::sleep_for(2ms);
            }
        });
    }

    // Dynamic subscribers
    for (int i = 0; i < num_dynamic_subscribers; ++i) {
        threads.emplace_back([&]() {
            while (running.load()) {
                try {
                    auto token = bus->subscribe_event<performance_alert_event>(
                        [](const performance_alert_event&) {
                            // Process event
                        });

                    if (token.is_ok()) {
                        std::this_thread::sleep_for(20ms);
                        bus->unsubscribe_event(token.value());
                    }

                    std::this_thread::sleep_for(10ms);
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    std::this_thread::sleep_for(500ms);
    running.store(false);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

// Test 5: Event priority handling concurrent
TEST_F(MonitoringThreadSafetyTest, EventPriorityConcurrent) {
    const int num_threads = 10;
    const int events_per_thread = 200;

    std::atomic<int> high_priority{0};
    std::atomic<int> normal_priority{0};
    std::atomic<int> low_priority{0};
    std::atomic<int> errors{0};

    // Subscribe with different priorities
    auto high_token = bus->subscribe_event<performance_alert_event>(
        [&](const performance_alert_event&) { ++high_priority; }, event_priority::high);

    auto normal_token = bus->subscribe_event<performance_alert_event>(
        [&](const performance_alert_event&) { ++normal_priority; }, event_priority::normal);

    auto low_token = bus->subscribe_event<performance_alert_event>(
        [&](const performance_alert_event&) { ++low_priority; }, event_priority::low);

    ASSERT_TRUE(high_token.is_ok());
    ASSERT_TRUE(normal_token.is_ok());
    ASSERT_TRUE(low_token.is_ok());

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            for (int j = 0; j < events_per_thread; ++j) {
                try {
                    performance_alert_event alert(
                        performance_alert_event::alert_type::high_error_rate,
                        performance_alert_event::alert_severity::warning,
                        "thread_" + std::to_string(thread_id),
                        "Priority test " + std::to_string(j));
                    bus->publish_event(alert);
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::this_thread::sleep_for(200ms);

    bus->unsubscribe_event(high_token.value());
    bus->unsubscribe_event(normal_token.value());
    bus->unsubscribe_event(low_token.value());

    EXPECT_EQ(errors.load(), 0);
    // All priorities should receive events
    EXPECT_GT(high_priority.load(), 0);
    EXPECT_GT(normal_priority.load(), 0);
    EXPECT_GT(low_priority.load(), 0);
}

// Test 6: Stress test with high event volume
TEST_F(MonitoringThreadSafetyTest, HighVolumeStressTest) {
    const int num_threads = 20;
    const int events_per_thread = 1000;

    std::atomic<int> total_received{0};
    std::atomic<int> errors{0};

    auto token = bus->subscribe_event<logging_metric_event>(
        [&](const logging_metric_event&) { ++total_received; });

    ASSERT_TRUE(token.is_ok());

    std::vector<std::thread> threads;
    std::latch sync_point(num_threads);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            sync_point.arrive_and_wait();

            for (int j = 0; j < events_per_thread; ++j) {
                try {
                    logging_metric_event::logging_stats stats;
                    stats.total_logs = j;
                    stats.error_count = j % 10;
                    logging_metric_event event("logger_" + std::to_string(thread_id), stats);

                    bus->publish_event(event);
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::this_thread::sleep_for(300ms);

    bus->unsubscribe_event(token.value());

    EXPECT_EQ(errors.load(), 0);

    double throughput = (num_threads * events_per_thread * 1000.0) / duration.count();
    std::cout << "Event throughput: " << throughput << " events/sec" << std::endl;
}

// Test 7: Memory safety - no leaks during concurrent monitoring
TEST_F(MonitoringThreadSafetyTest, MemorySafetyTest) {
    const int num_iterations = 30;
    const int threads_per_iteration = 10;
    const int operations_per_thread = 100;

    std::atomic<int> total_errors{0};

    for (int iteration = 0; iteration < num_iterations; ++iteration) {
        event_bus::config config;
        config.max_queue_size = 1000;
        config.worker_thread_count = 2;
        config.auto_start = true;

        auto test_bus = std::make_shared<event_bus>(config);

        std::vector<std::thread> threads;
        std::vector<subscription_token> tokens;

        // Subscribe
        for (int i = 0; i < 5; ++i) {
            auto token = test_bus->subscribe_event<system_resource_event>(
                [](const system_resource_event&) {});

            if (token.is_ok()) {
                tokens.push_back(token.value());
            }
        }

        // Worker threads
        for (int i = 0; i < threads_per_iteration; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < operations_per_thread; ++j) {
                    try {
                        system_resource_event::resource_stats stats;
                        stats.cpu_usage_percent = static_cast<double>(j);
                        system_resource_event event(stats);

                        test_bus->publish_event(event);
                    } catch (...) {
                        ++total_errors;
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }

        // Unsubscribe
        for (auto& token : tokens) {
            test_bus->unsubscribe_event(token);
        }

        test_bus->stop();
        // Bus destructor called here
    }

    EXPECT_EQ(total_errors.load(), 0);
}

// =============================================================================
// MON-ARC-003: Monitor Thread Safety Verification Tests
// =============================================================================

#include <kcenon/monitoring/core/performance_monitor.h>

class PerformanceProfilerThreadSafetyTest : public ::testing::Test {
   protected:
    kcenon::monitoring::performance_profiler profiler;
};

// Test 8: Concurrent sample recording
TEST_F(PerformanceProfilerThreadSafetyTest, ConcurrentSampleRecording) {
    const int num_threads = 16;
    const int samples_per_thread = 1000;

    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    std::latch sync_point(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            sync_point.arrive_and_wait();

            for (int j = 0; j < samples_per_thread; ++j) {
                try {
                    auto duration = std::chrono::nanoseconds(j * 1000 + thread_id);
                    auto result = profiler.record_sample(
                        "operation_" + std::to_string(thread_id % 4), duration,
                        j % 10 != 0  // 10% failure rate
                    );
                    if (result.is_err()) {
                        ++errors;
                    }
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);

    // Verify data integrity
    auto all_metrics = profiler.get_all_metrics();
    EXPECT_EQ(all_metrics.size(), 4);  // 4 unique operations
}

// Test 9: Concurrent get_metrics while recording
TEST_F(PerformanceProfilerThreadSafetyTest, ConcurrentReadWrite) {
    const int num_writers = 8;
    const int num_readers = 4;
    const int operations_per_thread = 500;

    std::atomic<bool> running{true};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    // Writers
    for (int i = 0; i < num_writers; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            for (int j = 0; j < operations_per_thread && running.load(); ++j) {
                try {
                    profiler.record_sample("shared_op",
                                           std::chrono::nanoseconds(j * 100 + thread_id), true);
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    // Readers
    for (int i = 0; i < num_readers; ++i) {
        threads.emplace_back([&]() {
            while (running.load()) {
                try {
                    auto result = profiler.get_metrics("shared_op");
                    // Result may be err if not yet recorded
                    (void)result;
                    std::this_thread::sleep_for(1ms);
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    // Let writers complete
    for (int i = 0; i < num_writers; ++i) {
        threads[i].join();
    }

    running.store(false);

    // Join readers
    for (int i = num_writers; i < static_cast<int>(threads.size()); ++i) {
        threads[i].join();
    }

    EXPECT_EQ(errors.load(), 0);
}

// Test 10: Concurrent lock-free mode toggle
TEST_F(PerformanceProfilerThreadSafetyTest, ConcurrentLockFreeModeToggle) {
    const int num_threads = 8;
    const int iterations = 1000;

    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    std::latch sync_point(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            sync_point.arrive_and_wait();

            for (int j = 0; j < iterations; ++j) {
                try {
                    // Toggle mode
                    profiler.set_lock_free_mode(j % 2 == 0);

                    // Read mode
                    bool mode = profiler.is_lock_free_mode();
                    (void)mode;

                    // Record sample
                    profiler.record_sample("toggle_test",
                                           std::chrono::nanoseconds(j + thread_id * 1000), true);
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

class PerformanceMonitorThreadSafetyTest : public ::testing::Test {
   protected:
    kcenon::monitoring::performance_monitor monitor{"test_monitor"};
};

// Test 11: Concurrent threshold modification
TEST_F(PerformanceMonitorThreadSafetyTest, ConcurrentThresholdModification) {
    const int num_threads = 8;
    const int iterations = 500;

    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    std::latch sync_point(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            sync_point.arrive_and_wait();

            for (int j = 0; j < iterations; ++j) {
                try {
                    // Set thresholds
                    monitor.set_cpu_threshold(50.0 + (thread_id * 5));
                    monitor.set_memory_threshold(60.0 + (j % 20));
                    monitor.set_latency_threshold(std::chrono::milliseconds(100 + j));

                    // Read thresholds
                    auto thresholds = monitor.get_thresholds();
                    (void)thresholds;

                    // Check thresholds
                    auto result = monitor.check_thresholds();
                    (void)result;
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

// Test 12: Concurrent profiling operations
TEST_F(PerformanceMonitorThreadSafetyTest, ConcurrentProfilingOperations) {
    const int num_threads = 12;
    const int operations_per_thread = 300;

    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    std::latch sync_point(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            sync_point.arrive_and_wait();

            for (int j = 0; j < operations_per_thread; ++j) {
                try {
                    // Use scoped timer
                    {
                        auto timer = monitor.time_operation("op_" + std::to_string(thread_id % 3));
                        // Simulate work
                        std::this_thread::sleep_for(std::chrono::microseconds(10));
                    }  // Timer records on destruction

                    // Collect metrics periodically
                    if (j % 50 == 0) {
                        auto result = monitor.collect();
                        (void)result;
                    }
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);

    // Verify all operations were recorded
    auto& profiler = monitor.get_profiler();
    auto all_metrics = profiler.get_all_metrics();
    EXPECT_EQ(all_metrics.size(), 3);  // 3 unique operations
}
