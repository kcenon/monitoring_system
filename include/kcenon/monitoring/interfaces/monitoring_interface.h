#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file monitoring_interface.h
 * @brief Deprecated forwarding header for monitoring core interfaces
 *
 * DEPRECATED: This header is deprecated and will be removed in version 3.0.0.
 * Please use <kcenon/monitoring/interfaces/monitoring_core.h> instead.
 *
 * This file previously contained the core monitoring interfaces but was
 * renamed to monitoring_core.h to avoid naming collision with common_system's
 * monitoring_interface.h which defines the canonical IMonitor interface.
 *
 * Migration guide:
 * - Replace #include <kcenon/monitoring/interfaces/monitoring_interface.h>
 *   with #include <kcenon/monitoring/interfaces/monitoring_core.h>
 *
 * Note on interface disambiguation:
 * - kcenon/common/interfaces/monitoring_interface.h: IMonitor interface
 *   (standard monitoring interface for all systems)
 * - kcenon/monitoring/interfaces/monitoring_core.h: monitoring_interface class
 *   (extended monitoring system with collectors, storage, analyzers)
 */

// Deprecation warning
#if defined(__GNUC__) || defined(__clang__)
#warning "kcenon/monitoring/interfaces/monitoring_interface.h is deprecated. Use <kcenon/monitoring/interfaces/monitoring_core.h> instead."
#elif defined(_MSC_VER)
#pragma message("Warning: kcenon/monitoring/interfaces/monitoring_interface.h is deprecated. Use <kcenon/monitoring/interfaces/monitoring_core.h> instead.")
#endif

// Forward to the new header
#include "monitoring_core.h"
