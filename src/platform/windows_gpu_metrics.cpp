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

#include <kcenon/monitoring/collectors/gpu_collector.h>

#if defined(_WIN32)

// Windows stub implementation
// Future: Could implement via:
// - NVIDIA: NVML (nvidia-ml-py) or DirectX
// - AMD: ADL (AMD Display Library)
// - Intel: Intel GPU Tools
// - Generic: DirectX DXGI or WMI

namespace kcenon {
namespace monitoring {

// gpu_info_collector stub implementation for Windows

gpu_info_collector::gpu_info_collector() {}
gpu_info_collector::~gpu_info_collector() {}

bool gpu_info_collector::is_gpu_available() const {
    // Stub: GPU monitoring not implemented on Windows
    return false;
}

std::vector<gpu_device_info> gpu_info_collector::enumerate_gpus() {
    // Stub: Return empty vector
    return {};
}

std::vector<gpu_device_info> gpu_info_collector::enumerate_gpus_impl() {
    // Stub: Return empty vector
    return {};
}

gpu_reading gpu_info_collector::read_gpu_metrics(const gpu_device_info& device) {
    // Stub: Return empty reading with device info
    gpu_reading reading;
    reading.device = device;
    reading.timestamp = std::chrono::system_clock::now();
    return reading;
}

gpu_reading gpu_info_collector::read_gpu_metrics_impl(const gpu_device_info& device) {
    return read_gpu_metrics(device);
}

std::vector<gpu_reading> gpu_info_collector::read_all_gpu_metrics() {
    // Stub: Return empty vector
    return {};
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // _WIN32
