#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file error_codes.h
 * @brief Monitoring system specific error codes
 * 
 * This file defines error codes used throughout the monitoring system,
 * following the pattern established by thread_system and logger_system.
 */

#include <cstdint>
#include <string>

namespace monitoring_system {

/**
 * @enum monitoring_error_code
 * @brief Comprehensive error codes for monitoring system operations
 */
enum class monitoring_error_code : std::uint32_t {
    // Success
    success = 0,
    
    // Collection errors (1000-1999)
    collector_not_found = 1000,
    collection_failed = 1001,
    collector_initialization_failed = 1002,
    collector_already_exists = 1003,
    collector_disabled = 1004,
    invalid_collector_config = 1005,
    monitoring_disabled = 1006,
    
    // Storage errors (2000-2999)
    storage_full = 2000,
    storage_corrupted = 2001,
    compression_failed = 2002,
    storage_not_initialized = 2003,
    storage_write_failed = 2004,
    storage_read_failed = 2005,
    storage_empty = 2006,
    
    // Configuration errors (3000-3999)
    invalid_configuration = 3000,
    invalid_interval = 3001,
    invalid_capacity = 3002,
    configuration_not_found = 3003,
    configuration_parse_error = 3004,
    
    // System errors (4000-4999)
    system_resource_unavailable = 4000,
    permission_denied = 4001,
    out_of_memory = 4002,
    operation_timeout = 4003,
    operation_cancelled = 4004,
    
    // Integration errors (5000-5999)
    thread_system_not_available = 5000,
    logger_system_not_available = 5001,
    incompatible_version = 5002,
    adapter_initialization_failed = 5003,
    
    // Metrics errors (6000-6999)
    metric_not_found = 6000,
    invalid_metric_type = 6001,
    metric_overflow = 6002,
    aggregation_failed = 6003,
    
    // Health check errors (7000-7999)
    health_check_failed = 7000,
    health_check_timeout = 7001,
    health_check_not_registered = 7002,
    
    // Unknown error
    unknown_error = 9999
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
        default:
            return error_code_to_string(code);
    }
}

} // namespace monitoring_system