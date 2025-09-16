#pragma once

#include <string>
#include <chrono>
#include <optional>

namespace monitoring_system {

/**
 * @brief Thread context information for distributed tracing
 */
struct thread_context {
    std::string thread_id;
    std::string span_id;
    std::string trace_id;
    std::string correlation_id;
    std::chrono::steady_clock::time_point start_time;
    std::optional<std::string> parent_span_id;

    thread_context() = default;

    thread_context(std::string tid, std::string sid, std::string trid)
        : thread_id(std::move(tid))
        , span_id(std::move(sid))
        , trace_id(std::move(trid))
        , start_time(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Thread-local context storage
 */
class thread_context_manager {
public:
    static void set_context(const thread_context& context);
    static std::optional<thread_context> get_context();
    static void clear_context();
    static std::string generate_request_id();
    static std::string generate_correlation_id();

private:
    static thread_local std::optional<thread_context> current_context_;
};

} // namespace monitoring_system