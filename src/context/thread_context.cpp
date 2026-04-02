// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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


std::string thread_context::generate_request_id() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_int_distribution<uint64_t> dis;
    auto id = dis(gen);
    std::stringstream ss;
    // W3C span_id requires 16 hex characters
    ss << std::hex << std::setfill('0') << std::setw(16) << id;
    return ss.str();
}

std::string thread_context::generate_correlation_id() {
    static thread_local std::random_device rd;
    static thread_local std::mt19937 gen(rd());
    static thread_local std::uniform_int_distribution<uint64_t> dis;
    auto id1 = dis(gen);
    auto id2 = dis(gen);
    std::stringstream ss;
    // W3C trace_id requires 32 hex characters (no dashes)
    ss << std::hex << std::setfill('0') << std::setw(16) << id1
       << std::setfill('0') << std::setw(16) << id2;
    return ss.str();
}

} } // namespace kcenon::monitoring