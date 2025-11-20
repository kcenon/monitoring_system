#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file result_types.h
 * @brief Result pattern type definitions for monitoring system
 *
 * This file provides Result pattern types using common_system's Result
 * implementation, ensuring consistent error management across the ecosystem.
 */

#include "error_codes.h"
#include <kcenon/common/patterns/result.h>

#include <optional>
#include <string>

namespace kcenon { namespace monitoring {

/**
 * @struct error_info
 * @brief Extended error information with context
 *
 * Provides monitoring-specific error information that integrates
 * with common_system's error handling.
 */
struct error_info {
    monitoring_error_code code;
    std::string message;
    std::optional<std::string> context{std::nullopt};

    error_info(monitoring_error_code c,
               const std::string& msg = "",
               const std::optional<std::string>& ctx = std::nullopt)
        : code(c)
        , message(msg.empty() ? error_code_to_string(c) : msg)
        , context(ctx) {}

    /**
     * @brief Get formatted error string
     * @return Formatted error message with context information
     */
    std::string to_string() const {
        std::string result = "[" + error_code_to_string(code) + "] " + message;
        if (context.has_value()) {
            result += " Context: " + context.value();
        }
        return result;
    }

    /**
     * @brief Convert to common_system error_info
     */
    common::error_info to_common_error() const {
        common::error_info info(static_cast<int>(code), message, "monitoring_system");
        if (context) {
            info.details = context;
        }
        return info;
    }

    /**
     * @brief Create from common_system error_info
     */
    static error_info from_common_error(const common::error_info& common_err) {
        error_info info(static_cast<monitoring_error_code>(common_err.code),
                       common_err.message);
        if (common_err.details) {
            info.context = common_err.details;
        }
        return info;
    }
};

// Type aliases for common::Result usage
template<typename T>
using result = common::Result<T>;

using result_void = common::VoidResult;

// Helper functions for creating results

/**
 * @brief Create a successful result
 * @tparam T The value type
 * @param value The success value
 * @return result<T> containing the value
 */
template<typename T>
result<std::decay_t<T>> make_success(T&& value) {
    return common::ok<std::decay_t<T>>(std::forward<T>(value));
}

/**
 * @brief Create an error result
 * @tparam T The expected value type
 * @param code The error code
 * @param message Optional error message
 * @return result<T> containing the error
 */
template<typename T>
result<T> make_error(monitoring_error_code code,
                     const std::string& message = "") {
    error_info err(code, message);
    return result<T>::err(err.to_common_error());
}

inline result_void make_result_void(monitoring_error_code code,
                                    const std::string& message = "",
                                    const std::optional<std::string>& context = std::nullopt) {
    error_info err(code, message, context);
    return result_void::err(err.to_common_error());
}

/**
 * @brief Create an error result with context
 * @tparam T The expected value type
 * @param code The error code
 * @param message Error message
 * @param context Additional context
 * @return result<T> containing the error with context
 */
template<typename T>
result<T> make_error_with_context(monitoring_error_code code,
                                  const std::string& message,
                                  const std::string& context) {
    error_info err(code, message, context);
    return result<T>::err(err.to_common_error());
}

/**
 * @brief Helper function to create VoidResult with error
 * @param code The error code
 * @param message Error message
 * @return result_void containing the error
 */
inline result_void make_void_error(monitoring_error_code code,
                                   const std::string& message = "") {
    error_info err(code, message);
    return result_void::err(err.to_common_error());
}

/**
 * @brief Helper function to create successful VoidResult
 * @return result_void indicating success
 */
inline result_void make_void_success() {
    return result_void(std::monostate{});
}

// Macros for propagating errors (compatible with common::Result)
#define MONITORING_TRY(expr) COMMON_RETURN_IF_ERROR(expr)

#define MONITORING_TRY_ASSIGN(var, expr) \
    auto _result_##var = (expr); \
    if (_result_##var.is_err()) { \
        return decltype(_result_##var)::err(_result_##var.error()); \
    } \
    auto& var = _result_##var.value();

} } // namespace kcenon::monitoring
