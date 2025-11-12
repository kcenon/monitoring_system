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