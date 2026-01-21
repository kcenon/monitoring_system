// BSD 3-Clause License
//
// Copyright (c) 2021-2025, monitoring_system contributors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file test_hot_path_helper.cpp
 * @brief Unit tests for hot-path optimization helpers
 */

#include <gtest/gtest.h>
#include <kcenon/monitoring/utils/hot_path_helper.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using namespace kcenon::monitoring::hot_path;

namespace {

struct TestData {
    int value{0};
    std::string name;
};

}  // namespace

class HotPathHelperTest : public ::testing::Test {
protected:
    void SetUp() override {
        map_.clear();
    }

    void TearDown() override {}

    std::unordered_map<std::string, std::unique_ptr<TestData>> map_;
    std::shared_mutex mutex_;
};

// =========================================================================
// get_or_create Tests
// =========================================================================

TEST_F(HotPathHelperTest, GetOrCreateNewEntry) {
    auto* ptr = get_or_create(
        map_, mutex_, "key1",
        []() { return std::make_unique<TestData>(); });

    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(map_.size(), 1u);
    EXPECT_EQ(ptr->value, 0);
}

TEST_F(HotPathHelperTest, GetOrCreateExistingEntry) {
    // Pre-create an entry
    map_["key1"] = std::make_unique<TestData>();
    map_["key1"]->value = 42;

    int create_count = 0;
    auto* ptr = get_or_create(
        map_, mutex_, "key1",
        [&create_count]() {
            ++create_count;
            return std::make_unique<TestData>();
        });

    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(map_.size(), 1u);
    EXPECT_EQ(ptr->value, 42);
    EXPECT_EQ(create_count, 0);  // Factory should not be called
}

TEST_F(HotPathHelperTest, GetOrCreateMultipleKeys) {
    auto* ptr1 = get_or_create(
        map_, mutex_, "key1",
        []() { return std::make_unique<TestData>(); });
    auto* ptr2 = get_or_create(
        map_, mutex_, "key2",
        []() { return std::make_unique<TestData>(); });

    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_NE(ptr1, ptr2);
    EXPECT_EQ(map_.size(), 2u);
}

// =========================================================================
// get_or_create_with_init Tests
// =========================================================================

TEST_F(HotPathHelperTest, GetOrCreateWithInitNewEntry) {
    auto* ptr = get_or_create_with_init(
        map_, mutex_, "key1",
        []() { return std::make_unique<TestData>(); },
        [](TestData& d) {
            d.value = 100;
            d.name = "initialized";
        });

    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->value, 100);
    EXPECT_EQ(ptr->name, "initialized");
}

TEST_F(HotPathHelperTest, GetOrCreateWithInitExistingEntry) {
    // Pre-create an entry
    map_["key1"] = std::make_unique<TestData>();
    map_["key1"]->value = 42;
    map_["key1"]->name = "original";

    int init_count = 0;
    auto* ptr = get_or_create_with_init(
        map_, mutex_, "key1",
        []() { return std::make_unique<TestData>(); },
        [&init_count](TestData& d) {
            ++init_count;
            d.value = 100;
        });

    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->value, 42);  // Should not be modified
    EXPECT_EQ(ptr->name, "original");
    EXPECT_EQ(init_count, 0);  // Init should not be called
}

// =========================================================================
// get_or_create_and_update Tests
// =========================================================================

TEST_F(HotPathHelperTest, GetOrCreateAndUpdateNewEntry) {
    int result = get_or_create_and_update(
        map_, mutex_, "key1",
        []() { return std::make_unique<TestData>(); },
        [](TestData& d) {
            d.value = 42;
            return d.value;
        });

    EXPECT_EQ(result, 42);
    EXPECT_EQ(map_["key1"]->value, 42);
}

TEST_F(HotPathHelperTest, GetOrCreateAndUpdateExistingEntry) {
    // Pre-create an entry
    map_["key1"] = std::make_unique<TestData>();
    map_["key1"]->value = 10;

    int result = get_or_create_and_update(
        map_, mutex_, "key1",
        []() { return std::make_unique<TestData>(); },
        [](TestData& d) {
            d.value += 5;
            return d.value;
        });

    EXPECT_EQ(result, 15);
    EXPECT_EQ(map_["key1"]->value, 15);
}

// =========================================================================
// Concurrent Access Tests
// =========================================================================

TEST_F(HotPathHelperTest, ConcurrentGetOrCreate) {
    std::atomic<int> create_count{0};
    const int num_threads = 10;
    const int iterations_per_thread = 1000;

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, &create_count, iterations_per_thread]() {
            for (int i = 0; i < iterations_per_thread; ++i) {
                auto* ptr = get_or_create(
                    map_, mutex_, "shared_key",
                    [&create_count]() {
                        ++create_count;
                        return std::make_unique<TestData>();
                    });
                ASSERT_NE(ptr, nullptr);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(map_.size(), 1u);
    EXPECT_EQ(create_count.load(), 1);  // Only one creation should occur
}

TEST_F(HotPathHelperTest, ConcurrentDifferentKeys) {
    std::atomic<int> total_creates{0};
    const int num_threads = 10;
    const int keys_per_thread = 100;

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, &total_creates, keys_per_thread]() {
            for (int i = 0; i < keys_per_thread; ++i) {
                std::string key = "key_" + std::to_string(t) + "_" + std::to_string(i);
                auto* ptr = get_or_create(
                    map_, mutex_, key,
                    [&total_creates]() {
                        ++total_creates;
                        return std::make_unique<TestData>();
                    });
                ASSERT_NE(ptr, nullptr);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(map_.size(), static_cast<size_t>(num_threads * keys_per_thread));
    EXPECT_EQ(total_creates.load(), num_threads * keys_per_thread);
}

TEST_F(HotPathHelperTest, ConcurrentMixedReadWrite) {
    // Pre-create some entries
    for (int i = 0; i < 50; ++i) {
        map_["existing_" + std::to_string(i)] = std::make_unique<TestData>();
    }

    const int num_threads = 8;
    const int iterations = 500;

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, iterations]() {
            for (int i = 0; i < iterations; ++i) {
                // Half the time access existing, half the time create new
                std::string key = (i % 2 == 0)
                    ? "existing_" + std::to_string(i % 50)
                    : "new_" + std::to_string(t) + "_" + std::to_string(i);

                auto* ptr = get_or_create(
                    map_, mutex_, key,
                    []() { return std::make_unique<TestData>(); });
                ASSERT_NE(ptr, nullptr);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GE(map_.size(), 50u);  // At least the pre-existing entries
}

// =========================================================================
// Performance Characteristics Test
// =========================================================================

TEST_F(HotPathHelperTest, HotPathOptimizationVerification) {
    // Create entry first
    map_["hot_key"] = std::make_unique<TestData>();

    std::atomic<int> create_calls{0};
    const int hot_path_iterations = 10000;

    // Simulate hot path - many reads, no creates
    for (int i = 0; i < hot_path_iterations; ++i) {
        auto* ptr = get_or_create(
            map_, mutex_, "hot_key",
            [&create_calls]() {
                ++create_calls;
                return std::make_unique<TestData>();
            });
        ASSERT_NE(ptr, nullptr);
    }

    // Verify factory was never called (hot path optimization working)
    EXPECT_EQ(create_calls.load(), 0);
}
