#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file common_adapters.h
 * @brief Consolidated adapters for common_system <-> monitoring_system interop
 *
 * This header consolidates the bidirectional adapter pair:
 * - monitoring_to_common_adapter.h (monitoring_system -> common_system interface)
 * - common_to_monitoring_adapter.h (common_system -> monitoring_system interface)
 *
 * Both files define adapters under #if KCENON_HAS_COMMON_SYSTEM guard.
 * Include this header to get all common_system adapter functionality.
 *
 * @note Due to overlapping class names (monitor_from_common_adapter,
 * common_monitor_factory), each direction is conditionally compiled.
 * If you need only one direction, include the specific header directly.
 */

#include "monitoring_to_common_adapter.h"

// common_to_monitoring_adapter.h provides additional adapters:
//   - common_system_monitor_adapter (exposes monitoring_system as common IMonitor)
//   - common_system_monitorable_adapter (exposes monitorable as common IMonitorable)
//   - monitor_from_common_adapter (wraps common IMonitor as metrics_collector)
//
// Note: common_to_monitoring_adapter.h and monitoring_to_common_adapter.h
// define classes with the same name (monitor_from_common_adapter,
// common_monitor_factory). Only one can be included per translation unit.
// By default, this header includes monitoring_to_common_adapter.h which
// provides the more complete adapter set.
//
// To use common_to_monitoring_adapter.h instead, include it directly.
