#pragma once

#include <cstdint>
#include <string>

namespace kcenon { namespace monitoring {

/**
 * @brief Lightweight performance profile for aggregated metrics
 *
 * This is a simplified profile structure used by the central collector
 * for efficient metric aggregation. For detailed percentile analysis,
 * use performance_metrics.
 */
struct performance_profile {
    std::string operation_name;

    // Call statistics
    std::uint64_t total_calls{0};
    std::uint64_t error_count{0};

    // Duration statistics (in nanoseconds)
    std::int64_t total_duration_ns{0};
    std::int64_t min_duration_ns{INT64_MAX};
    std::int64_t max_duration_ns{0};
    std::int64_t avg_duration_ns{0};

    /**
     * @brief Calculate success rate
     * @return Success rate as percentage (0-100)
     */
    double success_rate() const {
        if (total_calls == 0) return 100.0;
        return 100.0 * (total_calls - error_count) / total_calls;
    }

    /**
     * @brief Calculate error rate
     * @return Error rate as percentage (0-100)
     */
    double error_rate() const {
        if (total_calls == 0) return 0.0;
        return 100.0 * error_count / total_calls;
    }
};

}} // namespace kcenon::monitoring
