#pragma once

/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, monitoring_system contributors
All rights reserved.
*****************************************************************************/

/**
 * @file thread_adapters.h
 * @brief Consolidated thread system adapters for monitoring_system
 *
 * This header consolidates:
 * - thread_to_monitoring_adapter.h (thread_system -> monitoring integration)
 * - thread_system_adapter.h (backward-compatible alias)
 *
 * Include this header to get all thread system adapter functionality.
 */

#include "thread_to_monitoring_adapter.h"

namespace kcenon { namespace monitoring {

// Backward-compatible alias
using thread_system_adapter = thread_to_monitoring_adapter;

} } // namespace kcenon::monitoring
