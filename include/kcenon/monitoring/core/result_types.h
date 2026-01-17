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
 * This file provides monitoring-specific error types that integrate with
 * common_system's Result implementation. For Result types, use common::Result<T>
 * and common::VoidResult directly from <kcenon/common/patterns/result.h>.
 *
 * @see common::Result<T> for the standard Result type
 * @see common::VoidResult for void results
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

} } // namespace kcenon::monitoring
