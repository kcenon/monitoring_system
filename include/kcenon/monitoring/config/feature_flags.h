// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file feature_flags.h
 * @brief Unified feature flags header for monitoring_system
 *
 * This is the main entry point for all feature detection and integration
 * flags in the monitoring_system library. Include this header to get access
 * to all KCENON_* feature macros.
 *
 * Header organization:
 * - Includes common_system's feature_flags.h for base feature detection
 * - Defines KCENON_HAS_COMMON_SYSTEM to indicate common_system availability
 * - Provides legacy aliases for backward compatibility
 *
 * Usage:
 * @code
 * #include <kcenon/monitoring/config/feature_flags.h>
 *
 * #if KCENON_HAS_COMMON_SYSTEM
 *     #include <kcenon/common/concepts/event.h>
 *     // Use common_system features
 * #endif
 * @endcode
 *
 * @see common_system feature_flags.h for base feature detection
 */

#pragma once

//==============================================================================
// Common System Integration
//==============================================================================

// Include common_system's feature flags if available
// This provides access to all KCENON_* feature macros from common_system
#if __has_include(<kcenon/common/config/feature_flags.h>)
    #include <kcenon/common/config/feature_flags.h>
    #ifndef KCENON_HAS_COMMON_SYSTEM
        #define KCENON_HAS_COMMON_SYSTEM 1
    #endif
#else
    #ifndef KCENON_HAS_COMMON_SYSTEM
        #define KCENON_HAS_COMMON_SYSTEM 0
    #endif
#endif

//==============================================================================
// Monitoring System Feature Flags
//==============================================================================

/**
 * @brief Enable legacy alias support (default: 1)
 *
 * When enabled, legacy macro names like BUILD_WITH_COMMON_SYSTEM are
 * defined as aliases to the new KCENON_* macros. Disable this to
 * ensure clean migration to the new naming convention.
 */
#ifndef KCENON_MONITORING_ENABLE_LEGACY_ALIASES
    #define KCENON_MONITORING_ENABLE_LEGACY_ALIASES 1
#endif

//==============================================================================
// Legacy Aliases
//==============================================================================

/**
 * @brief Legacy alias definitions for backward compatibility
 *
 * These aliases map old macro names to the new KCENON_* convention.
 * They are only defined if:
 * 1. KCENON_MONITORING_ENABLE_LEGACY_ALIASES is enabled (default: 1)
 * 2. The legacy macro is not already explicitly defined
 *
 * This ensures that explicit user definitions (including explicit 0 values)
 * are respected and not overwritten.
 *
 * @note Legacy aliases are planned for deprecation in v0.4.0 and
 *       removal in v1.0.0. Migrate to KCENON_* macros.
 */
#if KCENON_MONITORING_ENABLE_LEGACY_ALIASES

//------------------------------------------------------------------------------
// Common system integration aliases
//------------------------------------------------------------------------------

// BUILD_WITH_COMMON_SYSTEM -> KCENON_HAS_COMMON_SYSTEM
#ifndef BUILD_WITH_COMMON_SYSTEM
    #if KCENON_HAS_COMMON_SYSTEM
        #define BUILD_WITH_COMMON_SYSTEM 1
    #endif
#endif

// MONITORING_USING_COMMON_INTERFACES -> KCENON_HAS_COMMON_SYSTEM
#ifndef MONITORING_USING_COMMON_INTERFACES
    #if KCENON_HAS_COMMON_SYSTEM
        #define MONITORING_USING_COMMON_INTERFACES 1
    #endif
#endif

//------------------------------------------------------------------------------
// Logger system integration aliases
//------------------------------------------------------------------------------

// BUILD_WITH_LOGGER_SYSTEM -> KCENON_WITH_LOGGER_SYSTEM (from common_system)
#ifndef BUILD_WITH_LOGGER_SYSTEM
    #if defined(KCENON_WITH_LOGGER_SYSTEM) && KCENON_WITH_LOGGER_SYSTEM
        #define BUILD_WITH_LOGGER_SYSTEM 1
    #endif
#endif

#endif // KCENON_MONITORING_ENABLE_LEGACY_ALIASES

//==============================================================================
// Feature Detection Helpers
//==============================================================================

/**
 * @brief Check if common_system concepts are available
 *
 * This macro evaluates to 1 if both common_system is available and
 * C++20 concepts are supported.
 */
#ifndef KCENON_HAS_COMMON_CONCEPTS
    #if KCENON_HAS_COMMON_SYSTEM && defined(KCENON_HAS_CONCEPTS) && KCENON_HAS_CONCEPTS
        #define KCENON_HAS_COMMON_CONCEPTS 1
    #else
        #define KCENON_HAS_COMMON_CONCEPTS 0
    #endif
#endif

/**
 * @brief Check if common_system DI container is available
 */
#ifndef KCENON_HAS_COMMON_DI
    #if KCENON_HAS_COMMON_SYSTEM
        #define KCENON_HAS_COMMON_DI 1
    #else
        #define KCENON_HAS_COMMON_DI 0
    #endif
#endif

/**
 * @brief Check if common_system ILogger interface is available
 */
#ifndef KCENON_HAS_COMMON_ILOGGER
    #if KCENON_HAS_COMMON_SYSTEM
        #define KCENON_HAS_COMMON_ILOGGER 1
    #else
        #define KCENON_HAS_COMMON_ILOGGER 0
    #endif
#endif

/**
 * @brief Check if common_system IMonitor interface is available
 */
#ifndef KCENON_HAS_COMMON_IMONITOR
    #if KCENON_HAS_COMMON_SYSTEM
        #define KCENON_HAS_COMMON_IMONITOR 1
    #else
        #define KCENON_HAS_COMMON_IMONITOR 0
    #endif
#endif
