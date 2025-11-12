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

// Fault tolerance manager - stub for compatibility
#include "circuit_breaker.h"
#include "retry_policy.h"
#include "error_boundary.h"

namespace kcenon::monitoring {

/**
 * @brief Basic fault tolerance manager - stub implementation
 */
class fault_tolerance_manager {
public:
    fault_tolerance_manager() = default;

    template<typename Func>
    auto execute_with_fault_tolerance(Func&& func) -> decltype(func()) {
        // Stub implementation - direct execution
        return func();
    }

    void configure_circuit_breaker(const circuit_breaker::config& cfg) {
        circuit_breaker_ = std::make_unique<circuit_breaker>(cfg);
    }

    void configure_retry_policy(const retry_policy::config& cfg) {
        retry_policy_ = std::make_unique<retry_policy>(cfg);
    }

private:
    std::unique_ptr<circuit_breaker> circuit_breaker_;
    std::unique_ptr<retry_policy> retry_policy_;
    std::unique_ptr<error_boundary> error_boundary_;
};

} // namespace kcenon::monitoring