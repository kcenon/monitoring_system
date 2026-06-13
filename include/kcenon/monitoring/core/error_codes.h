#pragma once

// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.


/**
 * @file error_codes.h
 * @brief Monitoring system specific error codes
 * 
 * This file defines error codes used throughout the monitoring system,
 * following the pattern established by thread_system and logger_system.
 */

#include <cstdint>
#include <string>

namespace kcenon { namespace monitoring {

/**
 * @enum monitoring_error_code
 * @brief Comprehensive error codes for monitoring system operations
 *
 * Values are NEGATIVE and confined to the [-399, -300] range that
 * common_system reserves for monitoring_system (see
 * kcenon::common::error::category::monitoring_system and the
 * -300..-399 band documented in common's error_codes.h). Because these
 * codes are funneled into common's shared error_info.code via
 * result_types.h::to_common_error() (a plain static_cast<int>), they must
 * land in common's monitoring band so that
 * kcenon::common::error::get_category_name() classifies them as
 * "MonitoringSystem" rather than "Invalid" (any code > 0 is rejected as
 * invalid after the common classifier change). The underlying type is
 * therefore SIGNED (std::int32_t).
 *
 * The 100-slot window holds 69 distinct negative error codes plus
 * success (0); unknown_error is pinned to the band floor (-399).
 */
enum class monitoring_error_code : std::int32_t {
    // Success
    success = 0,

    // Collection errors (-300 to -306)
    collector_not_found = -300,
    collection_failed = -301,
    collector_initialization_failed = -302,
    collector_already_exists = -303,
    collector_disabled = -304,
    invalid_collector_config = -305,
    monitoring_disabled = -306,

    // Storage errors (-310 to -316)
    storage_full = -310,
    storage_corrupted = -311,
    compression_failed = -312,
    storage_not_initialized = -313,
    storage_write_failed = -314,
    storage_read_failed = -315,
    storage_empty = -316,

    // Configuration errors (-320 to -324)
    invalid_configuration = -320,
    invalid_interval = -321,
    invalid_capacity = -322,
    configuration_not_found = -323,
    configuration_parse_error = -324,

    // System errors (-330 to -335)
    system_resource_unavailable = -330,
    permission_denied = -331,
    out_of_memory = -332,
    memory_allocation_failed = -333,
    operation_timeout = -334,
    operation_cancelled = -335,

    // Integration errors (-340 to -343)
    thread_system_not_available = -340,
    logger_system_not_available = -341,
    incompatible_version = -342,
    adapter_initialization_failed = -343,

    // Metrics errors (-345 to -349)
    metric_not_found = -345,
    invalid_metric_type = -346,
    metric_overflow = -347,
    aggregation_failed = -348,
    processing_failed = -349,

    // Health check errors (-350 to -352)
    health_check_failed = -350,
    health_check_timeout = -351,
    health_check_not_registered = -352,

    // Fault tolerance errors (-355 to -364)
    circuit_breaker_open = -355,
    circuit_breaker_half_open = -356,
    retry_attempts_exhausted = -357,
    operation_failed = -358,
    network_error = -359,
    service_unavailable = -360,
    service_degraded = -361,
    error_boundary_triggered = -362,
    fallback_failed = -363,
    recovery_failed = -364,

    // General errors (-365 to -371)
    invalid_argument = -365,
    invalid_state = -366,
    not_found = -367,
    already_exists = -368,
    resource_exhausted = -369,
    already_started = -370,
    dependency_missing = -371,

    // Resource management errors (-375 to -380)
    quota_exceeded = -375,
    rate_limit_exceeded = -376,
    cpu_throttled = -377,
    memory_quota_exceeded = -378,
    bandwidth_exceeded = -379,
    resource_unavailable = -380,

    // Data consistency errors (-385 to -392)
    transaction_failed = -385,
    transaction_timeout = -386,
    transaction_aborted = -387,
    validation_failed = -388,
    data_corrupted = -389,
    state_inconsistent = -390,
    deadlock_detected = -391,
    rollback_failed = -392,

    // Unknown error (band floor)
    unknown_error = -399
};

/**
 * @brief Convert error code to string representation
 * @param code The error code to convert
 * @return String representation of the error code
 */
inline std::string error_code_to_string(monitoring_error_code code) {
    switch (code) {
        case monitoring_error_code::success:
            return "Success";
            
        // Collection errors
        case monitoring_error_code::collector_not_found:
            return "Collector not found";
        case monitoring_error_code::collection_failed:
            return "Collection failed";
        case monitoring_error_code::collector_initialization_failed:
            return "Collector initialization failed";
        case monitoring_error_code::collector_already_exists:
            return "Collector already exists";
        case monitoring_error_code::collector_disabled:
            return "Collector is disabled";
        case monitoring_error_code::invalid_collector_config:
            return "Invalid collector configuration";
        case monitoring_error_code::monitoring_disabled:
            return "Monitoring is disabled";
            
        // Storage errors
        case monitoring_error_code::storage_full:
            return "Storage is full";
        case monitoring_error_code::storage_corrupted:
            return "Storage is corrupted";
        case monitoring_error_code::compression_failed:
            return "Compression failed";
        case monitoring_error_code::storage_not_initialized:
            return "Storage not initialized";
        case monitoring_error_code::storage_write_failed:
            return "Storage write failed";
        case monitoring_error_code::storage_read_failed:
            return "Storage read failed";
        case monitoring_error_code::storage_empty:
            return "Storage is empty";
            
        // Configuration errors
        case monitoring_error_code::invalid_configuration:
            return "Invalid configuration";
        case monitoring_error_code::invalid_interval:
            return "Invalid interval";
        case monitoring_error_code::invalid_capacity:
            return "Invalid capacity";
        case monitoring_error_code::configuration_not_found:
            return "Configuration not found";
        case monitoring_error_code::configuration_parse_error:
            return "Configuration parse error";
            
        // System errors
        case monitoring_error_code::system_resource_unavailable:
            return "System resource unavailable";
        case monitoring_error_code::permission_denied:
            return "Permission denied";
        case monitoring_error_code::out_of_memory:
            return "Out of memory";
        case monitoring_error_code::memory_allocation_failed:
            return "Memory allocation failed";
        case monitoring_error_code::operation_timeout:
            return "Operation timeout";
        case monitoring_error_code::operation_cancelled:
            return "Operation cancelled";
            
        // Integration errors
        case monitoring_error_code::thread_system_not_available:
            return "Thread system not available";
        case monitoring_error_code::logger_system_not_available:
            return "Logger system not available";
        case monitoring_error_code::incompatible_version:
            return "Incompatible version";
        case monitoring_error_code::adapter_initialization_failed:
            return "Adapter initialization failed";
            
        // Metrics errors
        case monitoring_error_code::metric_not_found:
            return "Metric not found";
        case monitoring_error_code::invalid_metric_type:
            return "Invalid metric type";
        case monitoring_error_code::metric_overflow:
            return "Metric overflow";
        case monitoring_error_code::aggregation_failed:
            return "Aggregation failed";
            
        // Health check errors
        case monitoring_error_code::health_check_failed:
            return "Health check failed";
        case monitoring_error_code::health_check_timeout:
            return "Health check timeout";
        case monitoring_error_code::health_check_not_registered:
            return "Health check not registered";
            
        // Fault tolerance errors
        case monitoring_error_code::circuit_breaker_open:
            return "Circuit breaker is open";
        case monitoring_error_code::circuit_breaker_half_open:
            return "Circuit breaker is half-open";
        case monitoring_error_code::retry_attempts_exhausted:
            return "Retry attempts exhausted";
        case monitoring_error_code::operation_failed:
            return "Operation failed";
        case monitoring_error_code::network_error:
            return "Network error";
        case monitoring_error_code::service_unavailable:
            return "Service unavailable";
        case monitoring_error_code::service_degraded:
            return "Service operating in degraded mode";
        case monitoring_error_code::error_boundary_triggered:
            return "Error boundary triggered";
        case monitoring_error_code::fallback_failed:
            return "Fallback operation failed";
        case monitoring_error_code::recovery_failed:
            return "Recovery operation failed";
            
        // General errors
        case monitoring_error_code::invalid_argument:
            return "Invalid argument";
        case monitoring_error_code::invalid_state:
            return "Invalid state";
        case monitoring_error_code::not_found:
            return "Not found";
        case monitoring_error_code::already_exists:
            return "Already exists";
        case monitoring_error_code::resource_exhausted:
            return "Resource exhausted";
        case monitoring_error_code::already_started:
            return "Already started";
        case monitoring_error_code::dependency_missing:
            return "Dependency missing";

        // Resource management errors
        case monitoring_error_code::quota_exceeded:
            return "Quota exceeded";
        case monitoring_error_code::rate_limit_exceeded:
            return "Rate limit exceeded";
        case monitoring_error_code::cpu_throttled:
            return "CPU throttled";
        case monitoring_error_code::memory_quota_exceeded:
            return "Memory quota exceeded";
        case monitoring_error_code::bandwidth_exceeded:
            return "Bandwidth exceeded";
        case monitoring_error_code::resource_unavailable:
            return "Resource unavailable";
            
        // Data consistency errors
        case monitoring_error_code::transaction_failed:
            return "Transaction failed";
        case monitoring_error_code::transaction_timeout:
            return "Transaction timeout";
        case monitoring_error_code::transaction_aborted:
            return "Transaction aborted";
        case monitoring_error_code::validation_failed:
            return "Validation failed";
        case monitoring_error_code::data_corrupted:
            return "Data corrupted";
        case monitoring_error_code::state_inconsistent:
            return "State inconsistent";
        case monitoring_error_code::deadlock_detected:
            return "Deadlock detected";
        case monitoring_error_code::rollback_failed:
            return "Rollback failed";
            
        // Unknown error
        case monitoring_error_code::unknown_error:
        default:
            return "Unknown error";
    }
}

/**
 * @brief Get detailed error message
 * @param code The error code
 * @return Detailed error message with suggestions
 */
inline std::string get_error_details(monitoring_error_code code) {
    switch (code) {
        case monitoring_error_code::collector_not_found:
            return "The specified collector was not found. Check collector name and ensure it's registered.";
        case monitoring_error_code::storage_full:
            return "Storage capacity exceeded. Consider increasing buffer size or reducing collection frequency.";
        case monitoring_error_code::invalid_configuration:
            return "Configuration validation failed. Review configuration parameters and constraints.";
        case monitoring_error_code::thread_system_not_available:
            return "Thread system integration not available. Ensure thread_system is properly linked.";
        case monitoring_error_code::circuit_breaker_open:
            return "Circuit breaker is open, rejecting calls to protect downstream services. Wait for recovery or check service health.";
        case monitoring_error_code::retry_attempts_exhausted:
            return "All retry attempts have been exhausted. The operation failed permanently. Check service availability and error conditions.";
        case monitoring_error_code::operation_failed:
            return "The requested operation failed. Check service status, network connectivity, and input parameters.";
        case monitoring_error_code::service_degraded:
            return "Service is operating in degraded mode due to detected issues. Some features may be unavailable.";
        case monitoring_error_code::error_boundary_triggered:
            return "Error boundary has been triggered to prevent error propagation. Check upstream service health.";
        case monitoring_error_code::fallback_failed:
            return "Both primary operation and fallback mechanism failed. Check alternative service configurations.";
        case monitoring_error_code::quota_exceeded:
            return "Resource quota has been exceeded. Reduce resource consumption or increase quota limits.";
        case monitoring_error_code::rate_limit_exceeded:
            return "Rate limit has been exceeded. Reduce request frequency or increase rate limits.";
        case monitoring_error_code::cpu_throttled:
            return "Operation has been throttled due to high CPU usage. Reduce system load or adjust CPU limits.";
        case monitoring_error_code::memory_quota_exceeded:
            return "Memory quota has been exceeded. Free memory or increase memory quota limits.";
        case monitoring_error_code::bandwidth_exceeded:
            return "Bandwidth quota has been exceeded. Reduce data transfer or increase bandwidth limits.";
        case monitoring_error_code::resource_unavailable:
            return "Required resource is currently unavailable. Try again later or check resource status.";
        case monitoring_error_code::transaction_failed:
            return "Transaction failed to complete successfully. Check operation prerequisites and system state.";
        case monitoring_error_code::transaction_timeout:
            return "Transaction exceeded its timeout limit. Consider increasing timeout or reducing transaction scope.";
        case monitoring_error_code::transaction_aborted:
            return "Transaction was aborted due to conflicts or errors. Review transaction operations and retry.";
        case monitoring_error_code::validation_failed:
            return "Data validation failed. Check data integrity and consistency requirements.";
        case monitoring_error_code::data_corrupted:
            return "Data corruption detected. Run data repair operations or restore from backup.";
        case monitoring_error_code::state_inconsistent:
            return "System state is inconsistent across components. Synchronization or recovery needed.";
        case monitoring_error_code::deadlock_detected:
            return "Deadlock detected in transaction processing. Review locking strategy and transaction ordering.";
        case monitoring_error_code::rollback_failed:
            return "Transaction rollback failed. Manual cleanup may be required to restore consistent state.";
        default:
            return error_code_to_string(code);
    }
}

} } // namespace kcenon::monitoring