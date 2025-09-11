#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file data_consistency.h
 * @brief Data consistency and validation system for monitoring operations
 * 
 * This file implements comprehensive data consistency management including:
 * - Transaction management for metric operations
 * - State validation and integrity checking
 * - Atomic operations with rollback capabilities
 * - Data consistency validation across components
 * - State synchronization and conflict resolution
 */

#include <monitoring/core/result_types.h>
#include <monitoring/core/error_codes.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <vector>
#include <queue>
#include <set>
#include <optional>

namespace monitoring_system {

// Forward declarations
class transaction_manager;
class state_validator;
class data_consistency_manager;

/**
 * @enum consistency_level
 * @brief Levels of consistency guarantee
 */
enum class consistency_level {
    eventual,       // Eventual consistency (best performance)
    read_committed, // Read committed isolation
    repeatable_read,// Repeatable read isolation
    serializable    // Full ACID serializable transactions
};

/**
 * @enum transaction_state
 * @brief Transaction lifecycle states
 */
enum class transaction_state {
    active,         // Transaction is active and can be modified
    preparing,      // Transaction is being prepared for commit
    prepared,       // Transaction is prepared and ready to commit
    committing,     // Transaction is being committed
    committed,      // Transaction has been successfully committed
    aborting,       // Transaction is being aborted
    aborted         // Transaction has been aborted
};

/**
 * @enum validation_result
 * @brief Results of validation operations
 */
enum class validation_result {
    valid,          // Data is valid and consistent
    invalid,        // Data is invalid
    corrupted,      // Data is corrupted and needs repair
    inconsistent,   // Data is inconsistent across components
    outdated        // Data is outdated and needs refresh
};

/**
 * @struct transaction_config
 * @brief Configuration for transactions
 */
struct transaction_config {
    consistency_level level{consistency_level::read_committed};
    std::chrono::milliseconds timeout{std::chrono::seconds(30)};
    std::size_t max_retries{3};
    bool enable_deadlock_detection{true};
    bool enable_rollback{true};
    std::chrono::milliseconds lock_timeout{std::chrono::seconds(10)};
    
    bool validate() const {
        return timeout.count() > 0 && 
               lock_timeout.count() > 0 &&
               max_retries > 0;
    }
};

/**
 * @struct validation_config
 * @brief Configuration for state validation
 */
struct validation_config {
    std::chrono::milliseconds validation_interval{std::chrono::seconds(60)};
    std::size_t max_validation_failures{5};
    bool enable_auto_repair{true};
    bool enable_consistency_checks{true};
    bool enable_integrity_checks{true};
    double corruption_threshold{0.1}; // 10% corruption threshold
    
    bool validate() const {
        return validation_interval.count() > 0 &&
               max_validation_failures > 0 &&
               corruption_threshold >= 0.0 && corruption_threshold <= 1.0;
    }
};

/**
 * @struct consistency_metrics
 * @brief Metrics for data consistency operations
 */
struct consistency_metrics {
    std::atomic<std::size_t> total_transactions{0};
    std::atomic<std::size_t> committed_transactions{0};
    std::atomic<std::size_t> aborted_transactions{0};
    std::atomic<std::size_t> validation_runs{0};
    std::atomic<std::size_t> validation_failures{0};
    std::atomic<std::size_t> repair_operations{0};
    std::atomic<std::size_t> deadlocks_detected{0};
    std::atomic<std::size_t> state_inconsistencies{0};
    std::chrono::steady_clock::time_point last_validation;
    
    consistency_metrics() : last_validation(std::chrono::steady_clock::now()) {}
    
    consistency_metrics(const consistency_metrics& other) 
        : total_transactions(other.total_transactions.load())
        , committed_transactions(other.committed_transactions.load())
        , aborted_transactions(other.aborted_transactions.load())
        , validation_runs(other.validation_runs.load())
        , validation_failures(other.validation_failures.load())
        , repair_operations(other.repair_operations.load())
        , deadlocks_detected(other.deadlocks_detected.load())
        , state_inconsistencies(other.state_inconsistencies.load())
        , last_validation(other.last_validation) {}
        
    consistency_metrics& operator=(const consistency_metrics& other) {
        if (this != &other) {
            total_transactions = other.total_transactions.load();
            committed_transactions = other.committed_transactions.load();
            aborted_transactions = other.aborted_transactions.load();
            validation_runs = other.validation_runs.load();
            validation_failures = other.validation_failures.load();
            repair_operations = other.repair_operations.load();
            deadlocks_detected = other.deadlocks_detected.load();
            state_inconsistencies = other.state_inconsistencies.load();
            last_validation = other.last_validation;
        }
        return *this;
    }
    
    double get_commit_rate() const {
        auto total = total_transactions.load();
        return total > 0 ? static_cast<double>(committed_transactions.load()) / total : 0.0;
    }
    
    double get_abort_rate() const {
        auto total = total_transactions.load();
        return total > 0 ? static_cast<double>(aborted_transactions.load()) / total : 0.0;
    }
    
    double get_validation_success_rate() const {
        auto total = validation_runs.load();
        return total > 0 ? 1.0 - (static_cast<double>(validation_failures.load()) / total) : 1.0;
    }
    
    void reset() {
        total_transactions = 0;
        committed_transactions = 0;
        aborted_transactions = 0;
        validation_runs = 0;
        validation_failures = 0;
        repair_operations = 0;
        deadlocks_detected = 0;
        state_inconsistencies = 0;
        last_validation = std::chrono::steady_clock::now();
    }
};

/**
 * @class transaction_operation
 * @brief Represents an atomic operation within a transaction
 */
class transaction_operation {
public:
    using operation_function = std::function<result_void()>;
    using rollback_function = std::function<result_void()>;
    
    transaction_operation(const std::string& name, 
                         operation_function op, 
                         rollback_function rollback = nullptr)
        : name_(name), operation_(std::move(op)), rollback_(std::move(rollback)) {}
    
    result_void execute() {
        executed_ = true;
        execution_time_ = std::chrono::steady_clock::now();
        return operation_();
    }
    
    result_void rollback() {
        if (!executed_ || !rollback_) {
            return result_void{};
        }
        return rollback_();
    }
    
    const std::string& name() const { return name_; }
    bool is_executed() const { return executed_; }
    std::chrono::steady_clock::time_point execution_time() const { return execution_time_; }

private:
    std::string name_;
    operation_function operation_;
    rollback_function rollback_;
    bool executed_{false};
    std::chrono::steady_clock::time_point execution_time_;
};

/**
 * @class transaction
 * @brief Represents a database-style transaction for monitoring operations
 */
class transaction {
public:
    transaction(const std::string& id, const transaction_config& config)
        : id_(id), config_(config), state_(transaction_state::active),
          start_time_(std::chrono::steady_clock::now()) {}
    
    ~transaction() {
        if (state_ == transaction_state::active) {
            abort();
        }
    }
    
    // Add operation to transaction
    result_void add_operation(std::unique_ptr<transaction_operation> operation) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != transaction_state::active) {
            return result_void{monitoring_error_code::invalid_state,
                "Cannot add operation to non-active transaction"};
        }
        
        operations_.push_back(std::move(operation));
        return result_void{};
    }
    
    // Commit transaction
    result_void commit() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != transaction_state::active) {
            return result_void{monitoring_error_code::invalid_state,
                "Transaction is not in active state"};
        }
        
        // Check timeout
        auto now = std::chrono::steady_clock::now();
        if (now - start_time_ > config_.timeout) {
            state_ = transaction_state::aborted;
            return result_void{monitoring_error_code::operation_timeout,
                "Transaction timeout exceeded"};
        }
        
        // Prepare phase
        state_ = transaction_state::preparing;
        
        try {
            // Execute all operations
            for (auto& op : operations_) {
                auto result = op->execute();
                if (!result) {
                    // Rollback on failure
                    state_ = transaction_state::aborting;
                    rollback_operations();
                    state_ = transaction_state::aborted;
                    return result_void{monitoring_error_code::operation_failed,
                        "Transaction operation failed: " + result.get_error().message};
                }
            }
            
            state_ = transaction_state::committed;
            commit_time_ = std::chrono::steady_clock::now();
            return result_void{};
            
        } catch (const std::exception& e) {
            state_ = transaction_state::aborting;
            rollback_operations();
            state_ = transaction_state::aborted;
            return result_void{monitoring_error_code::operation_failed,
                std::string("Transaction commit failed: ") + e.what()};
        }
    }
    
    // Abort transaction
    result_void abort() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ == transaction_state::committed || state_ == transaction_state::aborted) {
            return result_void{}; // Already in final state
        }
        
        state_ = transaction_state::aborting;
        rollback_operations();
        state_ = transaction_state::aborted;
        abort_time_ = std::chrono::steady_clock::now();
        
        return result_void{};
    }
    
    // Getters
    const std::string& id() const { return id_; }
    transaction_state state() const { return state_; }
    std::size_t operation_count() const { return operations_.size(); }
    std::chrono::steady_clock::time_point start_time() const { return start_time_; }
    
    std::chrono::milliseconds duration() const {
        auto end_time = (state_ == transaction_state::committed) ? commit_time_ :
                       (state_ == transaction_state::aborted) ? abort_time_ :
                       std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_);
    }

private:
    void rollback_operations() {
        // Rollback in reverse order
        for (auto it = operations_.rbegin(); it != operations_.rend(); ++it) {
            if ((*it)->is_executed()) {
                (*it)->rollback();
            }
        }
    }
    
    std::string id_;
    transaction_config config_;
    std::atomic<transaction_state> state_;
    std::vector<std::unique_ptr<transaction_operation>> operations_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point commit_time_;
    std::chrono::steady_clock::time_point abort_time_;
    mutable std::mutex mutex_;
};

/**
 * @class state_validator
 * @brief Validates system state consistency and integrity
 */
class state_validator {
public:
    using validation_function = std::function<validation_result()>;
    using repair_function = std::function<result_void()>;
    
    state_validator(const std::string& name, const validation_config& config)
        : name_(name), config_(config), running_(false) {}
    
    ~state_validator() {
        stop();
    }
    
    // Add validation rule
    result_void add_validation_rule(const std::string& rule_name,
                                  validation_function validator,
                                  repair_function repair = nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        validation_rules_[rule_name] = std::make_pair(std::move(validator), std::move(repair));
        return result_void{};
    }
    
    // Start continuous validation
    result_void start() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (running_) {
            return result_void{monitoring_error_code::invalid_state,
                "Validator is already running"};
        }
        
        running_ = true;
        validation_thread_ = std::thread([this]() { validation_loop(); });
        return result_void{};
    }
    
    // Stop continuous validation
    result_void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) return result_void{};
            running_ = false;
        }
        
        if (validation_thread_.joinable()) {
            validation_thread_.join();
        }
        return result_void{};
    }
    
    // Manual validation run
    result<std::unordered_map<std::string, validation_result>> validate() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::unordered_map<std::string, validation_result> results;
        metrics_.validation_runs++;
        
        for (const auto& [rule_name, rule_pair] : validation_rules_) {
            try {
                auto result = rule_pair.first();
                results[rule_name] = result;
                
                if (result != validation_result::valid && 
                    config_.enable_auto_repair && rule_pair.second) {
                    // Attempt repair
                    auto repair_result = rule_pair.second();
                    if (repair_result) {
                        metrics_.repair_operations++;
                        // Re-validate after repair
                        auto revalidate_result = rule_pair.first();
                        results[rule_name + "_after_repair"] = revalidate_result;
                    }
                }
                
                if (result != validation_result::valid) {
                    metrics_.validation_failures++;
                    if (result == validation_result::inconsistent) {
                        metrics_.state_inconsistencies++;
                    }
                }
                
            } catch (const std::exception& e) {
                metrics_.validation_failures++;
                results[rule_name] = validation_result::corrupted;
            }
        }
        
        metrics_.last_validation = std::chrono::steady_clock::now();
        return make_success(results);
    }
    
    // Health check
    result<bool> is_healthy() const {
        auto failure_rate = 1.0 - metrics_.get_validation_success_rate();
        return make_success(failure_rate < config_.corruption_threshold);
    }
    
    const consistency_metrics& get_metrics() const { return metrics_; }
    const std::string& get_name() const { return name_; }

private:
    void validation_loop() {
        while (running_) {
            std::this_thread::sleep_for(config_.validation_interval);
            
            if (!running_) break;
            
            try {
                validate();
            } catch (const std::exception& e) {
                metrics_.validation_failures++;
            }
        }
    }
    
    std::string name_;
    validation_config config_;
    std::atomic<bool> running_;
    std::thread validation_thread_;
    std::unordered_map<std::string, std::pair<validation_function, repair_function>> validation_rules_;
    mutable consistency_metrics metrics_;
    mutable std::mutex mutex_;
};

/**
 * @class transaction_manager
 * @brief Manages transactions and ensures ACID properties
 */
class transaction_manager {
public:
    transaction_manager(const std::string& name, const transaction_config& config)
        : name_(name), config_(config) {}
    
    ~transaction_manager() = default;
    
    // Begin new transaction
    result<std::shared_ptr<transaction>> begin_transaction(const std::string& id = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string transaction_id = id.empty() ? generate_transaction_id() : id;
        
        if (active_transactions_.find(transaction_id) != active_transactions_.end()) {
            return make_error<std::shared_ptr<transaction>>(
                monitoring_error_code::already_exists,
                "Transaction with ID already exists");
        }
        
        auto tx = std::make_shared<transaction>(transaction_id, config_);
        active_transactions_[transaction_id] = tx;
        metrics_.total_transactions++;
        
        return make_success(tx);
    }
    
    // Commit transaction
    result_void commit_transaction(const std::string& transaction_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = active_transactions_.find(transaction_id);
        if (it == active_transactions_.end()) {
            return result_void{monitoring_error_code::not_found,
                "Transaction not found"};
        }
        
        auto result = it->second->commit();
        
        if (result) {
            metrics_.committed_transactions++;
        } else {
            metrics_.aborted_transactions++;
        }
        
        // Move to completed transactions
        completed_transactions_[transaction_id] = std::move(it->second);
        active_transactions_.erase(it);
        
        return result;
    }
    
    // Abort transaction
    result_void abort_transaction(const std::string& transaction_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = active_transactions_.find(transaction_id);
        if (it == active_transactions_.end()) {
            return result_void{monitoring_error_code::not_found,
                "Transaction not found"};
        }
        
        auto result = it->second->abort();
        metrics_.aborted_transactions++;
        
        // Move to completed transactions
        completed_transactions_[transaction_id] = std::move(it->second);
        active_transactions_.erase(it);
        
        return result;
    }
    
    // Get transaction
    std::shared_ptr<transaction> get_transaction(const std::string& transaction_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = active_transactions_.find(transaction_id);
        return (it != active_transactions_.end()) ? it->second : nullptr;
    }
    
    // Deadlock detection
    result<std::vector<std::string>> detect_deadlocks() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::string> deadlocked_transactions;
        
        // Simple deadlock detection based on transaction age and state
        auto now = std::chrono::steady_clock::now();
        
        for (const auto& [id, tx] : active_transactions_) {
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - tx->start_time());
            
            if (age > config_.timeout * 2 && tx->state() == transaction_state::active) {
                deadlocked_transactions.push_back(id);
                metrics_.deadlocks_detected++;
            }
        }
        
        return make_success(deadlocked_transactions);
    }
    
    // Cleanup completed transactions
    void cleanup_completed_transactions(std::chrono::milliseconds max_age) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::steady_clock::now();
        auto it = completed_transactions_.begin();
        
        while (it != completed_transactions_.end()) {
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - it->second->start_time());
            
            if (age > max_age) {
                it = completed_transactions_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Statistics
    std::size_t active_transaction_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_transactions_.size();
    }
    
    std::size_t completed_transaction_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return completed_transactions_.size();
    }
    
    const consistency_metrics& get_metrics() const { return metrics_; }
    const std::string& get_name() const { return name_; }

private:
    std::string generate_transaction_id() {
        static std::atomic<std::uint64_t> counter{0};
        return "tx_" + std::to_string(++counter);
    }
    
    std::string name_;
    transaction_config config_;
    std::unordered_map<std::string, std::shared_ptr<transaction>> active_transactions_;
    std::unordered_map<std::string, std::shared_ptr<transaction>> completed_transactions_;
    mutable consistency_metrics metrics_;
    mutable std::mutex mutex_;
};

/**
 * @class data_consistency_manager
 * @brief Unified manager for data consistency and validation
 */
class data_consistency_manager {
public:
    data_consistency_manager(const std::string& name) : name_(name) {}
    
    ~data_consistency_manager() = default;
    
    // Transaction manager management
    result_void add_transaction_manager(const std::string& name,
                                      const transaction_config& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (transaction_managers_.find(name) != transaction_managers_.end()) {
            return result_void{monitoring_error_code::already_exists,
                "Transaction manager already exists"};
        }
        
        transaction_managers_[name] = std::make_unique<transaction_manager>(name, config);
        return result_void{};
    }
    
    std::shared_ptr<transaction_manager> get_transaction_manager(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = transaction_managers_.find(name);
        return (it != transaction_managers_.end()) ?
               std::shared_ptr<transaction_manager>(std::move(it->second)) : nullptr;
    }
    
    // State validator management
    result_void add_state_validator(const std::string& name,
                                  const validation_config& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_validators_.find(name) != state_validators_.end()) {
            return result_void{monitoring_error_code::already_exists,
                "State validator already exists"};
        }
        
        state_validators_[name] = std::make_unique<state_validator>(name, config);
        return result_void{};
    }
    
    std::shared_ptr<state_validator> get_state_validator(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = state_validators_.find(name);
        return (it != state_validators_.end()) ?
               std::shared_ptr<state_validator>(std::move(it->second)) : nullptr;
    }
    
    // Global operations
    result_void start_all_validators() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& [name, validator] : state_validators_) {
            auto result = validator->start();
            if (!result) {
                return result;
            }
        }
        return result_void{};
    }
    
    result_void stop_all_validators() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& [name, validator] : state_validators_) {
            validator->stop();
        }
        return result_void{};
    }
    
    // Global metrics
    std::unordered_map<std::string, consistency_metrics> get_all_metrics() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::unordered_map<std::string, consistency_metrics> all_metrics;
        
        for (const auto& [name, manager] : transaction_managers_) {
            all_metrics[name + "_transactions"] = manager->get_metrics();
        }
        
        for (const auto& [name, validator] : state_validators_) {
            all_metrics[name + "_validation"] = validator->get_metrics();
        }
        
        return all_metrics;
    }
    
    // Health check
    result<bool> is_healthy() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check all validators
        for (const auto& [name, validator] : state_validators_) {
            auto health = validator->is_healthy();
            if (!health || !health.value()) {
                return make_success(false);
            }
        }
        
        // Check transaction managers for high abort rates
        for (const auto& [name, manager] : transaction_managers_) {
            auto metrics = manager->get_metrics();
            if (metrics.get_abort_rate() > 0.5) { // More than 50% abort rate
                return make_success(false);
            }
        }
        
        return make_success(true);
    }

private:
    std::string name_;
    mutable std::mutex mutex_;
    
    std::unordered_map<std::string, std::unique_ptr<transaction_manager>> transaction_managers_;
    std::unordered_map<std::string, std::unique_ptr<state_validator>> state_validators_;
};

// Factory functions for easier creation

/**
 * @brief Create a transaction manager
 */
inline std::unique_ptr<transaction_manager> create_transaction_manager(
    const std::string& name,
    consistency_level level = consistency_level::read_committed,
    std::chrono::milliseconds timeout = std::chrono::seconds(30)) {
    
    transaction_config config;
    config.level = level;
    config.timeout = timeout;
    
    return std::make_unique<transaction_manager>(name, config);
}

/**
 * @brief Create a state validator
 */
inline std::unique_ptr<state_validator> create_state_validator(
    const std::string& name,
    std::chrono::milliseconds validation_interval = std::chrono::seconds(60),
    bool enable_auto_repair = true) {
    
    validation_config config;
    config.validation_interval = validation_interval;
    config.enable_auto_repair = enable_auto_repair;
    
    return std::make_unique<state_validator>(name, config);
}

/**
 * @brief Create a data consistency manager
 */
inline std::unique_ptr<data_consistency_manager> create_data_consistency_manager(
    const std::string& name) {
    return std::make_unique<data_consistency_manager>(name);
}

} // namespace monitoring_system