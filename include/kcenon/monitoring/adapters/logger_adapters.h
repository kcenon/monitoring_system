#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file logger_adapters.h
 * @brief Consolidated logger system adapters for monitoring_system
 *
 * This header consolidates:
 * - logger_to_monitoring_adapter.h (logger -> monitoring integration)
 * - logger_system_adapter.h (backward-compatible alias)
 *
 * Include this header to get all logger adapter functionality.
 */

#include "logger_to_monitoring_adapter.h"

namespace kcenon { namespace monitoring {

// Backward-compatible alias
using logger_system_adapter = logger_to_monitoring_adapter;

} } // namespace kcenon::monitoring
