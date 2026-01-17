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

#include <kcenon/monitoring/core/performance_monitor.h>

#if defined(_WIN32)

// Define NOMINMAX and WIN32_LEAN_AND_MEAN before including windows.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <vector>
#include <string>
#include <algorithm>

// Link with Pdh.lib for PDH functions
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")

namespace kcenon {
namespace monitoring {

namespace {

// Helper to convert wide string to string
std::string wide_to_string(const std::wstring& wide_str) {
    if (wide_str.empty()) {
        return std::string();
    }

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(),
                                         static_cast<int>(wide_str.length()),
                                         nullptr, 0, nullptr, nullptr);
    std::string str_to(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(),
                       static_cast<int>(wide_str.length()),
                       &str_to[0], size_needed, nullptr, nullptr);
    return str_to;
}

double get_cpu_usage() {
    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER counter = nullptr;
    PDH_FMT_COUNTERVALUE counter_value;
    PDH_STATUS status;

    // Create query
    status = PdhOpenQueryW(nullptr, 0, &query);
    if (status != ERROR_SUCCESS) {
        return 0.0;
    }

    // Add CPU usage counter
    status = PdhAddEnglishCounterW(query,
                                   L"\\Processor(_Total)\\% Processor Time",
                                   0, &counter);
    if (status != ERROR_SUCCESS) {
        PdhCloseQuery(query);
        return 0.0;
    }

    // Collect first sample
    PdhCollectQueryData(query);

    // Wait a bit to get meaningful data
    Sleep(100);

    // Collect second sample
    status = PdhCollectQueryData(query);
    if (status != ERROR_SUCCESS) {
        PdhCloseQuery(query);
        return 0.0;
    }

    // Get formatted value
    status = PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &counter_value);

    PdhCloseQuery(query);

    if (status != ERROR_SUCCESS || counter_value.CStatus != ERROR_SUCCESS) {
        return 0.0;
    }

    // Clamp to [0, 100] range
    return std::max(0.0, std::min(100.0, counter_value.doubleValue));
}

struct memory_info {
    uint64_t total_bytes;
    uint64_t available_bytes;
    uint64_t used_bytes;
};

std::optional<memory_info> get_memory_info() {
    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);

    if (!GlobalMemoryStatusEx(&mem_status)) {
        return std::nullopt;
    }

    memory_info info;
    info.total_bytes = mem_status.ullTotalPhys;
    info.available_bytes = mem_status.ullAvailPhys;
    info.used_bytes = info.total_bytes - info.available_bytes;

    return info;
}

uint64_t get_thread_count() {
    HANDLE process = GetCurrentProcess();

    // Get process info
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (!GetProcessMemoryInfo(process, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        return 1; // At least the main thread exists
    }

    // Alternative approach: Query number of threads via process information
    DWORD thread_count = 0;

    // Get snapshot of all threads
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 1;
    }

    DWORD current_process_id = GetCurrentProcessId();
    THREADENTRY32 thread_entry;
    thread_entry.dwSize = sizeof(THREADENTRY32);

    if (Thread32First(snapshot, &thread_entry)) {
        do {
            if (thread_entry.th32OwnerProcessID == current_process_id) {
                ++thread_count;
            }
        } while (Thread32Next(snapshot, &thread_entry));
    }

    CloseHandle(snapshot);
    return thread_count > 0 ? thread_count : 1;
}

} // anonymous namespace

common::Result<system_metrics> get_windows_system_metrics() {
    system_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();

    // CPU usage
    metrics.cpu_usage_percent = get_cpu_usage();

    // Memory usage
    auto mem_info = get_memory_info();
    if (mem_info) {
        metrics.memory_usage_bytes = mem_info->used_bytes;
        metrics.available_memory_bytes = mem_info->available_bytes;

        if (mem_info->total_bytes > 0) {
            metrics.memory_usage_percent =
                100.0 * (static_cast<double>(mem_info->used_bytes) / mem_info->total_bytes);
        }
    }

    // Thread count
    metrics.thread_count = static_cast<uint32_t>(get_thread_count());

    return common::ok(metrics);
}

} // namespace monitoring
} // namespace kcenon

#endif // defined(_WIN32)
