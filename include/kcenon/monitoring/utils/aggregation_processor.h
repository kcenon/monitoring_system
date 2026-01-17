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
 * @file aggregation_processor.h
 * @brief Metric aggregation processing pipeline
 *
 * This file provides the aggregation processor for processing metric streams
 * and generating aggregated statistics that can be stored or exported.
 */

#include "../core/result_types.h"
#include "../core/error_codes.h"
#include "stream_aggregator.h"
#include "metric_storage.h"

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace kcenon::monitoring {

/**
 * @struct aggregation_rule
 * @brief Configuration for metric aggregation
 */
struct aggregation_rule {
    std::string source_metric;                                  ///< Source metric name
    std::string target_metric_prefix;                           ///< Prefix for aggregated metrics
    std::chrono::milliseconds aggregation_interval{60000};      ///< Aggregation interval
    std::vector<double> percentiles = {0.5, 0.9, 0.95, 0.99};   ///< Percentiles to compute
    bool compute_rate = false;                                  ///< Compute rate of change
    bool detect_outliers = true;                                ///< Enable outlier detection
    double outlier_threshold = 3.0;                             ///< Outlier detection threshold

    /**
     * @brief Validate the aggregation rule
     */
    common::VoidResult validate() const {
        if (source_metric.empty()) {
            return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                                   "Source metric name cannot be empty").to_common_error());
        }
        if (target_metric_prefix.empty()) {
            return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                                   "Target metric prefix cannot be empty").to_common_error());
        }
        if (aggregation_interval.count() <= 0) {
            return common::VoidResult::err(error_info(monitoring_error_code::invalid_configuration,
                                   "Aggregation interval must be positive").to_common_error());
        }
        return common::ok();
    }
};

/**
 * @struct stream_aggregation_result
 * @brief Result of an aggregation operation
 */
struct stream_aggregation_result {
    std::string source_metric;                              ///< Source metric name
    size_t samples_processed = 0;                           ///< Number of samples processed
    std::chrono::microseconds processing_duration{0};       ///< Processing duration
    streaming_statistics statistics;                        ///< Computed statistics
    std::chrono::system_clock::time_point timestamp;        ///< Aggregation timestamp
    bool stored_successfully = false;                       ///< Whether results were stored
};

// Alias for backward compatibility
using aggregation_result_stream = stream_aggregation_result;

/**
 * @class aggregation_processor
 * @brief Processes metric streams and generates aggregated statistics
 *
 * The aggregation processor manages multiple stream aggregators, one for each
 * configured metric, and periodically computes and stores aggregated statistics.
 */
class aggregation_processor {
public:
    /**
     * @brief Default constructor
     */
    aggregation_processor() : storage_(nullptr) {}

    /**
     * @brief Constructor with storage backend
     * @param storage Shared pointer to metric storage
     */
    explicit aggregation_processor(std::shared_ptr<metric_storage> storage)
        : storage_(std::move(storage)) {}

    /**
     * @brief Add an aggregation rule
     * @param rule The aggregation rule to add
     * @return Result indicating success or failure
     */
    common::VoidResult add_aggregation_rule(const aggregation_rule& rule) {
        auto validation = rule.validate();
        if (validation.is_err()) {
            return validation;
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);

        // Check for duplicate
        if (aggregators_.find(rule.source_metric) != aggregators_.end()) {
            return common::VoidResult::err(error_info(monitoring_error_code::already_exists,
                                   "Aggregation rule already exists for metric: " +
                                   rule.source_metric).to_common_error());
        }

        // Create stream aggregator config
        stream_aggregator_config config;
        config.enable_outlier_detection = rule.detect_outliers;
        config.outlier_threshold = rule.outlier_threshold;
        config.percentiles_to_track = rule.percentiles;

        // Create aggregator
        aggregator_entry entry;
        entry.rule = rule;
        entry.aggregator = std::make_unique<stream_aggregator>(config);
        entry.last_aggregation = std::chrono::system_clock::now();

        aggregators_.emplace(rule.source_metric, std::move(entry));

        return common::ok();
    }

    /**
     * @brief Process an observation for a metric
     * @param metric_name The metric name
     * @param value The observed value
     * @return Result indicating success or failure
     */
    common::VoidResult process_observation(const std::string& metric_name, double value) {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = aggregators_.find(metric_name);
        if (it == aggregators_.end()) {
            // No rule for this metric, silently accept
            return common::ok();
        }

        return it->second.aggregator->add_observation(value);
    }

    /**
     * @brief Get current statistics for a metric
     * @param metric_name The metric name
     * @return Result containing the statistics
     */
    common::Result<streaming_statistics> get_current_statistics(const std::string& metric_name) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = aggregators_.find(metric_name);
        if (it == aggregators_.end()) {
            return common::make_error<streaming_statistics>(
                static_cast<int>(monitoring_error_code::metric_not_found),
                "No aggregator found for metric: " + metric_name);
        }

        return common::ok(it->second.aggregator->get_statistics());
    }

    /**
     * @brief Get list of configured metrics
     * @return Vector of metric names
     */
    std::vector<std::string> get_configured_metrics() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        std::vector<std::string> metrics;
        metrics.reserve(aggregators_.size());

        for (const auto& [name, entry] : aggregators_) {
            metrics.push_back(name);
        }

        return metrics;
    }

    /**
     * @brief Force aggregation for a metric
     * @param metric_name The metric name
     * @return Result containing the aggregation result
     */
    common::Result<stream_aggregation_result> force_aggregation(const std::string& metric_name) {
        auto start_time = std::chrono::steady_clock::now();

        std::unique_lock<std::shared_mutex> lock(mutex_);

        auto it = aggregators_.find(metric_name);
        if (it == aggregators_.end()) {
            return common::make_error<stream_aggregation_result>(
                static_cast<int>(monitoring_error_code::metric_not_found),
                "No aggregator found for metric: " + metric_name);
        }

        auto& entry = it->second;
        auto stats = entry.aggregator->get_statistics();

        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);

        stream_aggregation_result agg_result;
        agg_result.source_metric = metric_name;
        agg_result.samples_processed = stats.count;
        agg_result.processing_duration = duration;
        agg_result.statistics = stats;
        agg_result.timestamp = std::chrono::system_clock::now();

        // Store aggregated metrics if storage is available
        if (storage_) {
            const auto& prefix = entry.rule.target_metric_prefix;

            storage_->store_metric(prefix + ".mean", stats.mean, metric_type::gauge);
            storage_->store_metric(prefix + ".min", stats.min_value, metric_type::gauge);
            storage_->store_metric(prefix + ".max", stats.max_value, metric_type::gauge);
            storage_->store_metric(prefix + ".stddev", stats.std_deviation, metric_type::gauge);
            storage_->store_metric(prefix + ".count",
                                   static_cast<double>(stats.count), metric_type::counter);

            for (const auto& [p, value] : stats.percentiles) {
                std::string suffix = ".p" + std::to_string(static_cast<int>(p * 100));
                storage_->store_metric(prefix + suffix, value, metric_type::gauge);
            }

            agg_result.stored_successfully = true;
        }

        // Reset the aggregator for next interval
        entry.aggregator->reset();
        entry.last_aggregation = std::chrono::system_clock::now();

        return common::ok(agg_result);
    }

    /**
     * @brief Remove an aggregation rule
     * @param metric_name The metric name
     * @return Result indicating success or failure
     */
    common::VoidResult remove_aggregation_rule(const std::string& metric_name) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        auto it = aggregators_.find(metric_name);
        if (it == aggregators_.end()) {
            return common::VoidResult::err(error_info(monitoring_error_code::metric_not_found,
                                   "No aggregator found for metric: " + metric_name).to_common_error());
        }

        aggregators_.erase(it);
        return common::ok();
    }

    /**
     * @brief Check if a metric has an aggregation rule
     * @param metric_name The metric name
     * @return true if rule exists
     */
    bool has_rule(const std::string& metric_name) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return aggregators_.find(metric_name) != aggregators_.end();
    }

    /**
     * @brief Get the number of configured rules
     */
    size_t rule_count() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return aggregators_.size();
    }

    /**
     * @brief Clear all aggregation rules
     */
    void clear() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        aggregators_.clear();
    }

private:
    struct aggregator_entry {
        aggregation_rule rule;
        std::unique_ptr<stream_aggregator> aggregator;
        std::chrono::system_clock::time_point last_aggregation;
    };

    mutable std::shared_mutex mutex_;
    std::shared_ptr<metric_storage> storage_;
    std::unordered_map<std::string, aggregator_entry> aggregators_;
};

/**
 * @brief Create standard aggregation rules for common metrics
 * @return Vector of pre-configured aggregation rules
 */
inline std::vector<aggregation_rule> create_standard_aggregation_rules() {
    std::vector<aggregation_rule> rules;

    // Response time metrics
    {
        aggregation_rule rule;
        rule.source_metric = "response_time";
        rule.target_metric_prefix = "response_time_agg";
        rule.aggregation_interval = std::chrono::milliseconds(60000);
        rule.percentiles = {0.5, 0.9, 0.95, 0.99};
        rule.compute_rate = false;
        rule.detect_outliers = true;
        rules.push_back(rule);
    }

    // Request count metrics
    {
        aggregation_rule rule;
        rule.source_metric = "request_count";
        rule.target_metric_prefix = "request_count_agg";
        rule.aggregation_interval = std::chrono::milliseconds(60000);
        rule.percentiles = {0.5, 0.9, 0.95, 0.99};
        rule.compute_rate = true;
        rule.detect_outliers = false;
        rules.push_back(rule);
    }

    // Error count metrics
    {
        aggregation_rule rule;
        rule.source_metric = "error_count";
        rule.target_metric_prefix = "error_count_agg";
        rule.aggregation_interval = std::chrono::milliseconds(60000);
        rule.percentiles = {0.5, 0.9, 0.95, 0.99};
        rule.compute_rate = true;
        rule.detect_outliers = true;
        rule.outlier_threshold = 2.0;  // More sensitive for errors
        rules.push_back(rule);
    }

    // CPU usage metrics
    {
        aggregation_rule rule;
        rule.source_metric = "cpu_usage";
        rule.target_metric_prefix = "cpu_usage_agg";
        rule.aggregation_interval = std::chrono::milliseconds(60000);
        rule.percentiles = {0.5, 0.9, 0.95, 0.99};
        rule.compute_rate = false;
        rule.detect_outliers = true;
        rules.push_back(rule);
    }

    // Memory usage metrics
    {
        aggregation_rule rule;
        rule.source_metric = "memory_usage";
        rule.target_metric_prefix = "memory_usage_agg";
        rule.aggregation_interval = std::chrono::milliseconds(60000);
        rule.percentiles = {0.5, 0.9, 0.95, 0.99};
        rule.compute_rate = false;
        rule.detect_outliers = true;
        rules.push_back(rule);
    }

    return rules;
}

} // namespace kcenon::monitoring
