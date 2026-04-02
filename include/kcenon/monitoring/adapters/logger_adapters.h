#pragma once

// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.


/**
 * @file logger_adapters.h
 * @brief Consolidated logger system adapters for monitoring_system
 *
 * Provides logger_to_monitoring_adapter and the logger_system_adapter alias.
 * Include this header to get all logger adapter functionality.
 */

#include "logger_to_monitoring_adapter.h"

namespace kcenon { namespace monitoring {

// Backward-compatible alias
using logger_system_adapter = logger_to_monitoring_adapter;

} } // namespace kcenon::monitoring
