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

#include <cctype>
#include <chrono>
#include <functional>
#include <optional>
#include <regex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

    /**
     * @brief Get a configuration value from a set of allowed values
     * @tparam T The target type
     * @param config The configuration map
     * @param key The configuration key to look up
     * @param default_value The default value if key is not found or invalid
     * @param allowed_values Set of valid values
     * @return The parsed value if valid, otherwise default
     */
    template <typename T>
    static T get_enum(const config_map& config, const std::string& key, const T& default_value,
                      const std::unordered_set<T>& allowed_values) {
        T value = get<T>(config, key, default_value);
        if (allowed_values.find(value) != allowed_values.end()) {
            return value;
        }
        return default_value;
    }

    /**
     * @brief Get a string configuration value matching a regex pattern
     * @param config The configuration map
     * @param key The configuration key to look up
     * @param default_value The default value if key is not found or invalid
     * @param pattern The regex pattern to match against
     * @return The value if it matches the pattern, otherwise default
     */
    static std::string get_matching(const config_map& config, const std::string& key,
                                     const std::string& default_value, const std::string& pattern) {
        auto it = config.find(key);
        if (it == config.end()) {
            return default_value;
        }
        try {
            std::regex regex(pattern);
            if (std::regex_match(it->second, regex)) {
                return it->second;
            }
        } catch (...) {
            // Invalid regex or no match
        }
        return default_value;
    }

    /**
     * @brief Get a configuration value with custom validation
     * @tparam T The target type
     * @param config The configuration map
     * @param key The configuration key to look up
     * @param default_value The default value if key is not found or validation fails
     * @param validator A function that returns true if the value is valid
     * @return The parsed value if valid, otherwise default
     */
    template <typename T>
    static T get_validated(const config_map& config, const std::string& key, const T& default_value,
                           std::function<bool(const T&)> validator) {
        T value = get<T>(config, key, default_value);
        if (validator(value)) {
            return value;
        }
        return default_value;
    }

    /**
     * @brief Get a duration value from configuration
     * @tparam Duration The chrono duration type (e.g., std::chrono::milliseconds)
     * @param config The configuration map
     * @param key The configuration key to look up
     * @param default_value The default duration if key is not found
     * @return The parsed duration value
     *
     * Supported formats:
     * - Plain number: interpreted as the Duration's unit (e.g., 1000 = 1000ms for milliseconds)
     * - With suffix: 100ms, 5s, 2m, 1h (milliseconds, seconds, minutes, hours)
     */
    template <typename Duration>
    static Duration get_duration(const config_map& config, const std::string& key,
                                  const Duration& default_value) {
        auto it = config.find(key);
        if (it == config.end()) {
            return default_value;
        }
        return parse_duration<Duration>(it->second, default_value);
    }

    /**
     * @brief Get a list of values from a comma-separated string
     * @tparam T The element type
     * @param config The configuration map
     * @param key The configuration key to look up
     * @param default_values The default list if key is not found
     * @return Vector of parsed values
     */
    template <typename T>
    static std::vector<T> get_list(const config_map& config, const std::string& key,
                                    const std::vector<T>& default_values) {
        auto it = config.find(key);
        if (it == config.end()) {
            return default_values;
        }
        return parse_list<T>(it->second, default_values);
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

    /**
     * @brief Parse duration string with optional suffix
     * Supported suffixes: ms (milliseconds), s (seconds), m (minutes), h (hours)
     */
    template <typename Duration>
    static Duration parse_duration(const std::string& str, const Duration& default_value) {
        try {
            if (str.empty()) {
                return default_value;
            }

            // Find where the number ends and suffix begins
            size_t suffix_start = str.find_first_not_of("0123456789.-");

            if (suffix_start == std::string::npos) {
                // Plain number, interpret as Duration's unit
                long long value = std::stoll(str);
                return Duration(value);
            }

            // Parse the numeric part
            long long value = std::stoll(str.substr(0, suffix_start));
            std::string suffix = str.substr(suffix_start);

            // Trim whitespace from suffix
            while (!suffix.empty() && std::isspace(static_cast<unsigned char>(suffix.front()))) {
                suffix.erase(0, 1);
            }

            // Convert to lowercase
            for (auto& c : suffix) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }

            // Parse suffix and convert to Duration
            if (suffix == "ms" || suffix == "millisecond" || suffix == "milliseconds") {
                return std::chrono::duration_cast<Duration>(std::chrono::milliseconds(value));
            } else if (suffix == "s" || suffix == "sec" || suffix == "second" || suffix == "seconds") {
                return std::chrono::duration_cast<Duration>(std::chrono::seconds(value));
            } else if (suffix == "m" || suffix == "min" || suffix == "minute" || suffix == "minutes") {
                return std::chrono::duration_cast<Duration>(std::chrono::minutes(value));
            } else if (suffix == "h" || suffix == "hr" || suffix == "hour" || suffix == "hours") {
                return std::chrono::duration_cast<Duration>(std::chrono::hours(value));
            } else {
                // Unknown suffix, treat as plain number
                return Duration(value);
            }
        } catch (...) {
            return default_value;
        }
    }

    /**
     * @brief Parse comma-separated list of values
     */
    template <typename T>
    static std::vector<T> parse_list(const std::string& str, const std::vector<T>& default_values) {
        try {
            if (str.empty()) {
                return default_values;
            }

            std::vector<T> result;
            std::string current;

            for (char c : str) {
                if (c == ',') {
                    // Trim whitespace
                    size_t start = current.find_first_not_of(" \t");
                    size_t end = current.find_last_not_of(" \t");
                    if (start != std::string::npos) {
                        std::string trimmed = current.substr(start, end - start + 1);
                        auto parsed = parse_value_optional<T>(trimmed);
                        if (parsed) {
                            result.push_back(*parsed);
                        }
                    }
                    current.clear();
                } else {
                    current += c;
                }
            }

            // Handle last element
            if (!current.empty()) {
                size_t start = current.find_first_not_of(" \t");
                size_t end = current.find_last_not_of(" \t");
                if (start != std::string::npos) {
                    std::string trimmed = current.substr(start, end - start + 1);
                    auto parsed = parse_value_optional<T>(trimmed);
                    if (parsed) {
                        result.push_back(*parsed);
                    }
                }
            }

            return result.empty() ? default_values : result;
        } catch (...) {
            return default_values;
        }
    }
};

}  // namespace kcenon::monitoring
