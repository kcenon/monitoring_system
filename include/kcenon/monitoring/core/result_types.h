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
 * This file provides Result pattern types that integrate with thread_system's
 * error handling approach, ensuring consistent error management across the
 * monitoring system.
 */

#include "error_codes.h"

// Try to include common system's result.h if available
#if __has_include(<kcenon/common/patterns/result.h>)
    #include <kcenon/common/patterns/result.h>
    #define MONITORING_HAS_COMMON_RESULT 1
#elif __has_include(<common/patterns/result.h>)
    #include <common/patterns/result.h>
    #define MONITORING_HAS_COMMON_RESULT 1
    // Create namespace alias for compatibility
    namespace kcenon::common = ::common;
#else
    #define MONITORING_HAS_COMMON_RESULT 0
#endif

#include <optional>
#include <memory>
#include <string>
#include <type_traits>
#include <stdexcept>
#include <utility>
#include <variant>

// Check for source_location support
#if __has_include(<source_location>) && defined(__cpp_lib_source_location)
    #include <source_location>
    #define MONITORING_HAS_SOURCE_LOCATION 1
#else
    #define MONITORING_HAS_SOURCE_LOCATION 0
#endif

namespace monitoring_system {

/**
 * @struct error_info
 * @brief Extended error information with context
 */
struct error_info {
    monitoring_error_code code;
    std::string message;
#if MONITORING_HAS_SOURCE_LOCATION
    std::source_location location;
#else
    const char* file = "";
    int line = 0;
#endif
    std::optional<std::string> context;

#if MONITORING_HAS_SOURCE_LOCATION
    error_info(monitoring_error_code c,
               const std::string& msg = "",
               const std::source_location& loc = std::source_location::current(),
               const std::optional<std::string>& ctx = std::nullopt)
        : code(c)
        , message(msg.empty() ? error_code_to_string(c) : msg)
        , location(loc)
        , context(ctx) {}
#else
    error_info(monitoring_error_code c,
               const std::string& msg = "",
               const char* f = __builtin_FILE(),
               int l = __builtin_LINE(),
               const std::optional<std::string>& ctx = std::nullopt)
        : code(c)
        , message(msg.empty() ? error_code_to_string(c) : msg)
        , file(f)
        , line(l)
        , context(ctx) {}
#endif

    /**
     * @brief Get formatted error string
     * @return Formatted error message with location information
     */
    std::string to_string() const {
        std::string result = "[" + error_code_to_string(code) + "] " + message;
#if MONITORING_HAS_SOURCE_LOCATION
        result += " (at " + std::string(location.file_name()) + ":" +
                  std::to_string(location.line()) + ")";
#else
        if (file && file[0] != '\0') {
            result += " (at " + std::string(file) + ":" + std::to_string(line) + ")";
        }
#endif
        if (context.has_value()) {
            result += " Context: " + context.value();
        }
        return result;
    }
};

#if MONITORING_HAS_COMMON_RESULT
namespace detail {

inline common::error_info to_common_error(const error_info& error) {
    common::error_info info(static_cast<int>(error.code), error.message, "monitoring_system");
    if (error.context) {
        info.details = error.context;
    }
    return info;
}

} // namespace detail
#endif

/**
 * @class result
 * @brief Result type for operations that may fail
 * @tparam T The success value type
 * 
 * This class follows the Result pattern used in thread_system,
 * providing explicit error handling without exceptions.
 */
template<typename T>
class result {
public:
#if MONITORING_HAS_COMMON_RESULT
    result(T&& value)
        : value_(common::ok<T>(std::forward<T>(value))), error_(std::nullopt) {}

    result(const T& value)
        : value_(common::ok<T>(value)), error_(std::nullopt) {}

    result(error_info&& error)
        : value_(detail::to_common_error(error)), error_(std::move(error)) {}

    result(const error_info& error)
        : value_(detail::to_common_error(error)), error_(error) {}
#else
    result(T&& value)
        : value_(std::forward<T>(value)) {}

    result(const T& value)
        : value_(value) {}

    result(error_info&& error)
        : value_(std::move(error)) {}

    result(const error_info& error)
        : value_(error) {}
#endif

#if MONITORING_HAS_SOURCE_LOCATION
    result(monitoring_error_code code,
           const std::string& message = "",
           const std::source_location& loc = std::source_location::current())
        : result(error_info(code, message, loc)) {}
#else
    result(monitoring_error_code code,
           const std::string& message = "")
        : result(error_info(code, message)) {}
#endif

#if MONITORING_HAS_COMMON_RESULT
    bool has_value() const { return common::is_ok(value_); }
#else
    bool has_value() const { return std::holds_alternative<T>(value_); }
#endif

    operator bool() const { return has_value(); }

    T& value() {
        ensure_value();
#if MONITORING_HAS_COMMON_RESULT
        return common::get_value(value_);
#else
        return std::get<T>(value_);
#endif
    }

    const T& value() const {
        ensure_value();
#if MONITORING_HAS_COMMON_RESULT
        return common::get_value(value_);
#else
        return std::get<T>(value_);
#endif
    }

    T value_or(T&& default_value) const {
#if MONITORING_HAS_COMMON_RESULT
        return has_value() ? common::get_value(value_) : std::forward<T>(default_value);
#else
        return has_value() ? std::get<T>(value_) : std::forward<T>(default_value);
#endif
    }

    const error_info& get_error() const {
#if MONITORING_HAS_COMMON_RESULT
        if (!error_) {
            const auto& common_error = common::get_error(value_);
            error_.emplace(static_cast<monitoring_error_code>(common_error.code),
                           common_error.message);
            if (common_error.details) {
                error_->context = common_error.details;
            }
        }
        return *error_;
#else
        return std::get<error_info>(value_);
#endif
    }

    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }

    T& operator*() { return value(); }
    const T& operator*() const { return value(); }

    template<typename F>
    auto map(F&& f) -> result<std::decay_t<decltype(f(std::declval<T>()))>> {
        using U = std::decay_t<decltype(f(std::declval<T>()))>;
        if (has_value()) {
            return result<U>(f(value()));
        }
        return result<U>(get_error());
    }

    template<typename F>
    auto and_then(F&& f) -> decltype(f(std::declval<T>())) {
        using U = decltype(f(std::declval<T>()));
        if (has_value()) {
            return f(value());
        }
        return U(get_error());
    }

    template<typename F>
    result or_else(F&& f) {
        if (!has_value()) {
            return f(get_error());
        }
        return *this;
    }

#if MONITORING_HAS_COMMON_RESULT
    const common::Result<T>& raw() const { return value_; }
#endif

private:
    void ensure_value() const {
        if (!has_value()) {
            throw std::runtime_error(get_error().to_string());
        }
    }

#if MONITORING_HAS_COMMON_RESULT
    common::Result<T> value_;
    mutable std::optional<error_info> error_;
#else
    std::variant<T, error_info> value_;
#endif
};

/**
 * @class result_void
 * @brief Specialization for operations with no return value
 */
class result_void {
public:
#if MONITORING_HAS_COMMON_RESULT
    result_void() : value_(std::monostate{}), error_(std::nullopt) {}
#else
    result_void() : error_(std::nullopt) {}
#endif

#if MONITORING_HAS_COMMON_RESULT
    result_void(error_info&& error)
        : value_(detail::to_common_error(error)), error_(std::move(error)) {}

    result_void(const error_info& error)
        : value_(detail::to_common_error(error)), error_(error) {}
#else
    result_void(error_info&& error)
        : error_(std::move(error)) {}

    result_void(const error_info& error)
        : error_(error) {}
#endif

#if MONITORING_HAS_SOURCE_LOCATION
    result_void(monitoring_error_code code,
                const std::string& message = "",
                const std::source_location& loc = std::source_location::current())
        : result_void(error_info(code, message, loc)) {}
#else
    result_void(monitoring_error_code code,
                const std::string& message = "")
        : result_void(error_info(code, message)) {}
#endif

    static result_void success() { return result_void(); }

    static result_void error(monitoring_error_code code,
                            const std::string& message = "") {
        return result_void(code, message);
    }

#if MONITORING_HAS_COMMON_RESULT
    bool is_success() const { return common::is_ok(value_); }
#else
    bool is_success() const { return !error_.has_value(); }
#endif

    operator bool() const { return is_success(); }

    const error_info& get_error() const {
#if MONITORING_HAS_COMMON_RESULT
        if (!error_) {
            const auto& common_error = common::get_error(value_);
            error_.emplace(static_cast<monitoring_error_code>(common_error.code),
                           common_error.message);
            if (common_error.details) {
                error_->context = common_error.details;
            }
        }
        return *error_;
#else
        return error_.value();
#endif
    }

    bool is_error(monitoring_error_code code) const {
        return !is_success() && get_error().code == code;
    }

#if MONITORING_HAS_COMMON_RESULT
    const common::VoidResult& raw() const { return value_; }
#endif

private:
#if MONITORING_HAS_COMMON_RESULT
    common::VoidResult value_{std::monostate{}};
    mutable std::optional<error_info> error_;
#else
    std::optional<error_info> error_;
#endif
};

// Helper functions for creating results

/**
 * @brief Create a successful result
 * @tparam T The value type
 * @param value The success value
 * @return result<T> containing the value
 */
template<typename T>
result<std::decay_t<T>> make_success(T&& value) {
    return result<std::decay_t<T>>(std::forward<T>(value));
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
    return result<T>(code, message);
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
    error_info error(code, message);
    error.context = context;
    return result<T>(error);
}

// Macro for propagating errors
#define MONITORING_TRY(expr) \
    do { \
        auto _result = (expr); \
        if (!_result) { \
            return _result.get_error(); \
        } \
    } while(0)

#define MONITORING_TRY_ASSIGN(var, expr) \
    auto _result_##var = (expr); \
    if (!_result_##var) { \
        return _result_##var.get_error(); \
    } \
    auto& var = *_result_##var;

} // namespace monitoring_system
