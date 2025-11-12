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

/**
 * @file thread_context.cpp
 * @brief Thread context implementation
 */

#include <kcenon/monitoring/context/thread_context.h>
#include <random>
#include <sstream>
#include <iomanip>

namespace kcenon { namespace monitoring {

// New thread_context implementation
thread_local std::unique_ptr<thread_context_data> thread_context::current_context_;

thread_context_data& thread_context::create(const std::string& request_id) {
    if (request_id.empty()) {
        current_context_ = std::make_unique<thread_context_data>(generate_request_id());
    } else {
        current_context_ = std::make_unique<thread_context_data>(request_id);
    }
    return *current_context_;
}

thread_context_data* thread_context::current() {
    return current_context_.get();
}

bool thread_context::has_context() {
    return current_context_ != nullptr;
}

void thread_context::clear() {
    current_context_.reset();
}

bool thread_context::copy_from(const thread_context_data& source) {
    current_context_ = std::make_unique<thread_context_data>(source);
    return true;
}

// Legacy thread_context_manager implementation
thread_local std::optional<thread_context_data> thread_context_manager::current_context_;

void thread_context_manager::set_context(const thread_context_data& context) {
    current_context_ = context;
}

std::optional<thread_context_data> thread_context_manager::get_context() {
    return current_context_;
}

void thread_context_manager::clear_context() {
    current_context_.reset();
}

std::string thread_context::generate_request_id() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_int_distribution<uint64_t> dis;
    auto id = dis(gen);
    std::stringstream ss;
    ss << std::hex << id;
    return ss.str();
}

std::string thread_context::generate_correlation_id() {
    return generate_request_id(); // Simple implementation for now
}

std::string thread_context_manager::generate_request_id() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_int_distribution<uint64_t> dis;

    auto id = dis(gen);
    std::stringstream ss;
    ss << std::hex << id;
    return ss.str();
}

std::string thread_context_manager::generate_correlation_id() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_int_distribution<uint64_t> dis;

    auto id1 = dis(gen);
    auto id2 = dis(gen);
    std::stringstream ss;
    ss << std::hex << id1 << "-" << std::hex << id2;
    return ss.str();
}

} } // namespace kcenon::monitoring