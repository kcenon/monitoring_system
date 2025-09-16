#pragma once

// Retry policy implementation - stub for compatibility
#include <chrono>
#include <functional>

namespace monitoring_system {

/**
 * @brief Basic retry policy implementation - stub
 */
class retry_policy {
public:
    struct config {
        size_t max_attempts = 3;
        std::chrono::milliseconds base_delay = std::chrono::milliseconds(1000);
        double backoff_multiplier = 2.0;
    };

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

} // namespace monitoring_system