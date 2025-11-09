/**
 * @file test_lock_free_collector.cpp
 * @brief Integration tests for lock-free collector components (Sprint 3-4)
 */

#include <gtest/gtest.h>
#include <kcenon/monitoring/core/thread_local_buffer.h>
#include <kcenon/monitoring/core/central_collector.h>
#include <thread>
#include <vector>

using namespace kcenon::monitoring;

class LockFreeCollectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        collector = std::make_shared<central_collector>();
    }

    std::shared_ptr<central_collector> collector;
};

TEST_F(LockFreeCollectorTest, BasicSampleRecording) {
    thread_local_buffer buffer(256, collector);

    // Record some samples
    metric_sample sample1{"operation1", std::chrono::nanoseconds(1000), true};
    metric_sample sample2{"operation1", std::chrono::nanoseconds(2000), true};
    metric_sample sample3{"operation1", std::chrono::nanoseconds(1500), false};

    EXPECT_TRUE(buffer.record(sample1));
    EXPECT_TRUE(buffer.record(sample2));
    EXPECT_TRUE(buffer.record(sample3));

    EXPECT_EQ(buffer.size(), 3);

    // Flush to collector
    size_t flushed = buffer.flush();
    EXPECT_EQ(flushed, 3);
    EXPECT_EQ(buffer.size(), 0);

    // Check collector stats
    auto stats = collector->get_stats();
    EXPECT_EQ(stats.total_samples, 3);
    EXPECT_EQ(stats.batches_received, 1);
    EXPECT_EQ(stats.operation_count, 1);

    // Get profile
    auto profile_result = collector->get_profile("operation1");
    ASSERT_TRUE(profile_result.is_ok());

    auto profile = profile_result.value();
    EXPECT_EQ(profile.total_calls, 3);
    EXPECT_EQ(profile.error_count, 1);
    EXPECT_EQ(profile.min_duration_ns, 1000);
    EXPECT_EQ(profile.max_duration_ns, 2000);
}

TEST_F(LockFreeCollectorTest, BufferAutoFlush) {
    thread_local_buffer buffer(10, collector);  // Small buffer

    // Fill buffer
    for (int i = 0; i < 10; ++i) {
        metric_sample sample{"op", std::chrono::nanoseconds(100), true};
        EXPECT_TRUE(buffer.record(sample));
    }

    EXPECT_TRUE(buffer.is_full());

    // Next record should fail
    metric_sample sample{"op", std::chrono::nanoseconds(100), true};
    EXPECT_FALSE(buffer.record(sample));

    // But auto-flush should work
    EXPECT_TRUE(buffer.record_auto_flush(sample));
    EXPECT_EQ(buffer.size(), 1);  // Buffer now has 1 sample after flush+record

    // Check collector received the batch
    auto stats = collector->get_stats();
    EXPECT_EQ(stats.total_samples, 10);  // First 10 samples flushed
}

TEST_F(LockFreeCollectorTest, MultiThreadedCollection) {
    constexpr int NUM_THREADS = 4;
    constexpr int SAMPLES_PER_THREAD = 1000;

    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t]() {
            thread_local_buffer buffer(256, collector);
            std::string op_name = "thread_op_" + std::to_string(t);

            for (int i = 0; i < SAMPLES_PER_THREAD; ++i) {
                metric_sample sample{
                    op_name,
                    std::chrono::nanoseconds(100 + i),
                    (i % 10) != 0  // 10% error rate
                };
                buffer.record_auto_flush(sample);
            }

            // Final flush
            buffer.flush();
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // Verify all samples were collected
    auto stats = collector->get_stats();
    EXPECT_EQ(stats.total_samples, NUM_THREADS * SAMPLES_PER_THREAD);
    EXPECT_EQ(stats.operation_count, NUM_THREADS);

    // Verify each thread's profile
    for (int t = 0; t < NUM_THREADS; ++t) {
        std::string op_name = "thread_op_" + std::to_string(t);
        auto profile_result = collector->get_profile(op_name);
        ASSERT_TRUE(profile_result.is_ok());

        auto profile = profile_result.value();
        EXPECT_EQ(profile.total_calls, SAMPLES_PER_THREAD);
        EXPECT_EQ(profile.error_count, SAMPLES_PER_THREAD / 10);
    }
}

TEST_F(LockFreeCollectorTest, LRUEviction) {
    auto limited_collector = std::make_shared<central_collector>(10);  // Only 10 profiles max

    thread_local_buffer buffer(256, limited_collector);

    // Create 15 operations (should evict 5)
    for (int i = 0; i < 15; ++i) {
        std::string op_name = "op_" + std::to_string(i);
        metric_sample sample{op_name, std::chrono::nanoseconds(1000), true};
        buffer.record_auto_flush(sample);
    }

    buffer.flush();

    // Should have exactly 10 operations (LRU evicted the rest)
    auto stats = limited_collector->get_stats();
    EXPECT_LE(stats.operation_count, 10);
    EXPECT_GE(stats.lru_evictions, 5);
}

TEST_F(LockFreeCollectorTest, GetAllProfiles) {
    thread_local_buffer buffer(256, collector);

    // Create multiple operations
    for (int i = 0; i < 5; ++i) {
        std::string op_name = "operation_" + std::to_string(i);
        metric_sample sample{op_name, std::chrono::nanoseconds(1000 * (i + 1)), true};
        buffer.record(sample);
    }

    buffer.flush();

    // Get all profiles
    auto all_profiles = collector->get_all_profiles();
    EXPECT_EQ(all_profiles.size(), 5);

    // Verify each operation exists
    for (int i = 0; i < 5; ++i) {
        std::string op_name = "operation_" + std::to_string(i);
        EXPECT_NE(all_profiles.find(op_name), all_profiles.end());
        EXPECT_EQ(all_profiles[op_name].total_calls, 1);
        EXPECT_EQ(all_profiles[op_name].avg_duration_ns, 1000 * (i + 1));
    }
}

TEST_F(LockFreeCollectorTest, ClearCollector) {
    thread_local_buffer buffer(256, collector);

    // Add some samples
    for (int i = 0; i < 10; ++i) {
        metric_sample sample{"op", std::chrono::nanoseconds(1000), true};
        buffer.record(sample);
    }
    buffer.flush();

    // Verify data exists
    auto stats_before = collector->get_stats();
    EXPECT_GT(stats_before.total_samples, 0);

    // Clear
    collector->clear();

    // Verify cleared
    auto stats_after = collector->get_stats();
    EXPECT_EQ(stats_after.total_samples, 0);
    EXPECT_EQ(stats_after.operation_count, 0);
    EXPECT_EQ(stats_after.batches_received, 0);
}

TEST_F(LockFreeCollectorTest, ProfileNotFound) {
    auto result = collector->get_profile("nonexistent");
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, static_cast<int>(monitoring_error_code::metric_not_found));
}
