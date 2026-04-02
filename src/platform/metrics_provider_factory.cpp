// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <kcenon/monitoring/platform/metrics_provider.h>

// Platform-specific includes
#if defined(__linux__)
#include "linux/linux_metrics_provider.h"
#elif defined(__APPLE__)
#include "macos/macos_metrics_provider.h"
#elif defined(_WIN32)
#include "windows/windows_metrics_provider.h"
#else
#include "null/null_metrics_provider.h"
#endif

namespace kcenon {
namespace monitoring {
namespace platform {

std::unique_ptr<metrics_provider> metrics_provider::create() {
#if defined(__linux__)
    return std::make_unique<linux_metrics_provider>();
#elif defined(__APPLE__)
    return std::make_unique<macos_metrics_provider>();
#elif defined(_WIN32)
    return std::make_unique<windows_metrics_provider>();
#else
    // Unsupported platform - return null object for safe fallback
    return std::make_unique<null_metrics_provider>();
#endif
}

}  // namespace platform
}  // namespace monitoring
}  // namespace kcenon
