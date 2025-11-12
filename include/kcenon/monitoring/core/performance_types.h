// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
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
