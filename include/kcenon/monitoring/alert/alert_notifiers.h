// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
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
 * @file alert_notifiers.h
 * @brief Alert notification implementations
 *
 * This file provides various notifier implementations for sending
 * alert notifications to different targets (webhooks, files, etc.).
 */

#include <chrono>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "alert_manager.h"
#include "alert_types.h"
#include "../core/result_types.h"

namespace kcenon::monitoring {

/**
 * @struct webhook_config
 * @brief Configuration for webhook notifier
 */
struct webhook_config {
    std::string url;                                        ///< Webhook URL
    std::string method = "POST";                            ///< HTTP method
    std::chrono::milliseconds timeout{30000};               ///< Request timeout
    std::unordered_map<std::string, std::string> headers;   ///< Custom headers
    size_t max_retries = 3;                                 ///< Maximum retry attempts
    std::chrono::milliseconds retry_delay{1000};            ///< Delay between retries
    bool send_resolved = true;                              ///< Send resolved notifications
    std::string content_type = "application/json";          ///< Content type header

    /**
     * @brief Add a custom header
     */
    webhook_config& add_header(const std::string& key, const std::string& value) {
        headers[key] = value;
        return *this;
    }

    /**
     * @brief Validate configuration
     */
    bool validate() const {
        return !url.empty() && timeout.count() > 0;
    }
};

/**
 * @class alert_formatter
 * @brief Formats alerts for notification payloads
 *
 * Base class for formatting alerts into various formats
 * (JSON, text, etc.) for different notification targets.
 */
class alert_formatter {
public:
    virtual ~alert_formatter() = default;

    /**
     * @brief Format a single alert
     * @param a Alert to format
     * @return Formatted string
     */
    virtual std::string format(const alert& a) const = 0;

    /**
     * @brief Format an alert group
     * @param group Alert group to format
     * @return Formatted string
     */
    virtual std::string format_group(const alert_group& group) const = 0;
};

/**
 * @class json_alert_formatter
 * @brief Formats alerts as JSON
 */
class json_alert_formatter : public alert_formatter {
public:
    std::string format(const alert& a) const override {
        std::ostringstream oss;
        oss << "{";
        oss << "\"name\":\"" << escape_json(a.name) << "\",";
        oss << "\"state\":\"" << alert_state_to_string(a.state) << "\",";
        oss << "\"severity\":\"" << alert_severity_to_string(a.severity) << "\",";
        oss << "\"value\":" << a.value << ",";
        oss << "\"summary\":\"" << escape_json(a.annotations.summary) << "\",";
        oss << "\"description\":\"" << escape_json(a.annotations.description) << "\",";
        oss << "\"fingerprint\":\"" << a.fingerprint() << "\",";
        oss << "\"labels\":{";
        bool first = true;
        for (const auto& [key, value] : a.labels.labels) {
            if (!first) oss << ",";
            oss << "\"" << escape_json(key) << "\":\"" << escape_json(value) << "\"";
            first = false;
        }
        oss << "}}";
        return oss.str();
    }

    std::string format_group(const alert_group& group) const override {
        std::ostringstream oss;
        oss << "{";
        oss << "\"group_key\":\"" << escape_json(group.group_key) << "\",";
        oss << "\"severity\":\"" << alert_severity_to_string(group.max_severity()) << "\",";
        oss << "\"alert_count\":" << group.size() << ",";
        oss << "\"alerts\":[";
        bool first = true;
        for (const auto& a : group.alerts) {
            if (!first) oss << ",";
            oss << format(a);
            first = false;
        }
        oss << "]}";
        return oss.str();
    }

private:
    static std::string escape_json(const std::string& s) {
        std::ostringstream oss;
        for (char c : s) {
            switch (c) {
                case '"':  oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\n': oss << "\\n";  break;
                case '\r': oss << "\\r";  break;
                case '\t': oss << "\\t";  break;
                default:   oss << c;      break;
            }
        }
        return oss.str();
    }
};

/**
 * @class text_alert_formatter
 * @brief Formats alerts as human-readable text
 */
class text_alert_formatter : public alert_formatter {
public:
    std::string format(const alert& a) const override {
        std::ostringstream oss;
        oss << "[" << alert_state_to_string(a.state) << "] "
            << a.name << " (" << alert_severity_to_string(a.severity) << ")\n"
            << "  Summary: " << a.annotations.summary << "\n"
            << "  Value: " << a.value << "\n"
            << "  Fingerprint: " << a.fingerprint();
        return oss.str();
    }

    std::string format_group(const alert_group& group) const override {
        std::ostringstream oss;
        oss << "Alert Group: " << group.group_key << "\n"
            << "  Total alerts: " << group.size() << "\n"
            << "  Max severity: " << alert_severity_to_string(group.max_severity()) << "\n"
            << "  Alerts:\n";
        for (const auto& a : group.alerts) {
            oss << "    - " << a.name << " (" << alert_state_to_string(a.state) << ")\n";
        }
        return oss.str();
    }
};

/**
 * @class webhook_notifier
 * @brief Sends alerts to a webhook endpoint
 *
 * Note: Actual HTTP implementation requires network_system integration
 * or an external HTTP client. This implementation provides the interface
 * and can be extended with actual HTTP support.
 *
 * @example
 * @code
 * webhook_config config;
 * config.url = "https://hooks.example.com/alert";
 * config.add_header("Authorization", "Bearer token");
 *
 * auto notifier = std::make_shared<webhook_notifier>(config);
 * manager.add_notifier(notifier);
 * @endcode
 */
class webhook_notifier : public alert_notifier {
public:
    using http_sender_func = std::function<common::VoidResult(
        const std::string& url,
        const std::string& method,
        const std::unordered_map<std::string, std::string>& headers,
        const std::string& body
    )>;

    /**
     * @brief Construct webhook notifier
     * @param config Webhook configuration
     * @param formatter Alert formatter (default: JSON)
     */
    explicit webhook_notifier(const webhook_config& config,
                              std::shared_ptr<alert_formatter> formatter = nullptr)
        : config_(config)
        , formatter_(formatter ? formatter : std::make_shared<json_alert_formatter>()) {}

    std::string name() const override {
        return "webhook:" + config_.url;
    }

    common::VoidResult notify(const alert& a) override {
        if (!config_.send_resolved && a.state == alert_state::resolved) {
            return common::ok();
        }

        std::string payload = formatter_->format(a);
        return send_with_retry(payload);
    }

    common::VoidResult notify_group(const alert_group& group) override {
        std::string payload = formatter_->format_group(group);
        return send_with_retry(payload);
    }

    bool is_ready() const override {
        return config_.validate() && http_sender_ != nullptr;
    }

    /**
     * @brief Set HTTP sender function for actual HTTP calls
     * @param sender Function to send HTTP requests
     *
     * This allows injecting a real HTTP implementation:
     * @code
     * notifier->set_http_sender([](const std::string& url,
     *                              const std::string& method,
     *                              const auto& headers,
     *                              const std::string& body) {
     *     // Use network_system or other HTTP client
     *     return http_client.request(url, method, headers, body);
     * });
     * @endcode
     */
    void set_http_sender(http_sender_func sender) {
        http_sender_ = std::move(sender);
    }

    /**
     * @brief Get configuration
     */
    const webhook_config& config() const { return config_; }

private:
    common::VoidResult send_with_retry(const std::string& payload) {
        if (!http_sender_) {
            return common::VoidResult::err(error_info(monitoring_error_code::operation_failed, "No HTTP sender configured").to_common_error());
        }

        auto headers = config_.headers;
        headers["Content-Type"] = config_.content_type;

        for (size_t attempt = 0; attempt <= config_.max_retries; ++attempt) {
            auto result = http_sender_(config_.url, config_.method, headers, payload);
            if (result.is_ok()) {
                return result;
            }

            if (attempt < config_.max_retries) {
                std::this_thread::sleep_for(config_.retry_delay);
            }
        }

        return make_void_error(monitoring_error_code::retry_attempts_exhausted,
                               "Failed to send webhook after " +
                               std::to_string(config_.max_retries) + " retries");
    }

    webhook_config config_;
    std::shared_ptr<alert_formatter> formatter_;
    http_sender_func http_sender_;
};

/**
 * @class file_notifier
 * @brief Writes alerts to a file
 *
 * Appends alert notifications to a specified file, useful for
 * logging or audit trails.
 */
class file_notifier : public alert_notifier {
public:
    /**
     * @brief Construct file notifier
     * @param file_path Path to output file
     * @param formatter Alert formatter (default: text)
     */
    explicit file_notifier(std::string file_path,
                           std::shared_ptr<alert_formatter> formatter = nullptr)
        : file_path_(std::move(file_path))
        , formatter_(formatter ? formatter : std::make_shared<text_alert_formatter>()) {}

    std::string name() const override {
        return "file:" + file_path_;
    }

    common::VoidResult notify(const alert& a) override {
        return write_to_file(formatter_->format(a));
    }

    common::VoidResult notify_group(const alert_group& group) override {
        return write_to_file(formatter_->format_group(group));
    }

    bool is_ready() const override {
        return !file_path_.empty();
    }

private:
    common::VoidResult write_to_file(const std::string& content) {
        std::lock_guard<std::mutex> lock(mutex_);

        std::ofstream file(file_path_, std::ios::app);
        if (!file) {
            return make_void_error(monitoring_error_code::storage_write_failed,
                                   "Failed to open file: " + file_path_);
        }

        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);

        file << "=== " << std::ctime(&time_t_now);
        file << content << "\n\n";

        return common::ok();
    }

    std::string file_path_;
    std::shared_ptr<alert_formatter> formatter_;
    std::mutex mutex_;
};

/**
 * @class multi_notifier
 * @brief Sends alerts to multiple notifiers
 *
 * Wraps multiple notifiers and sends alerts to all of them.
 * Collects results from all notifiers.
 */
class multi_notifier : public alert_notifier {
public:
    /**
     * @brief Construct with name
     */
    explicit multi_notifier(std::string notifier_name)
        : name_(std::move(notifier_name)) {}

    /**
     * @brief Add a child notifier
     */
    void add_notifier(std::shared_ptr<alert_notifier> notifier) {
        std::lock_guard<std::mutex> lock(mutex_);
        notifiers_.push_back(std::move(notifier));
    }

    std::string name() const override { return name_; }

    common::VoidResult notify(const alert& a) override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<std::string> failures;
        for (const auto& notifier : notifiers_) {
            if (notifier && notifier->is_ready()) {
                auto result = notifier->notify(a);
                if (!result.is_ok()) {
                    failures.push_back(notifier->name());
                }
            }
        }

        if (!failures.empty()) {
            return make_void_error(monitoring_error_code::operation_failed,
                                   "Failed notifiers: " + join_strings(failures, ", "));
        }
        return common::ok();
    }

    common::VoidResult notify_group(const alert_group& group) override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<std::string> failures;
        for (const auto& notifier : notifiers_) {
            if (notifier && notifier->is_ready()) {
                auto result = notifier->notify_group(group);
                if (!result.is_ok()) {
                    failures.push_back(notifier->name());
                }
            }
        }

        if (!failures.empty()) {
            return make_void_error(monitoring_error_code::operation_failed,
                                   "Failed notifiers: " + join_strings(failures, ", "));
        }
        return common::ok();
    }

    bool is_ready() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return !notifiers_.empty();
    }

private:
    static std::string join_strings(const std::vector<std::string>& strings,
                                    const std::string& delimiter) {
        std::ostringstream oss;
        for (size_t i = 0; i < strings.size(); ++i) {
            if (i > 0) oss << delimiter;
            oss << strings[i];
        }
        return oss.str();
    }

    std::string name_;
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<alert_notifier>> notifiers_;
};

/**
 * @class buffered_notifier
 * @brief Buffers alerts and sends in batches
 *
 * Collects alerts and sends them in batches either when the buffer
 * is full or when flush is called.
 */
class buffered_notifier : public alert_notifier {
public:
    /**
     * @brief Construct buffered notifier
     * @param inner Inner notifier to use for actual sending
     * @param buffer_size Maximum alerts before auto-flush
     * @param flush_interval Maximum time before auto-flush
     */
    buffered_notifier(std::shared_ptr<alert_notifier> inner,
                      size_t buffer_size = 100,
                      std::chrono::milliseconds flush_interval = std::chrono::seconds(30))
        : inner_(std::move(inner))
        , buffer_size_(buffer_size)
        , flush_interval_(flush_interval)
        , last_flush_(std::chrono::steady_clock::now()) {}

    std::string name() const override {
        return "buffered:" + (inner_ ? inner_->name() : "none");
    }

    common::VoidResult notify(const alert& a) override {
        std::lock_guard<std::mutex> lock(mutex_);

        buffer_.push_back(a);

        if (should_flush()) {
            return flush_internal();
        }

        return common::ok();
    }

    common::VoidResult notify_group(const alert_group& group) override {
        std::lock_guard<std::mutex> lock(mutex_);

        for (const auto& a : group.alerts) {
            buffer_.push_back(a);
        }

        if (should_flush()) {
            return flush_internal();
        }

        return common::ok();
    }

    bool is_ready() const override {
        return inner_ && inner_->is_ready();
    }

    /**
     * @brief Flush buffered alerts
     */
    common::VoidResult flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        return flush_internal();
    }

    /**
     * @brief Get current buffer size
     */
    size_t pending_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer_.size();
    }

private:
    bool should_flush() const {
        if (buffer_.size() >= buffer_size_) {
            return true;
        }
        auto now = std::chrono::steady_clock::now();
        return (now - last_flush_) >= flush_interval_;
    }

    common::VoidResult flush_internal() {
        if (buffer_.empty() || !inner_) {
            return common::ok();
        }

        // Create a group from buffered alerts
        alert_group group("buffered");
        for (auto& a : buffer_) {
            group.add_alert(std::move(a));
        }
        buffer_.clear();

        last_flush_ = std::chrono::steady_clock::now();

        return inner_->notify_group(group);
    }

    std::shared_ptr<alert_notifier> inner_;
    size_t buffer_size_;
    std::chrono::milliseconds flush_interval_;

    mutable std::mutex mutex_;
    std::vector<alert> buffer_;
    std::chrono::steady_clock::time_point last_flush_;
};

/**
 * @class routing_notifier
 * @brief Routes alerts to different notifiers based on criteria
 *
 * Allows configuring different notification targets based on
 * alert properties like severity or labels.
 */
class routing_notifier : public alert_notifier {
public:
    using route_condition = std::function<bool(const alert&)>;

    /**
     * @brief Construct routing notifier
     */
    explicit routing_notifier(std::string notifier_name)
        : name_(std::move(notifier_name)) {}

    /**
     * @brief Add a route
     * @param condition Condition for this route
     * @param notifier Notifier to use when condition matches
     */
    void add_route(route_condition condition,
                   std::shared_ptr<alert_notifier> notifier) {
        std::lock_guard<std::mutex> lock(mutex_);
        routes_.push_back({std::move(condition), std::move(notifier)});
    }

    /**
     * @brief Add a default route for non-matching alerts
     */
    void set_default_route(std::shared_ptr<alert_notifier> notifier) {
        std::lock_guard<std::mutex> lock(mutex_);
        default_route_ = std::move(notifier);
    }

    std::string name() const override { return name_; }

    common::VoidResult notify(const alert& a) override {
        std::lock_guard<std::mutex> lock(mutex_);

        for (const auto& [condition, notifier] : routes_) {
            if (condition(a) && notifier && notifier->is_ready()) {
                return notifier->notify(a);
            }
        }

        if (default_route_ && default_route_->is_ready()) {
            return default_route_->notify(a);
        }

        return common::ok();
    }

    common::VoidResult notify_group(const alert_group& group) override {
        // Route each alert individually
        for (const auto& a : group.alerts) {
            auto result = notify(a);
            if (!result.is_ok()) {
                return result;
            }
        }
        return common::ok();
    }

    bool is_ready() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return !routes_.empty() || default_route_ != nullptr;
    }

    // Factory methods for common routes
    /**
     * @brief Route by severity
     */
    void route_by_severity(alert_severity severity,
                          std::shared_ptr<alert_notifier> notifier) {
        add_route(
            [severity](const alert& a) { return a.severity == severity; },
            std::move(notifier)
        );
    }

    /**
     * @brief Route by label
     */
    void route_by_label(const std::string& key,
                        const std::string& value,
                        std::shared_ptr<alert_notifier> notifier) {
        add_route(
            [key, value](const alert& a) { return a.labels.get(key) == value; },
            std::move(notifier)
        );
    }

private:
    struct route {
        route_condition condition;
        std::shared_ptr<alert_notifier> notifier;
    };

    std::string name_;
    mutable std::mutex mutex_;
    std::vector<route> routes_;
    std::shared_ptr<alert_notifier> default_route_;
};

} // namespace kcenon::monitoring
