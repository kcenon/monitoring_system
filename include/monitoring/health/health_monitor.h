#pragma once

// Health monitoring interface - basic implementation for compatibility
#include <string>
#include <unordered_map>
#include <chrono>

namespace monitoring_system {

/**
 * @brief Basic health status enumeration
 */
enum class health_status {
    healthy,
    degraded,
    unhealthy,
    unknown
};

/**
 * @brief Health check result
 */
struct health_check_result {
    health_status status{health_status::unknown};
    std::string message;
    std::chrono::steady_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> details;
};

/**
 * @brief Basic health monitor interface - stub implementation
 */
class health_monitor {
public:
    health_monitor() = default;
    virtual ~health_monitor() = default;

    virtual health_check_result check_health() const {
        health_check_result result;
        result.status = health_status::healthy;
        result.message = "Health monitor stub - always healthy";
        result.timestamp = std::chrono::steady_clock::now();
        return result;
    }

    virtual void start() {}
    virtual void stop() {}
    virtual bool is_running() const { return true; }
};

} // namespace monitoring_system