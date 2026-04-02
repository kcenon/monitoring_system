// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once
#include <string>
#include <chrono>
#include <optional>

namespace kcenon { namespace monitoring {

struct trace_context {
    std::string trace_id;
    std::string span_id;
    std::optional<std::string> parent_span_id;
    std::chrono::system_clock::time_point start_time;
    
    static trace_context create_root(const std::string& operation) {
        return {generate_id(), generate_id(), std::nullopt, 
                std::chrono::system_clock::now()};
    }
    
    trace_context create_child(const std::string& operation) const {
        return {trace_id, generate_id(), span_id,
                std::chrono::system_clock::now()};
    }
    
private:
    static std::string generate_id();
};

} // namespace
