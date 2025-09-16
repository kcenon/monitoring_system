#pragma once

// Circuit breaker implementation - stub for compatibility
#include <atomic>
#include <chrono>
#include <functional>

namespace monitoring_system {

/**
 * @brief Circuit breaker states
 */
enum class circuit_state {
    closed,
    open,
    half_open
};

/**
 * @brief Basic circuit breaker implementation - stub
 */
class circuit_breaker {
public:
    struct config {
        size_t failure_threshold = 5;
        std::chrono::milliseconds timeout = std::chrono::milliseconds(60000);
        size_t success_threshold = 3;
    };

    circuit_breaker() : config_() {}
    explicit circuit_breaker(const config& cfg) : config_(cfg) {}

    template<typename Func>
    auto execute(Func&& func) -> decltype(func()) {
        // Stub implementation - always execute
        return func();
    }

    circuit_state get_state() const { return circuit_state::closed; }
    size_t get_failure_count() const { return 0; }

private:
    config config_;
    std::atomic<size_t> failure_count_{0};
    std::atomic<circuit_state> state_{circuit_state::closed};
};

} // namespace monitoring_system