/**
 * @file event_bus_bench.cpp
 * @brief Benchmark for event bus performance
 * @details Measures event publication and subscription latency
 *
 * Target Metrics:
 * - Event publication latency: < 10μs
 * - Event delivery latency: < 100μs
 * - Throughput: > 100k events/sec
 *
 * Phase 0, Task 0.2: Baseline Performance Benchmarking
 */

#include <benchmark/benchmark.h>
#include <kcenon/monitoring/core/event_bus.h>
#include <memory>
#include <string>
#include <atomic>

using namespace monitoring_system;

// Test event types
struct simple_event {
    int value;
};

struct complex_event {
    std::string message;
    int code;
    double timestamp;
};

//-----------------------------------------------------------------------------
// Event Publication Latency
//-----------------------------------------------------------------------------

static void BM_EventPublication_Simple(benchmark::State& state) {
    auto bus = std::make_shared<event_bus>();
    size_t published = 0;

    for (auto _ : state) {
        simple_event evt{42};
        bus->publish(evt);
        published++;
    }

    state.SetItemsProcessed(published);
    state.SetLabel("simple_event");
}
BENCHMARK(BM_EventPublication_Simple);

static void BM_EventPublication_Complex(benchmark::State& state) {
    auto bus = std::make_shared<event_bus>();
    size_t published = 0;

    for (auto _ : state) {
        complex_event evt{"Test message", 100, 123.456};
        bus->publish(evt);
        published++;
    }

    state.SetItemsProcessed(published);
    state.SetLabel("complex_event");
}
BENCHMARK(BM_EventPublication_Complex);

//-----------------------------------------------------------------------------
// Event Subscription Overhead
//-----------------------------------------------------------------------------

static void BM_EventSubscription(benchmark::State& state) {
    auto bus = std::make_shared<event_bus>();

    for (auto _ : state) {
        auto subscription = bus->subscribe<simple_event>(
            [](const simple_event& evt) {
                benchmark::DoNotOptimize(evt);
            });
        benchmark::DoNotOptimize(subscription);
    }

    state.SetLabel("subscription_creation");
}
BENCHMARK(BM_EventSubscription);

//-----------------------------------------------------------------------------
// Event Delivery Latency
//-----------------------------------------------------------------------------

static void BM_EventDelivery_OneSubscriber(benchmark::State& state) {
    auto bus = std::make_shared<event_bus>();
    std::atomic<size_t> received{0};

    auto sub = bus->subscribe<simple_event>(
        [&received](const simple_event& evt) {
            received.fetch_add(1, std::memory_order_relaxed);
            benchmark::DoNotOptimize(evt);
        });

    size_t published = 0;

    for (auto _ : state) {
        simple_event evt{42};
        bus->publish(evt);
        published++;
    }

    state.SetItemsProcessed(published);
    state.counters["received"] = received.load();
    state.SetLabel("one_subscriber");
}
BENCHMARK(BM_EventDelivery_OneSubscriber);

static void BM_EventDelivery_MultipleSubscribers(benchmark::State& state) {
    auto bus = std::make_shared<event_bus>();
    std::atomic<size_t> received{0};

    const int subscriber_count = state.range(0);

    // Create multiple subscribers
    std::vector<subscription_handle> subscriptions;
    for (int i = 0; i < subscriber_count; ++i) {
        auto sub = bus->subscribe<simple_event>(
            [&received](const simple_event& evt) {
                received.fetch_add(1, std::memory_order_relaxed);
                benchmark::DoNotOptimize(evt);
            });
        subscriptions.push_back(std::move(sub));
    }

    size_t published = 0;

    for (auto _ : state) {
        simple_event evt{42};
        bus->publish(evt);
        published++;
    }

    state.SetItemsProcessed(published * subscriber_count);
    state.counters["subscribers"] = subscriber_count;
    state.counters["received"] = received.load();
}
BENCHMARK(BM_EventDelivery_MultipleSubscribers)
    ->Arg(1)
    ->Arg(5)
    ->Arg(10)
    ->Arg(50)
    ->Arg(100);

//-----------------------------------------------------------------------------
// Event Bus Throughput
//-----------------------------------------------------------------------------

static void BM_EventBus_Throughput(benchmark::State& state) {
    auto bus = std::make_shared<event_bus>();
    std::atomic<size_t> received{0};

    auto sub = bus->subscribe<simple_event>(
        [&received](const simple_event&) {
            received.fetch_add(1, std::memory_order_relaxed);
        });

    size_t published = 0;

    for (auto _ : state) {
        simple_event evt{static_cast<int>(published++)};
        bus->publish(evt);
    }

    state.SetItemsProcessed(published);
    state.counters["events/sec"] = benchmark::Counter(
        published, benchmark::Counter::kIsRate);
}
BENCHMARK(BM_EventBus_Throughput);

//-----------------------------------------------------------------------------
// Concurrent Event Publication
//-----------------------------------------------------------------------------

static void BM_ConcurrentEventPublication(benchmark::State& state) {
    static std::shared_ptr<event_bus> shared_bus;

    if (state.thread_index() == 0) {
        shared_bus = std::make_shared<event_bus>();
    }

    std::atomic<size_t> published{0};

    for (auto _ : state) {
        simple_event evt{static_cast<int>(state.thread_index())};
        shared_bus->publish(evt);
        published.fetch_add(1, std::memory_order_relaxed);
    }

    state.SetItemsProcessed(published.load());
    state.counters["thread_count"] = state.threads();

    if (state.thread_index() == 0) {
        shared_bus.reset();
    }
}
BENCHMARK(BM_ConcurrentEventPublication)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8)
    ->Threads(16)
    ->UseRealTime();

//-----------------------------------------------------------------------------
// Event Priority Handling
//-----------------------------------------------------------------------------

static void BM_EventPriority(benchmark::State& state) {
    auto bus = std::make_shared<event_bus>();
    std::atomic<size_t> received{0};

    auto sub = bus->subscribe<simple_event>(
        [&received](const simple_event&) {
            received.fetch_add(1, std::memory_order_relaxed);
        });

    size_t published = 0;

    for (auto _ : state) {
        simple_event evt{static_cast<int>(published)};
        event_priority priority = (published % 2 == 0) ?
            event_priority::high : event_priority::normal;
        bus->publish(evt, priority);
        published++;
    }

    state.SetItemsProcessed(published);
    state.SetLabel("mixed_priority");
}
BENCHMARK(BM_EventPriority);

//-----------------------------------------------------------------------------
// Subscription Lifetime
//-----------------------------------------------------------------------------

static void BM_SubscriptionLifetime(benchmark::State& state) {
    auto bus = std::make_shared<event_bus>();

    for (auto _ : state) {
        {
            auto sub = bus->subscribe<simple_event>(
                [](const simple_event& evt) {
                    benchmark::DoNotOptimize(evt);
                });
            // Subscription destroyed here
        }
    }

    state.SetLabel("subscribe_unsubscribe");
}
BENCHMARK(BM_SubscriptionLifetime);

//-----------------------------------------------------------------------------
// Event Queue Size Impact
//-----------------------------------------------------------------------------

static void BM_EventQueueSize(benchmark::State& state) {
    event_bus_config config;
    config.max_queue_size = state.range(0);
    auto bus = std::make_shared<event_bus>(config);

    std::atomic<size_t> received{0};
    auto sub = bus->subscribe<simple_event>(
        [&received](const simple_event&) {
            received.fetch_add(1, std::memory_order_relaxed);
        });

    size_t published = 0;

    for (auto _ : state) {
        simple_event evt{static_cast<int>(published++)};
        bus->publish(evt);
    }

    state.SetItemsProcessed(published);
    state.counters["queue_size"] = config.max_queue_size;
}
BENCHMARK(BM_EventQueueSize)
    ->Arg(100)
    ->Arg(1000)
    ->Arg(10000)
    ->Arg(100000);

//-----------------------------------------------------------------------------
// Event Filtering Performance
//-----------------------------------------------------------------------------

static void BM_EventFiltering(benchmark::State& state) {
    auto bus = std::make_shared<event_bus>();
    std::atomic<size_t> received{0};

    // Subscribe with filter
    auto sub = bus->subscribe<simple_event>(
        [&received](const simple_event& evt) {
            if (evt.value % 2 == 0) {  // Only even values
                received.fetch_add(1, std::memory_order_relaxed);
            }
        });

    size_t published = 0;

    for (auto _ : state) {
        simple_event evt{static_cast<int>(published++)};
        bus->publish(evt);
    }

    state.SetItemsProcessed(published);
    state.counters["filtered_events"] = received.load();
    state.SetLabel("even_filter");
}
BENCHMARK(BM_EventFiltering);

//-----------------------------------------------------------------------------
// Burst Event Publication
//-----------------------------------------------------------------------------

static void BM_BurstEventPublication(benchmark::State& state) {
    auto bus = std::make_shared<event_bus>();
    std::atomic<size_t> received{0};

    auto sub = bus->subscribe<simple_event>(
        [&received](const simple_event&) {
            received.fetch_add(1, std::memory_order_relaxed);
        });

    const size_t burst_size = state.range(0);
    size_t total_published = 0;

    for (auto _ : state) {
        // Publish burst of events
        for (size_t i = 0; i < burst_size; ++i) {
            simple_event evt{static_cast<int>(i)};
            bus->publish(evt);
            total_published++;
        }
    }

    state.SetItemsProcessed(total_published);
    state.counters["burst_size"] = burst_size;
}
BENCHMARK(BM_BurstEventPublication)
    ->Arg(10)
    ->Arg(100)
    ->Arg(1000);
