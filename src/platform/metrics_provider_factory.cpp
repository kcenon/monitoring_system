// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
