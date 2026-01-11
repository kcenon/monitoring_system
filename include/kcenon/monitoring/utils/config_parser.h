// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

/**
 * @file config_parser.h
 * @brief Unified configuration parsing utility
 *
 * This file provides type-safe configuration value parsing utilities
 * to standardize configuration handling across all collectors.
 * Reduces code duplication and ensures consistent parsing behavior.
 *
 * Usage:
 * @code
 * using kcenon::monitoring::config_parser;
 *
 * config_map config = {{"enabled", "true"}, {"interval", "1000"}};
 *
 * bool enabled = config_parser::get<bool>(config, "enabled", true);
 * int interval = config_parser::get<int>(config, "interval", 500);
 * std::string name = config_parser::get<std::string>(config, "name", "default");
 * @endcode
 */

#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>

namespace kcenon::monitoring {

/**
 * @brief Type alias for configuration map
 */
using config_map = std::unordered_map<std::string, std::string>;

/**
 * @class config_parser
 * @brief Unified configuration parsing utility
 *
 * Provides type-safe parsing of configuration values with default value support.
 * Handles boolean, integer, floating-point, and string types consistently.
 */
class config_parser {
   public:
    /**
     * @brief Get a configuration value with type conversion
     * @tparam T The target type (bool, int, size_t, double, std::string, etc.)
     * @param config The configuration map
     * @param key The configuration key to look up
     * @param default_value The default value if key is not found or parsing fails
     * @return The parsed value or default
     */
    template <typename T>
    static T get(const config_map& config, const std::string& key, const T& default_value) {
        auto it = config.find(key);
        if (it == config.end()) {
            return default_value;
        }
        return parse_value<T>(it->second, default_value);
    }

    /**
     * @brief Get a configuration value as optional
     * @tparam T The target type
     * @param config The configuration map
     * @param key The configuration key to look up
     * @return Optional containing the value if found and parseable, empty otherwise
     */
    template <typename T>
    static std::optional<T> get_optional(const config_map& config, const std::string& key) {
        auto it = config.find(key);
        if (it == config.end()) {
            return std::nullopt;
        }
        return parse_value_optional<T>(it->second);
    }

    /**
     * @brief Check if a configuration key exists
     * @param config The configuration map
     * @param key The configuration key to check
     * @return true if the key exists
     */
    static bool has_key(const config_map& config, const std::string& key) {
        return config.find(key) != config.end();
    }

    /**
     * @brief Get a configuration value with validation
     * @tparam T The target type
     * @param config The configuration map
     * @param key The configuration key to look up
     * @param default_value The default value if key is not found
     * @param min_value Minimum allowed value
     * @param max_value Maximum allowed value
     * @return The parsed value clamped to [min_value, max_value] or default
     */
    template <typename T>
    static T get_clamped(const config_map& config, const std::string& key, const T& default_value,
                         const T& min_value, const T& max_value) {
        static_assert(std::is_arithmetic_v<T>, "get_clamped requires arithmetic type");
        T value = get<T>(config, key, default_value);
        if (value < min_value) {
            return min_value;
        }
        if (value > max_value) {
            return max_value;
        }
        return value;
    }

   private:
    /**
     * @brief Parse a string value to target type with default fallback
     */
    template <typename T>
    static T parse_value(const std::string& str, const T& default_value) {
        try {
            if constexpr (std::is_same_v<T, bool>) {
                return parse_bool(str);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return str;
            } else if constexpr (std::is_integral_v<T>) {
                return parse_integral<T>(str);
            } else if constexpr (std::is_floating_point_v<T>) {
                return parse_floating<T>(str);
            } else {
                return default_value;
            }
        } catch (...) {
            return default_value;
        }
    }

    /**
     * @brief Parse a string value to target type as optional
     */
    template <typename T>
    static std::optional<T> parse_value_optional(const std::string& str) {
        try {
            if constexpr (std::is_same_v<T, bool>) {
                return parse_bool(str);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return str;
            } else if constexpr (std::is_integral_v<T>) {
                return parse_integral<T>(str);
            } else if constexpr (std::is_floating_point_v<T>) {
                return parse_floating<T>(str);
            } else {
                return std::nullopt;
            }
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Parse boolean value from string
     * @param str String to parse ("true", "1", "yes", "on" -> true)
     * @return Parsed boolean value
     */
    static bool parse_bool(const std::string& str) {
        if (str.empty()) {
            return false;
        }
        // Case-insensitive comparison
        std::string lower = str;
        for (auto& c : lower) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return lower == "true" || lower == "1" || lower == "yes" || lower == "on";
    }

    /**
     * @brief Parse integral value from string
     */
    template <typename T>
    static T parse_integral(const std::string& str) {
        static_assert(std::is_integral_v<T>, "parse_integral requires integral type");
        if constexpr (std::is_signed_v<T>) {
            long long value = std::stoll(str);
            return static_cast<T>(value);
        } else {
            unsigned long long value = std::stoull(str);
            return static_cast<T>(value);
        }
    }

    /**
     * @brief Parse floating-point value from string
     */
    template <typename T>
    static T parse_floating(const std::string& str) {
        static_assert(std::is_floating_point_v<T>, "parse_floating requires floating point type");
        if constexpr (std::is_same_v<T, float>) {
            return std::stof(str);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::stod(str);
        } else {
            return static_cast<T>(std::stold(str));
        }
    }
};

}  // namespace kcenon::monitoring
