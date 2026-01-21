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
 * @file hot_path_helper.h
 * @brief Reusable hot-path optimization patterns for concurrent map access
 * @date 2025
 *
 * Provides thread-safe get-or-create patterns using double-check locking
 * with shared_mutex for optimal read performance on hot paths.
 *
 * The pattern reduces lock contention by:
 * 1. First attempting read with shared_lock (allows concurrent readers)
 * 2. Only acquiring exclusive write_lock when creation is necessary
 * 3. Double-checking after write_lock to handle race conditions
 */

#pragma once

#include <shared_mutex>

namespace kcenon {
namespace monitoring {
namespace hot_path {

/**
 * @brief Get existing value or create new one using double-check locking
 *
 * This function implements the double-check locking pattern for thread-safe
 * lazy initialization in concurrent maps. It optimizes for the common case
 * where the key already exists (hot path) by using a shared lock first.
 *
 * @tparam Map Container type supporting find(), operator[], and emplace()
 * @tparam Key Key type for the container
 * @tparam CreateFn Callable returning the value to insert if key not found
 *
 * @param map The container to access
 * @param mutex The shared_mutex protecting the container
 * @param key The key to look up or create
 * @param create_fn Factory function called when creating a new entry
 *
 * @return Pointer to the value (existing or newly created). Never null
 *         unless create_fn fails. The pointer remains valid as long as
 *         the entry exists in the map.
 *
 * @thread_safety Thread-safe. Uses shared_lock for reads, unique_lock for writes.
 *
 * @note The returned pointer should only be used while holding appropriate
 *       locks or when the caller can guarantee the entry won't be removed.
 *
 * @example
 * @code
 * std::unordered_map<std::string, std::unique_ptr<Data>> data_map;
 * std::shared_mutex map_mutex;
 *
 * // Get or create entry
 * Data* data = hot_path::get_or_create(
 *     data_map,
 *     map_mutex,
 *     "key1",
 *     []() { return std::make_unique<Data>(); }
 * );
 *
 * // data is now valid and can be used
 * @endcode
 */
template <typename Map, typename Key, typename CreateFn>
auto get_or_create(Map& map, std::shared_mutex& mutex, const Key& key,
                   CreateFn create_fn) ->
    typename std::remove_reference<decltype(*map.begin()->second)>::type* {
    // Fast path: try with shared lock (read-only, allows concurrent readers)
    {
        std::shared_lock<std::shared_mutex> read_lock(mutex);
        auto it = map.find(key);
        if (it != map.end()) {
            return it->second.get();
        }
    }

    // Slow path: upgrade to write lock and double-check
    {
        std::unique_lock<std::shared_mutex> write_lock(mutex);
        // Double-check after acquiring write lock
        // Another thread may have created the entry while we were waiting
        auto& ptr = map[key];
        if (!ptr) {
            ptr = create_fn();
        }
        return ptr.get();
    }
}

/**
 * @brief Get existing value or create new one with initialization callback
 *
 * Similar to get_or_create but additionally allows running an initialization
 * function only when a new entry is created. Useful when the value needs
 * additional setup beyond what the factory function provides.
 *
 * @tparam Map Container type supporting find(), operator[], and emplace()
 * @tparam Key Key type for the container
 * @tparam CreateFn Callable returning the value to insert if key not found
 * @tparam InitFn Callable taking a reference to the value type for initialization
 *
 * @param map The container to access
 * @param mutex The shared_mutex protecting the container
 * @param key The key to look up or create
 * @param create_fn Factory function called when creating a new entry
 * @param init_fn Function called to initialize newly created entry (called under write lock)
 *
 * @return Pointer to the value (existing or newly created)
 *
 * @thread_safety Thread-safe. init_fn is called under write lock.
 *
 * @example
 * @code
 * std::unordered_map<std::string, std::unique_ptr<MetricData>> metrics;
 * std::shared_mutex metrics_mutex;
 *
 * // Get or create with initialization
 * auto* data = hot_path::get_or_create_with_init(
 *     metrics,
 *     metrics_mutex,
 *     key,
 *     []() { return std::make_unique<MetricData>(); },
 *     [&](MetricData& d) {
 *         d.type = metric_type;
 *         d.tags = tags;
 *     }
 * );
 * @endcode
 */
template <typename Map, typename Key, typename CreateFn, typename InitFn>
auto get_or_create_with_init(Map& map, std::shared_mutex& mutex, const Key& key,
                             CreateFn create_fn, InitFn init_fn) ->
    typename std::remove_reference<decltype(*map.begin()->second)>::type* {
    // Fast path: try with shared lock (read-only, allows concurrent readers)
    {
        std::shared_lock<std::shared_mutex> read_lock(mutex);
        auto it = map.find(key);
        if (it != map.end()) {
            return it->second.get();
        }
    }

    // Slow path: upgrade to write lock and double-check
    {
        std::unique_lock<std::shared_mutex> write_lock(mutex);
        // Double-check after acquiring write lock
        // Another thread may have created the entry while we were waiting
        auto& ptr = map[key];
        if (!ptr) {
            ptr = create_fn();
            // Initialize the newly created entry while still under write lock
            init_fn(*ptr);
        }
        return ptr.get();
    }
}

/**
 * @brief Get existing value or create new one, then apply an update function
 *
 * Similar to get_or_create but additionally applies an update function to the
 * value after retrieval. Useful when you need to both ensure the entry exists
 * and perform an operation on it atomically with respect to creation.
 *
 * @tparam Map Container type supporting find(), operator[], and emplace()
 * @tparam Key Key type for the container
 * @tparam CreateFn Callable returning the value to insert if key not found
 * @tparam UpdateFn Callable taking a reference to the value type
 *
 * @param map The container to access
 * @param mutex The shared_mutex protecting the container
 * @param key The key to look up or create
 * @param create_fn Factory function called when creating a new entry
 * @param update_fn Function applied to the value after retrieval
 *
 * @return The result of update_fn
 *
 * @thread_safety Thread-safe. Uses shared_lock for reads, unique_lock for writes.
 *
 * @note The update_fn is called outside of any lock, so additional
 *       synchronization may be needed if the value has its own threading
 *       requirements.
 *
 * @example
 * @code
 * std::unordered_map<std::string, std::unique_ptr<Counter>> counters;
 * std::shared_mutex counters_mutex;
 *
 * // Get or create and increment
 * hot_path::get_or_create_and_update(
 *     counters,
 *     counters_mutex,
 *     "requests",
 *     []() { return std::make_unique<Counter>(); },
 *     [](Counter& c) { c.increment(); }
 * );
 * @endcode
 */
template <typename Map, typename Key, typename CreateFn, typename UpdateFn>
auto get_or_create_and_update(Map& map, std::shared_mutex& mutex, const Key& key,
                              CreateFn create_fn, UpdateFn update_fn)
    -> decltype(update_fn(*map.begin()->second)) {
    // Get or create the entry
    auto* ptr = get_or_create(map, mutex, key, std::move(create_fn));

    // Apply the update function outside of map locks
    // The value's own synchronization handles thread safety
    return update_fn(*ptr);
}

}  // namespace hot_path
}  // namespace monitoring
}  // namespace kcenon
