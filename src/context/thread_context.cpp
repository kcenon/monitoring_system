/**
 * @file thread_context.cpp
 * @brief Thread context implementation
 */

#include <kcenon/monitoring/context/thread_context.h>
#include <random>
#include <sstream>
#include <iomanip>

namespace kcenon::monitoring {

thread_local std::optional<thread_context> thread_context_manager::current_context_;

void thread_context_manager::set_context(const thread_context& context) {
    current_context_ = context;
}

std::optional<thread_context> thread_context_manager::get_context() {
    return current_context_;
}

void thread_context_manager::clear_context() {
    current_context_.reset();
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

} // namespace kcenon::monitoring