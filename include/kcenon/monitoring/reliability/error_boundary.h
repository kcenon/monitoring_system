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

// Error boundary implementation - stub for compatibility
#include <chrono>
#include <functional>
#include <string>
#include <atomic>
#include "kcenon/monitoring/core/result_types.h"

namespace kcenon::monitoring {

/**
 * @brief Degradation levels
 */
enum class degradation_level {
    none = 0,
    low = 1,
    medium = 2,
    high = 3,
    critical = 4
};

/**
 * @brief Error boundary configuration
 */
struct error_boundary_config {
    size_t error_threshold = 5;
    std::chrono::seconds error_window = std::chrono::seconds(60);
    bool enable_fallback_logging = true;
    degradation_level max_degradation = degradation_level::high;
};

/**
 * @brief Error boundary metrics
 */
struct error_boundary_metrics {
    size_t total_operations = 0;
    size_t failed_operations = 0;
    size_t recovered_operations = 0;
};

/**
 * @brief Basic error boundary implementation - stub
 */
template<typename T = void>
class error_boundary {
public:
    using config = error_boundary_config;

    error_boundary() : config_() {}
    explicit error_boundary(const std::string& name) : name_(name), config_() {}
    explicit error_boundary(const std::string& name, const config& cfg) : name_(name), config_(cfg) {}

    template<typename Func>
    auto execute(Func&& func) -> decltype(func()) {
        metrics_.total_operations++;
        try {
            return func();
        } catch (...) {
            metrics_.failed_operations++;
            throw;
        }
    }

    void set_error_handler(std::function<void(const error_info&, degradation_level)> handler) {
        error_handler_ = handler;
    }

    error_boundary_metrics get_metrics() const {
        return metrics_;
    }

private:
    std::string name_;
    config config_;
    std::function<void(const error_info&, degradation_level)> error_handler_;
    mutable error_boundary_metrics metrics_;
};

} // namespace kcenon::monitoring