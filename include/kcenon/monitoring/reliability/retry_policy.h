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

// Retry policy implementation - stub for compatibility
#include <chrono>
#include <functional>

namespace kcenon::monitoring {

/**
 * @brief Retry strategies
 */
enum class retry_strategy {
    fixed_delay,
    exponential_backoff,
    linear_backoff
};

/**
 * @brief Retry configuration
 */
struct retry_config {
    size_t max_attempts = 3;
    retry_strategy strategy = retry_strategy::exponential_backoff;
    std::chrono::milliseconds initial_delay = std::chrono::milliseconds(1000);
    std::chrono::milliseconds max_delay = std::chrono::milliseconds(30000);
    double backoff_multiplier = 2.0;
};

/**
 * @brief Basic retry policy implementation - stub
 */
class retry_policy {
public:
    using config = retry_config;

    retry_policy() : config_() {}
    explicit retry_policy(const config& cfg) : config_(cfg) {}

    template<typename Func>
    auto execute(Func&& func) -> decltype(func()) {
        // Stub implementation - single attempt
        return func();
    }

private:
    config config_;
};

} // namespace kcenon::monitoring