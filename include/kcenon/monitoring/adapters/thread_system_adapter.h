#pragma once

// Define guard so compatibility shim does not try to include a non‑existent path
#ifndef KCENON_MONITORING_ADAPTERS_THREAD_SYSTEM_ADAPTER_H
#define KCENON_MONITORING_ADAPTERS_THREAD_SYSTEM_ADAPTER_H

/*
 * BSD 3-Clause License
 * Copyright (c) 2025, monitoring_system contributors
 */

// DEPRECATED: Use thread_to_monitoring_adapter.h instead
// This file is kept for backwards compatibility

#include "thread_to_monitoring_adapter.h"

namespace kcenon { namespace monitoring {

// Type alias for backwards compatibility
// Note: [[deprecated]] attribute cannot be applied to type aliases in C++17/20
// Users will see deprecation warnings when using this header via compiler diagnostics
using thread_system_adapter = thread_to_monitoring_adapter;

} } // namespace kcenon::monitoring

#endif // KCENON_MONITORING_ADAPTERS_THREAD_SYSTEM_ADAPTER_H
