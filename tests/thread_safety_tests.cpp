/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include <gtest/gtest.h>
#include <kcenon/monitoring/core/event_bus.h>
#include <kcenon/monitoring/interfaces/metric_collector_interface.h>
#include <kcenon/monitoring/utils/time_series.h>

#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <barrier>

using namespace kcenon::monitoring;
using namespace std::chrono_literals;

class MonitoringThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test 1: Concurrent event publication to event_bus
TEST_F(MonitoringThreadSafetyTest, ConcurrentEventPublication) {
    event_bus bus;

    const int num_publishers = 15;
    const int events_per_publisher = 500;

    std::atomic<int> events_received{0};
    std::atomic<int> errors{0};

    auto subscriber_id = bus.subscribe([&](const event& e) {
        ++events_received;
    });

    std::vector<std::thread> threads;
    std::barrier sync_point(num_publishers);

    for (int i = 0; i < num_publishers; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            sync_point.arrive_and_wait();

            for (int j = 0; j < events_per_publisher; ++j) {
                try {
                    event e;
                    e.set_type("test_event");
                    e.set_source("thread_" + std::to_string(thread_id));
                    e.set_data("message_" + std::to_string(j));
                    bus.publish(std::move(e));
                } catch (...) {
                    ++errors;
                }

                if (j % 50 == 0) {
                    std::this_thread::sleep_for(1ms);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Process any remaining events
    std::this_thread::sleep_for(200ms);

    bus.unsubscribe(subscriber_id);

    EXPECT_EQ(errors.load(), 0);
    EXPECT_LE(events_received.load(), num_publishers * events_per_publisher);
}

// Test 2: Concurrent metric collection
TEST_F(MonitoringThreadSafetyTest, ConcurrentMetricCollection) {
    metric_collector collector;

    const int num_collectors = 10;
    const int metrics_per_collector = 1000;

    std::vector<std::thread> threads;
    std::atomic<int> collection_errors{0};

    for (int i = 0; i < num_collectors; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            for (int j = 0; j < metrics_per_collector; ++j) {
                try {
                    collector.record_metric("metric_" + std::to_string(thread_id),
                                          static_cast<double>(j));
                    collector.record_counter("counter_" + std::to_string(thread_id), 1);

                    if (j % 10 == 0) {
                        auto value = collector.get_metric("metric_" + std::to_string(thread_id));
                    }
                } catch (...) {
                    ++collection_errors;
                }

                if (j % 100 == 0) {
                    std::this_thread::sleep_for(1ms);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(collection_errors.load(), 0);
}

// Test 3: Time series concurrent writes
TEST_F(MonitoringThreadSafetyTest, TimeSeriesStress) {
    time_series<double> ts;

    const int num_writers = 12;
    const int writes_per_thread = 800;

    std::vector<std::thread> threads;
    std::atomic<int> write_errors{0};
    std::atomic<int> total_writes{0};

    for (int i = 0; i < num_writers; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            for (int j = 0; j < writes_per_thread; ++j) {
                try {
                    auto timestamp = std::chrono::system_clock::now();
                    ts.add_point(timestamp, static_cast<double>(thread_id * 1000 + j));
                    ++total_writes;
                } catch (...) {
                    ++write_errors;
                }

                if (j % 50 == 0) {
                    std::this_thread::sleep_for(1ms);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(write_errors.load(), 0);
    EXPECT_EQ(total_writes.load(), num_writers * writes_per_thread);
}

// Test 4: Multiple subscribers concurrent access
TEST_F(MonitoringThreadSafetyTest, MultipleSubscribersConcurrent) {
    event_bus bus;

    const int num_subscribers = 20;
    const int num_publishers = 5;
    const int events_per_publisher = 300;

    std::vector<std::atomic<int>> subscriber_counts(num_subscribers);
    std::vector<subscription_id> subscription_ids;
    std::atomic<int> errors{0};

    // Register subscribers
    for (int i = 0; i < num_subscribers; ++i) {
        auto id = bus.subscribe([&, sub_id = i](const event& e) {
            ++subscriber_counts[sub_id];
        });
        subscription_ids.push_back(id);
    }

    std::vector<std::thread> threads;

    // Publishers
    for (int i = 0; i < num_publishers; ++i) {
        threads.emplace_back([&, pub_id = i]() {
            for (int j = 0; j < events_per_publisher; ++j) {
                try {
                    event e;
                    e.set_type("broadcast");
                    e.set_data(std::to_string(pub_id * 1000 + j));
                    bus.publish(std::move(e));
                } catch (...) {
                    ++errors;
                }

                if (j % 30 == 0) {
                    std::this_thread::sleep_for(1ms);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::this_thread::sleep_for(100ms);

    // Unsubscribe all
    for (auto id : subscription_ids) {
        bus.unsubscribe(id);
    }

    EXPECT_EQ(errors.load(), 0);
}

// Test 5: Subscribe/unsubscribe during event publication
TEST_F(MonitoringThreadSafetyTest, DynamicSubscriptionChanges) {
    event_bus bus;

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
                    event e;
                    e.set_type("dynamic_test");
                    bus.publish(std::move(e));
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
                    auto id = bus.subscribe([](const event& e) {
                        // Process event
                    });

                    std::this_thread::sleep_for(20ms);

                    bus.unsubscribe(id);

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

// Test 6: Histogram concurrent updates
TEST_F(MonitoringThreadSafetyTest, HistogramConcurrentUpdates) {
    histogram hist;

    const int num_threads = 15;
    const int samples_per_thread = 1000;

    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            for (int j = 0; j < samples_per_thread; ++j) {
                try {
                    hist.record_value(static_cast<double>(thread_id * 100 + j));
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
    EXPECT_EQ(hist.count(), num_threads * samples_per_thread);
}

// Test 7: Concurrent metric aggregation
TEST_F(MonitoringThreadSafetyTest, MetricAggregationConcurrent) {
    metric_aggregator aggregator;

    const int num_threads = 12;
    const int operations_per_thread = 600;

    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            for (int j = 0; j < operations_per_thread; ++j) {
                try {
                    aggregator.add_sample("latency_ms", static_cast<double>(j));
                    aggregator.add_sample("throughput", static_cast<double>(thread_id * 10 + j));

                    if (j % 50 == 0) {
                        auto stats = aggregator.get_statistics("latency_ms");
                    }
                } catch (...) {
                    ++errors;
                }

                if (j % 100 == 0) {
                    std::this_thread::sleep_for(1ms);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

// Test 8: Event filtering concurrent access
TEST_F(MonitoringThreadSafetyTest, EventFilteringConcurrent) {
    event_bus bus;
    event_filter filter;

    const int num_publishers = 8;
    const int events_per_publisher = 500;

    std::atomic<int> filtered_events{0};
    std::atomic<int> errors{0};

    // Add filter rules
    filter.add_rule([](const event& e) {
        return e.get_type() == "allowed";
    });

    auto sub_id = bus.subscribe([&](const event& e) {
        if (filter.matches(e)) {
            ++filtered_events;
        }
    });

    std::vector<std::thread> threads;

    for (int i = 0; i < num_publishers; ++i) {
        threads.emplace_back([&, thread_id = i]() {
            for (int j = 0; j < events_per_publisher; ++j) {
                try {
                    event e;
                    e.set_type((j % 2 == 0) ? "allowed" : "blocked");
                    bus.publish(std::move(e));
                } catch (...) {
                    ++errors;
                }

                if (j % 50 == 0) {
                    std::this_thread::sleep_for(1ms);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    std::this_thread::sleep_for(100ms);
    bus.unsubscribe(sub_id);

    EXPECT_EQ(errors.load(), 0);
}

// Test 9: Ring buffer concurrent access (producer-consumer)
TEST_F(MonitoringThreadSafetyTest, RingBufferConcurrency) {
    ring_buffer<event> buffer(1000);

    const int num_producers = 8;
    const int num_consumers = 4;
    const int items_per_producer = 800;

    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<bool> running{true};
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;

    // Producers
    for (int i = 0; i < num_producers; ++i) {
        threads.emplace_back([&, prod_id = i]() {
            for (int j = 0; j < items_per_producer; ++j) {
                try {
                    event e;
                    e.set_data(std::to_string(prod_id * 1000 + j));

                    while (!buffer.try_push(std::move(e)) && running.load()) {
                        std::this_thread::sleep_for(1ms);
                    }

                    ++produced;
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    // Consumers
    for (int i = 0; i < num_consumers; ++i) {
        threads.emplace_back([&]() {
            while (running.load()) {
                try {
                    event e;
                    if (buffer.try_pop(e)) {
                        ++consumed;
                    } else {
                        std::this_thread::sleep_for(1ms);
                    }
                } catch (...) {
                    ++errors;
                }
            }
        });
    }

    // Wait for producers to finish
    for (int i = 0; i < num_producers; ++i) {
        threads[i].join();
    }

    // Allow consumers to drain the buffer
    std::this_thread::sleep_for(200ms);
    running.store(false);

    // Join consumers
    for (int i = num_producers; i < threads.size(); ++i) {
        threads[i].join();
    }

    EXPECT_EQ(errors.load(), 0);
    EXPECT_EQ(produced.load(), num_producers * items_per_producer);
}

// Test 10: Memory safety - no leaks during concurrent monitoring
TEST_F(MonitoringThreadSafetyTest, MemorySafetyTest) {
    const int num_iterations = 50;
    const int threads_per_iteration = 10;
    const int operations_per_thread = 100;

    std::atomic<int> total_errors{0};

    for (int iteration = 0; iteration < num_iterations; ++iteration) {
        event_bus bus;
        metric_collector collector;

        std::vector<std::thread> threads;
        std::vector<subscription_id> subscriptions;

        // Subscribe
        for (int i = 0; i < 5; ++i) {
            auto id = bus.subscribe([](const event& e) {});
            subscriptions.push_back(id);
        }

        // Worker threads
        for (int i = 0; i < threads_per_iteration; ++i) {
            threads.emplace_back([&]() {
                for (int j = 0; j < operations_per_thread; ++j) {
                    try {
                        event e;
                        e.set_type("test");
                        bus.publish(std::move(e));

                        collector.record_metric("test_metric", static_cast<double>(j));
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
        for (auto id : subscriptions) {
            bus.unsubscribe(id);
        }

        // Destructors called here
    }

    EXPECT_EQ(total_errors.load(), 0);
}
