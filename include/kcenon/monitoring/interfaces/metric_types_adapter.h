// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file metric_types_adapter.h
 * @brief Adapter for metric types to support interface definitions
 *
 * This file provides type definitions to bridge the gap between
 * the interface definitions and the actual metric implementation.
 */

#include "../utils/metric_types.h"
#include <string>
#include <unordered_map>
#include <variant>

namespace kcenon { namespace monitoring {

/**
 * @struct metric
 * @brief Basic metric structure for interface compatibility
 *
 * A generic metric container that can hold different value types
 * (double, int64_t, or string) along with metadata tags and type
 * information. Supports conversion to compact representation for
 * efficient storage.
 *
 * @thread_safety This struct is NOT thread-safe. External synchronization
 *                is required when accessed from multiple threads.
 *
 * @example
 * @code
 * // Create a gauge metric with tags
 * metric cpu_usage("cpu_usage_percent", 75.5,
 *                  {{"host", "server1"}, {"core", "0"}},
 *                  metric_type::gauge);
 *
 * // Create a counter metric
 * metric requests("total_requests", int64_t(12345),
 *                 {{"endpoint", "/api/users"}},
 *                 metric_type::counter);
 *
 * // Convert to compact representation for storage
 * auto compact = cpu_usage.to_compact();
 * @endcode
 *
 * @see compact_metric_value for space-efficient representation
 * @see metric_type for metric type classifications
 */
struct metric {
    std::string name;
    std::variant<double, int64_t, std::string> value;
    std::unordered_map<std::string, std::string> tags;
    metric_type type{metric_type::gauge};
    std::chrono::system_clock::time_point timestamp;

    metric() : timestamp(std::chrono::system_clock::now()) {}

    metric(const std::string& n, const std::variant<double, int64_t, std::string>& v,
           const std::unordered_map<std::string, std::string>& t,
           metric_type mt = metric_type::gauge)
        : name(n), value(v), tags(t), type(mt),
          timestamp(std::chrono::system_clock::now()) {}

    // Convert to compact representation
    compact_metric_value to_compact() const {
        // Simple hash function for name
        uint32_t hash = 0;
        for (char c : name) {
            hash = hash * 31 + static_cast<uint32_t>(c);
        }

        metric_metadata meta(hash, type, static_cast<uint8_t>(tags.size()));

        if (std::holds_alternative<double>(value)) {
            return compact_metric_value(meta, std::get<double>(value));
        } else if (std::holds_alternative<int64_t>(value)) {
            return compact_metric_value(meta, std::get<int64_t>(value));
        } else {
            return compact_metric_value(meta, std::get<std::string>(value));
        }
    }
};

/**
 * @struct metric_stats
 * @brief Statistics about metric collection
 *
 * Tracks collection performance metrics including success/error counts,
 * dropped metrics, and timing information. Useful for monitoring the
 * health of the collection system itself.
 *
 * @thread_safety This struct is NOT thread-safe. Use atomic counters
 *                or external synchronization for concurrent updates.
 *
 * @example
 * @code
 * metric_stats stats;
 * stats.total_collected = 1000;
 * stats.total_errors = 5;
 * stats.total_dropped = 2;
 * stats.avg_collection_time = std::chrono::milliseconds(15);
 * stats.last_collection = std::chrono::system_clock::now();
 *
 * // Calculate success rate
 * double rate = stats.success_rate();  // 0.995
 *
 * // Reset after reporting
 * stats.reset();
 * @endcode
 *
 * @see interface_metric_collector::get_stats for retrieving statistics
 */
struct metric_stats {
    uint64_t total_collected{0};
    uint64_t total_errors{0};
    uint64_t total_dropped{0};
    std::chrono::milliseconds avg_collection_time{0};
    std::chrono::system_clock::time_point last_collection;

    double success_rate() const {
        if (total_collected == 0) return 0.0;
        return 1.0 - (static_cast<double>(total_errors) / total_collected);
    }

    void reset() {
        total_collected = 0;
        total_errors = 0;
        total_dropped = 0;
        avg_collection_time = std::chrono::milliseconds{0};
    }
};

} } // namespace kcenon::monitoring