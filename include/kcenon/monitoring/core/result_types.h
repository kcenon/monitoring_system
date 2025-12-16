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
 *
 * @note MIGRATION NOTICE: This file provides backward-compatible aliases
 * for the transition to common::Result<T>. New code should use common::Result<T>
 * directly from <kcenon/common/patterns/result.h>.
 *
 * Deprecated aliases (use common:: equivalents instead):
 * - result<T>     -> common::Result<T>
 * - result_void   -> common::VoidResult
 * - make_success  -> common::ok
 * - make_error    -> common::make_error
 *
 * @see https://github.com/kcenon/common_system/issues/205
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

// ============================================================================
// DEPRECATED TYPE ALIASES
// ============================================================================
// These aliases are provided for backward compatibility during migration.
// New code should use common::Result<T> and common::VoidResult directly.
// ============================================================================

/**
 * @brief Backward-compatible alias for common::Result<T>
 * @deprecated Use common::Result<T> directly instead
 *
 * Migration example:
 * @code
 * // Old code:
 * result<int> my_function();
 *
 * // New code:
 * common::Result<int> my_function();
 * @endcode
 */
template<typename T>
using result = common::Result<T>;

/**
 * @brief Backward-compatible alias for common::VoidResult
 * @deprecated Use common::VoidResult directly instead
 *
 * Migration example:
 * @code
 * // Old code:
 * result_void my_function();
 *
 * // New code:
 * common::VoidResult my_function();
 * @endcode
 */
using result_void = common::VoidResult;

// ============================================================================
// DEPRECATED HELPER FUNCTIONS
// ============================================================================
// These functions are provided for backward compatibility during migration.
// New code should use common::ok() and common::make_error() directly.
// ============================================================================

/**
 * @brief Create a successful result
 * @deprecated Use common::ok() directly instead
 * @tparam T The value type
 * @param value The success value
 * @return result<T> containing the value
 *
 * Migration example:
 * @code
 * // Old code:
 * return make_success<int>(42);
 *
 * // New code:
 * return common::ok(42);
 * @endcode
 */
template<typename T>
common::Result<std::decay_t<T>> make_success(T&& value) {
    return common::ok<std::decay_t<T>>(std::forward<T>(value));
}

/**
 * @brief Create an error result
 * @deprecated Use common::make_error() directly instead
 * @tparam T The expected value type
 * @param code The error code
 * @param message Optional error message
 * @return result<T> containing the error
 *
 * Migration example:
 * @code
 * // Old code:
 * return make_error<int>(monitoring_error_code::invalid_configuration, "error");
 *
 * // New code:
 * return common::make_error<int>(
 *     static_cast<int>(monitoring_error_code::invalid_configuration),
 *     "error", "monitoring_system");
 * @endcode
 */
template<typename T>
common::Result<T> make_error(monitoring_error_code code,
                             const std::string& message = "") {
    error_info err(code, message);
    return common::Result<T>::err(err.to_common_error());
}

/**
 * @brief Create a VoidResult with error
 * @deprecated Use common::VoidResult::err() directly instead
 */
inline common::VoidResult make_result_void(monitoring_error_code code,
                                           const std::string& message = "",
                                           const std::optional<std::string>& context = std::nullopt) {
    error_info err(code, message, context);
    return common::VoidResult::err(err.to_common_error());
}

/**
 * @brief Create an error result with context
 * @deprecated Use common::make_error() with details parameter instead
 * @tparam T The expected value type
 * @param code The error code
 * @param message Error message
 * @param context Additional context
 * @return result<T> containing the error with context
 */
template<typename T>
common::Result<T> make_error_with_context(monitoring_error_code code,
                                          const std::string& message,
                                          const std::string& context) {
    error_info err(code, message, context);
    return common::Result<T>::err(err.to_common_error());
}

/**
 * @brief Helper function to create VoidResult with error
 * @deprecated Use common::VoidResult::err() directly instead
 * @param code The error code
 * @param message Error message
 * @return result_void containing the error
 */
inline common::VoidResult make_void_error(monitoring_error_code code,
                                          const std::string& message = "") {
    error_info err(code, message);
    return common::VoidResult::err(err.to_common_error());
}

/**
 * @brief Helper function to create successful VoidResult
 * @deprecated Use common::ok() directly instead
 * @return result_void indicating success
 *
 * Migration example:
 * @code
 * // Old code:
 * return make_void_success();
 *
 * // New code:
 * return common::ok();
 * @endcode
 */
inline common::VoidResult make_void_success() {
    return common::VoidResult(std::monostate{});
}

// ============================================================================
// DEPRECATED MACROS
// ============================================================================
// These macros are provided for backward compatibility during migration.
// New code should use COMMON_RETURN_IF_ERROR and COMMON_ASSIGN_OR_RETURN
// from <kcenon/common/patterns/result.h> directly.
// ============================================================================

/**
 * @def MONITORING_TRY
 * @deprecated Use COMMON_RETURN_IF_ERROR directly instead
 */
#define MONITORING_TRY(expr) COMMON_RETURN_IF_ERROR(expr)

/**
 * @def MONITORING_TRY_ASSIGN
 * @deprecated Use COMMON_ASSIGN_OR_RETURN directly instead
 */
#define MONITORING_TRY_ASSIGN(var, expr) \
    auto _result_##var = (expr); \
    if (_result_##var.is_err()) { \
        return decltype(_result_##var)::err(_result_##var.error()); \
    } \
    auto& var = _result_##var.value();

} } // namespace kcenon::monitoring
