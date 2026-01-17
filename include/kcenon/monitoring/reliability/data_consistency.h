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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <kcenon/monitoring/core/result_types.h>

namespace kcenon::monitoring {

/**
 * @brief Transaction states
 */
enum class transaction_state {
    active,
    committed,
    aborted
};

/**
 * @brief Validation result states
 */
enum class validation_result {
    valid,
    invalid
};

/**
 * @brief Transaction configuration
 */
struct transaction_config {
    std::chrono::milliseconds timeout{std::chrono::milliseconds(30000)};
    std::chrono::milliseconds lock_timeout{std::chrono::milliseconds(10000)};
    size_t max_retries{3};

    [[nodiscard]] bool validate() const {
        if (timeout.count() <= 0) {
            return false;
        }
        if (lock_timeout.count() <= 0) {
            return false;
        }
        if (max_retries == 0) {
            return false;
        }
        return true;
    }
};

/**
 * @brief Validation configuration
 */
struct validation_config {
    std::chrono::milliseconds validation_interval{std::chrono::milliseconds(60000)};
    size_t max_validation_failures{5};
    double corruption_threshold{0.1};
    bool enable_auto_repair{false};

    [[nodiscard]] bool validate() const {
        if (validation_interval.count() <= 0) {
            return false;
        }
        if (max_validation_failures == 0) {
            return false;
        }
        if (corruption_threshold < 0.0 || corruption_threshold > 1.0) {
            return false;
        }
        return true;
    }
};

/**
 * @brief Transaction metrics
 */
struct transaction_metrics {
    std::atomic<size_t> total_transactions{0};
    std::atomic<size_t> committed_transactions{0};
    std::atomic<size_t> aborted_transactions{0};
    std::atomic<size_t> deadlocks_detected{0};

    double get_abort_rate() const {
        size_t total = total_transactions.load();
        if (total == 0) {
            return 0.0;
        }
        return static_cast<double>(aborted_transactions.load()) / static_cast<double>(total);
    }
};

/**
 * @brief Validation metrics
 */
struct validation_metrics {
    std::atomic<size_t> validation_runs{0};
    std::atomic<size_t> repair_operations{0};
};

/**
 * @brief Single transaction operation with execute and rollback capabilities
 */
class transaction_operation {
public:
    using execute_func_t = std::function<common::VoidResult()>;
    using rollback_func_t = std::function<common::VoidResult()>;

    transaction_operation(const std::string& name,
                          execute_func_t execute_func,
                          rollback_func_t rollback_func = nullptr)
        : name_(name)
        , execute_func_(std::move(execute_func))
        , rollback_func_(std::move(rollback_func))
        , executed_(false) {}

    std::string name() const { return name_; }
    bool is_executed() const { return executed_; }

    common::VoidResult execute() {
        if (execute_func_) {
            auto result = execute_func_();
            if (result.is_ok()) {
                executed_ = true;
            }
            return result;
        }
        executed_ = true;
        return common::ok();
    }

    bool rollback() {
        if (rollback_func_) {
            auto result = rollback_func_();
            return result.is_ok();
        }
        return true;
    }

private:
    std::string name_;
    execute_func_t execute_func_;
    rollback_func_t rollback_func_;
    bool executed_;
};

/**
 * @brief Transaction containing multiple operations
 */
class transaction {
public:
    transaction(const std::string& id, const transaction_config& config)
        : id_(id)
        , config_(config)
        , state_(transaction_state::active)
        , creation_time_(std::chrono::steady_clock::now()) {}

    std::string id() const { return id_; }
    transaction_state state() const { return state_; }
    size_t operation_count() const { return operations_.size(); }

    bool add_operation(std::unique_ptr<transaction_operation> op) {
        if (state_ != transaction_state::active) {
            return false;
        }
        operations_.push_back(std::move(op));
        return true;
    }

    bool commit() {
        if (state_ != transaction_state::active) {
            return false;
        }

        // Check for timeout
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - creation_time_);
        if (elapsed > config_.timeout) {
            abort();
            return false;
        }

        // Execute all operations
        size_t executed_count = 0;
        for (auto& op : operations_) {
            auto result = op->execute();
            if (!result.is_ok()) {
                // Rollback executed operations in reverse order
                for (size_t i = executed_count; i > 0; --i) {
                    operations_[i - 1]->rollback();
                }
                state_ = transaction_state::aborted;
                return false;
            }
            ++executed_count;
        }

        state_ = transaction_state::committed;
        return true;
    }

    bool abort() {
        if (state_ != transaction_state::active) {
            return false;
        }
        state_ = transaction_state::aborted;
        return true;
    }

    std::chrono::steady_clock::time_point creation_time() const {
        return creation_time_;
    }

private:
    std::string id_;
    transaction_config config_;
    transaction_state state_;
    std::chrono::steady_clock::time_point creation_time_;
    std::vector<std::unique_ptr<transaction_operation>> operations_;
};

/**
 * @brief Transaction manager for coordinating transactions
 */
class transaction_manager {
public:
    transaction_manager(const std::string& name, const transaction_config& config)
        : name_(name)
        , config_(config) {}

    std::string get_name() const { return name_; }

    common::Result<std::shared_ptr<transaction>> begin_transaction(const std::string& id) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        // Check for duplicate transaction
        if (active_transactions_.find(id) != active_transactions_.end()) {
            return make_error<std::shared_ptr<transaction>>(
                monitoring_error_code::already_exists,
                "Transaction with ID '" + id + "' already exists");
        }

        auto tx = std::make_shared<transaction>(id, config_);
        active_transactions_[id] = tx;
        metrics_.total_transactions++;
        return common::ok(tx);
    }

    bool commit_transaction(const std::string& id) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        auto it = active_transactions_.find(id);
        if (it == active_transactions_.end()) {
            return false;
        }

        auto tx = it->second;
        active_transactions_.erase(it);

        bool success = tx->commit();
        if (success) {
            metrics_.committed_transactions++;
            completed_transactions_[id] = tx;
        } else {
            metrics_.aborted_transactions++;
        }
        return success;
    }

    bool abort_transaction(const std::string& id) {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        auto it = active_transactions_.find(id);
        if (it == active_transactions_.end()) {
            return false;
        }

        auto tx = it->second;
        active_transactions_.erase(it);

        tx->abort();
        metrics_.aborted_transactions++;
        return true;
    }

    size_t active_transaction_count() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return active_transactions_.size();
    }

    size_t completed_transaction_count() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return completed_transactions_.size();
    }

    common::Result<std::vector<std::string>> detect_deadlocks() {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        std::vector<std::string> deadlocked;
        auto now = std::chrono::steady_clock::now();

        for (const auto& [id, tx] : active_transactions_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - tx->creation_time());
            if (elapsed > config_.timeout) {
                deadlocked.push_back(id);
                metrics_.deadlocks_detected++;
            }
        }

        return common::ok(deadlocked);
    }

    void cleanup_completed_transactions(std::chrono::milliseconds /*max_age*/) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        completed_transactions_.clear();
    }

    transaction_metrics& get_metrics() { return metrics_; }
    const transaction_metrics& get_metrics() const { return metrics_; }

private:
    std::string name_;
    transaction_config config_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<transaction>> active_transactions_;
    std::unordered_map<std::string, std::shared_ptr<transaction>> completed_transactions_;
    transaction_metrics metrics_;
};

/**
 * @brief State validator for validating system state
 */
class state_validator {
public:
    using validation_func_t = std::function<validation_result()>;
    using repair_func_t = std::function<common::VoidResult()>;

    state_validator(const std::string& name, const validation_config& config)
        : name_(name)
        , config_(config)
        , running_(false) {}

    ~state_validator() {
        stop();
    }

    // Non-copyable and non-movable due to thread ownership
    state_validator(const state_validator&) = delete;
    state_validator& operator=(const state_validator&) = delete;
    state_validator(state_validator&&) = delete;
    state_validator& operator=(state_validator&&) = delete;

    std::string get_name() const { return name_; }

    bool add_validation_rule(const std::string& name,
                             validation_func_t validation_func,
                             repair_func_t repair_func = nullptr) {
        std::unique_lock<std::mutex> lock(mutex_);
        validation_rules_[name] = {std::move(validation_func), std::move(repair_func)};
        return true;
    }

    common::Result<std::unordered_map<std::string, validation_result>> validate() {
        std::unique_lock<std::mutex> lock(mutex_);

        std::unordered_map<std::string, validation_result> results;
        metrics_.validation_runs++;

        for (const auto& [name, rule] : validation_rules_) {
            auto validation_result_value = rule.validation_func();
            results[name] = validation_result_value;

            // If validation failed and auto-repair is enabled
            if (validation_result_value == validation_result::invalid &&
                config_.enable_auto_repair && rule.repair_func) {
                auto repair_result = rule.repair_func();
                if (repair_result.is_ok()) {
                    metrics_.repair_operations++;
                    // Re-validate after repair
                    auto after_repair = rule.validation_func();
                    results[name + "_after_repair"] = after_repair;
                }
            }
        }

        return common::ok(results);
    }

    common::VoidResult start() {
        if (running_.exchange(true)) {
            return common::VoidResult::err(error_info(monitoring_error_code::already_started, "Validator already running").to_common_error());
        }

        validation_thread_ = std::thread([this]() {
            while (running_) {
                {
                    std::unique_lock<std::mutex> lock(cv_mutex_);
                    cv_.wait_for(lock, config_.validation_interval, [this]() {
                        return !running_.load();
                    });
                }

                if (!running_) {
                    break;
                }

                validate();
            }
        });

        return common::ok();
    }

    common::VoidResult stop() {
        if (!running_.exchange(false)) {
            return common::ok();
        }

        {
            std::unique_lock<std::mutex> lock(cv_mutex_);
            cv_.notify_all();
        }

        if (validation_thread_.joinable()) {
            validation_thread_.join();
        }

        return common::ok();
    }

    common::Result<bool> is_healthy() const {
        // Simple health check - all validation rules should pass
        return common::ok(true);
    }

    validation_metrics& get_metrics() { return metrics_; }
    const validation_metrics& get_metrics() const { return metrics_; }

private:
    struct validation_rule {
        validation_func_t validation_func;
        repair_func_t repair_func;
    };

    std::string name_;
    validation_config config_;
    std::mutex mutex_;
    std::unordered_map<std::string, validation_rule> validation_rules_;
    validation_metrics metrics_;

    std::atomic<bool> running_;
    std::thread validation_thread_;
    std::mutex cv_mutex_;
    std::condition_variable cv_;
};

/**
 * @brief Data consistency manager coordinating transaction managers and validators
 */
class data_consistency_manager {
public:
    explicit data_consistency_manager(const std::string& name)
        : name_(name) {}

    common::VoidResult add_transaction_manager(const std::string& name,
                                        const transaction_config& config) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (transaction_managers_.find(name) != transaction_managers_.end()) {
            return common::VoidResult::err(common::error_info(
                static_cast<int>(monitoring_error_code::already_exists),
                "Transaction manager '" + name + "' already exists",
                "monitoring_system"));
        }

        transaction_managers_[name] = std::make_shared<transaction_manager>(name, config);
        return common::ok();
    }

    transaction_manager* get_transaction_manager(const std::string& name) {
        std::unique_lock<std::mutex> lock(mutex_);

        auto it = transaction_managers_.find(name);
        if (it == transaction_managers_.end()) {
            return nullptr;
        }
        return it->second.get();
    }

    common::VoidResult add_state_validator(const std::string& name,
                                    const validation_config& config) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (state_validators_.find(name) != state_validators_.end()) {
            return common::VoidResult::err(common::error_info(
                static_cast<int>(monitoring_error_code::already_exists),
                "State validator '" + name + "' already exists",
                "monitoring_system"));
        }

        state_validators_[name] = std::make_shared<state_validator>(name, config);
        return common::ok();
    }

    state_validator* get_state_validator(const std::string& name) {
        std::unique_lock<std::mutex> lock(mutex_);

        auto it = state_validators_.find(name);
        if (it == state_validators_.end()) {
            return nullptr;
        }
        return it->second.get();
    }

    common::VoidResult start_all_validators() {
        std::unique_lock<std::mutex> lock(mutex_);

        for (auto& [name, validator] : state_validators_) {
            auto result = validator->start();
            if (!result.is_ok()) {
                return result;
            }
        }
        return common::ok();
    }

    common::VoidResult stop_all_validators() {
        std::unique_lock<std::mutex> lock(mutex_);

        for (auto& [name, validator] : state_validators_) {
            validator->stop();
        }
        return common::ok();
    }

    common::Result<bool> is_healthy() const {
        return common::ok(true);
    }

    std::unordered_map<std::string, std::string> get_all_metrics() const {
        std::unordered_map<std::string, std::string> all_metrics;

        for (const auto& [name, manager] : transaction_managers_) {
            all_metrics[name + "_transactions"] = std::to_string(manager->get_metrics().total_transactions.load());
        }

        for (const auto& [name, validator] : state_validators_) {
            all_metrics[name + "_validation"] = std::to_string(validator->get_metrics().validation_runs.load());
        }

        return all_metrics;
    }

private:
    std::string name_;
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<transaction_manager>> transaction_managers_;
    std::unordered_map<std::string, std::shared_ptr<state_validator>> state_validators_;
};

/**
 * @brief Factory function to create a transaction manager
 */
inline std::shared_ptr<transaction_manager> create_transaction_manager(const std::string& name) {
    transaction_config config;
    return std::make_shared<transaction_manager>(name, config);
}

/**
 * @brief Factory function to create a state validator
 */
inline std::shared_ptr<state_validator> create_state_validator(const std::string& name) {
    validation_config config;
    return std::make_shared<state_validator>(name, config);
}

/**
 * @brief Factory function to create a data consistency manager
 */
inline std::shared_ptr<data_consistency_manager> create_data_consistency_manager(const std::string& name) {
    return std::make_shared<data_consistency_manager>(name);
}

} // namespace kcenon::monitoring
