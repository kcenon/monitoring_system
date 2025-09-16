#pragma once

// Error boundary implementation - stub for compatibility
#include <functional>
#include <exception>

namespace monitoring_system {

/**
 * @brief Basic error boundary implementation - stub
 */
class error_boundary {
public:
    using error_handler = std::function<void(const std::exception&)>;

    explicit error_boundary(error_handler handler = nullptr)
        : error_handler_(std::move(handler)) {}

    template<typename Func>
    auto execute(Func&& func) -> decltype(func()) {
        try {
            return func();
        } catch (const std::exception& e) {
            if (error_handler_) {
                error_handler_(e);
            }
            throw; // Re-throw for now
        }
    }

private:
    error_handler error_handler_;
};

} // namespace monitoring_system