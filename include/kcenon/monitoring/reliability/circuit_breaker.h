// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file circuit_breaker.h
 * @brief Circuit breaker integration for monitoring_system.
 *
 * Re-exports common_system's circuit breaker types for use within
 * monitoring_system. Use kcenon::common::resilience::circuit_breaker directly.
 */

#include <kcenon/common/resilience/circuit_breaker.h>
#include <kcenon/common/resilience/circuit_state.h>
#include <kcenon/common/resilience/circuit_breaker_config.h>

#include "kcenon/monitoring/core/result_types.h"

namespace kcenon::monitoring {

// Re-export common_system types for convenience
using circuit_state = common::resilience::circuit_state;
using circuit_breaker_config = common::resilience::circuit_breaker_config;
using circuit_breaker = common::resilience::circuit_breaker;

} // namespace kcenon::monitoring
