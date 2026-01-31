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

#include "circuit_breaker.h"
#include "retry_policy.h"
#include "error_boundary.h"

#include <any>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace kcenon::monitoring {

/**
 * @brief Fault tolerance metrics
 */
struct fault_tolerance_metrics {
    size_t total_operations = 0;
    size_t successful_operations = 0;
    size_t failed_operations = 0;
    size_t circuit_breaker_rejections = 0;
    size_t timeouts = 0;

    double get_overall_success_rate() const {
        if (total_operations == 0) {
            return 1.0;
        }
        return static_cast<double>(successful_operations) / static_cast<double>(total_operations);
    }
};

/**
 * @brief Fault tolerance configuration
 */
struct fault_tolerance_config {
    bool enable_circuit_breaker = true;
    bool enable_retry = true;
    bool circuit_breaker_first = true;
    circuit_breaker_config circuit_config;
    retry_config retry_cfg;

    bool validate() const {
        if (!enable_circuit_breaker && !enable_retry) {
            return false;
        }
        if (enable_circuit_breaker && !circuit_config.validate()) {
            return false;
        }
        if (enable_retry && !retry_cfg.validate()) {
            return false;
        }
        return true;
    }
};

/**
 * @brief Fault tolerance manager template class
 *
 * Combines circuit breaker and retry logic for resilient operations.
 *
 * @tparam T The return value type of operations
 */
template<typename T>
class fault_tolerance_manager {
public:
    fault_tolerance_manager() : name_("default"), config_() {
        initialize();
    }

    explicit fault_tolerance_manager(const std::string& name)
        : name_(name), config_() {
        initialize();
    }

    explicit fault_tolerance_manager(const std::string& name, const fault_tolerance_config& cfg)
        : name_(name), config_(cfg) {
        initialize();
    }

    /**
     * @brief Execute a function with fault tolerance
     *
     * @tparam Func The function type to execute (must return common::Result<T>)
     * @param func The function to execute
     * @return common::Result<T> containing success value or error
     */
    template<typename Func>
    common::Result<T> execute(Func&& func) {
        metrics_.total_operations++;

        common::Result<T> op_result = common::Result<T>::err(error_info(monitoring_error_code::operation_failed, "Not executed").to_common_error());

        if (config_.circuit_breaker_first) {
            op_result = execute_circuit_breaker_first(std::forward<Func>(func));
        } else {
            op_result = execute_retry_first(std::forward<Func>(func));
        }

        if (op_result.is_ok()) {
            metrics_.successful_operations++;
        } else {
            metrics_.failed_operations++;
        }

        return op_result;
    }

    /**
     * @brief Execute a function with timeout
     *
     * @tparam Func The function type to execute (must return common::Result<T>)
     * @param func The function to execute
     * @param timeout Maximum execution time
     * @return common::Result<T> containing success value or error
     */
    template<typename Func>
    common::Result<T> execute_with_timeout(Func&& func, std::chrono::milliseconds timeout) {
        metrics_.total_operations++;

        auto future_result = std::async(std::launch::async, [this, func = std::forward<Func>(func)]() mutable {
            return this->execute_internal(std::move(func));
        });

        if (future_result.wait_for(timeout) == std::future_status::timeout) {
            metrics_.timeouts++;
            metrics_.failed_operations++;
            return common::make_error<T>(static_cast<int>(monitoring_error_code::operation_timeout),
                               "Operation timed out after " + std::to_string(timeout.count()) + "ms");
        }

        auto op_result = future_result.get();
        if (op_result.is_ok()) {
            metrics_.successful_operations++;
        } else {
            metrics_.failed_operations++;
        }

        return op_result;
    }

    /**
     * @brief Check if fault tolerance manager is healthy
     * @return common::Result<bool> indicating health status
     */
    common::Result<bool> is_healthy() {
        if (config_.enable_circuit_breaker && circuit_breaker_) {
            auto state = circuit_breaker_->get_state();
            if (state == circuit_state::OPEN) {
                return common::ok(false);
            }
        }
        return common::ok(true);
    }

    /**
     * @brief Get fault tolerance metrics
     */
    fault_tolerance_metrics get_metrics() const {
        return metrics_;
    }

    /**
     * @brief Get manager name
     */
    const std::string& get_name() const {
        return name_;
    }

private:
    void initialize() {
        if (config_.enable_circuit_breaker) {
            circuit_breaker_ = std::make_unique<circuit_breaker<T>>(name_ + "_cb", config_.circuit_config);
        }
        if (config_.enable_retry) {
            retry_executor_ = std::make_unique<retry_executor<T>>(name_ + "_retry", config_.retry_cfg);
        }
    }

    template<typename Func>
    common::Result<T> execute_internal(Func&& func) {
        if (config_.circuit_breaker_first) {
            return execute_circuit_breaker_first(std::forward<Func>(func));
        }
        return execute_retry_first(std::forward<Func>(func));
    }

    template<typename Func>
    common::Result<T> execute_circuit_breaker_first(Func&& func) {
        if (config_.enable_circuit_breaker && circuit_breaker_) {
            if (config_.enable_retry && retry_executor_) {
                return circuit_breaker_->execute([this, &func]() {
                    return retry_executor_->execute(func);
                });
            }
            return circuit_breaker_->execute(std::forward<Func>(func));
        }
        if (config_.enable_retry && retry_executor_) {
            return retry_executor_->execute(std::forward<Func>(func));
        }
        return func();
    }

    template<typename Func>
    common::Result<T> execute_retry_first(Func&& func) {
        if (config_.enable_retry && retry_executor_) {
            if (config_.enable_circuit_breaker && circuit_breaker_) {
                return retry_executor_->execute([this, &func]() {
                    return circuit_breaker_->execute(func);
                });
            }
            return retry_executor_->execute(std::forward<Func>(func));
        }
        if (config_.enable_circuit_breaker && circuit_breaker_) {
            return circuit_breaker_->execute(std::forward<Func>(func));
        }
        return func();
    }

    std::string name_;
    fault_tolerance_config config_;
    std::unique_ptr<circuit_breaker<T>> circuit_breaker_;
    std::unique_ptr<retry_executor<T>> retry_executor_;
    mutable fault_tolerance_metrics metrics_;
};

/**
 * @brief Circuit breaker registry
 */
class circuit_breaker_registry {
public:
    template<typename T>
    void register_circuit_breaker(const std::string& name, std::shared_ptr<circuit_breaker<T>> breaker) {
        std::lock_guard<std::mutex> lock(mutex_);
        registry_[name] = std::move(breaker);
    }

    template<typename T>
    std::shared_ptr<circuit_breaker<T>> get_circuit_breaker(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registry_.find(name);
        if (it != registry_.end()) {
            return std::any_cast<std::shared_ptr<circuit_breaker<T>>>(it->second);
        }
        return nullptr;
    }

    void remove_circuit_breaker(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        registry_.erase(name);
    }

    std::vector<std::string> get_all_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(registry_.size());
        for (const auto& pair : registry_) {
            names.push_back(pair.first);
        }
        return names;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        registry_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::any> registry_;
};

/**
 * @brief Retry executor registry
 */
class retry_executor_registry {
public:
    template<typename T>
    void register_executor(const std::string& name, std::shared_ptr<retry_executor<T>> executor) {
        std::lock_guard<std::mutex> lock(mutex_);
        registry_[name] = std::move(executor);
    }

    template<typename T>
    std::shared_ptr<retry_executor<T>> get_executor(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registry_.find(name);
        if (it != registry_.end()) {
            return std::any_cast<std::shared_ptr<retry_executor<T>>>(it->second);
        }
        return nullptr;
    }

    void remove_executor(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        registry_.erase(name);
    }

    std::vector<std::string> get_all_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(registry_.size());
        for (const auto& pair : registry_) {
            names.push_back(pair.first);
        }
        return names;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        registry_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::any> registry_;
};

/**
 * @brief Fault tolerance manager registry
 */
class fault_tolerance_registry {
public:
    template<typename T>
    void register_manager(const std::string& name, std::shared_ptr<fault_tolerance_manager<T>> manager) {
        std::lock_guard<std::mutex> lock(mutex_);
        registry_[name] = std::move(manager);
    }

    template<typename T>
    std::shared_ptr<fault_tolerance_manager<T>> get_manager(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = registry_.find(name);
        if (it != registry_.end()) {
            return std::any_cast<std::shared_ptr<fault_tolerance_manager<T>>>(it->second);
        }
        return nullptr;
    }

    void remove_manager(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        registry_.erase(name);
    }

    std::vector<std::string> get_all_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(registry_.size());
        for (const auto& pair : registry_) {
            names.push_back(pair.first);
        }
        return names;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        registry_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::any> registry_;
};

/**
 * @brief Get global circuit breaker registry
 */
inline circuit_breaker_registry& global_circuit_breaker_registry() {
    static circuit_breaker_registry instance;
    return instance;
}

/**
 * @brief Get global retry executor registry
 */
inline retry_executor_registry& global_retry_executor_registry() {
    static retry_executor_registry instance;
    return instance;
}

/**
 * @brief Get global fault tolerance manager registry
 */
inline fault_tolerance_registry& global_fault_tolerance_registry() {
    static fault_tolerance_registry instance;
    return instance;
}

} // namespace kcenon::monitoring
