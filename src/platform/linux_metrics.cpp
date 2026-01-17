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

#if defined(__linux__)

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <numeric>

namespace kcenon {
namespace monitoring {

namespace {

// Read first line from /proc/stat for CPU times
struct cpu_times {
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;
};

std::optional<cpu_times> read_cpu_times() {
    std::ifstream stat_file("/proc/stat");
    if (!stat_file.is_open()) {
        return std::nullopt;
    }

    std::string line;
    if (!std::getline(stat_file, line)) {
        return std::nullopt;
    }

    // Line format: "cpu  user nice system idle iowait irq softirq steal guest guest_nice"
    std::istringstream iss(line);
    std::string cpu_label;
    cpu_times times{};

    iss >> cpu_label >> times.user >> times.nice >> times.system >> times.idle
        >> times.iowait >> times.irq >> times.softirq >> times.steal;

    if (cpu_label != "cpu") {
        return std::nullopt;
    }

    return times;
}

double calculate_cpu_usage() {
    // Read CPU times twice with a small delay to calculate usage
    auto times1 = read_cpu_times();
    if (!times1) {
        return 0.0;
    }

    // Small delay to measure CPU activity
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto times2 = read_cpu_times();
    if (!times2) {
        return 0.0;
    }

    // Calculate deltas
    uint64_t total1 = times1->user + times1->nice + times1->system + times1->idle +
                      times1->iowait + times1->irq + times1->softirq + times1->steal;
    uint64_t total2 = times2->user + times2->nice + times2->system + times2->idle +
                      times2->iowait + times2->irq + times2->softirq + times2->steal;

    uint64_t idle1 = times1->idle + times1->iowait;
    uint64_t idle2 = times2->idle + times2->iowait;

    uint64_t total_delta = total2 - total1;
    uint64_t idle_delta = idle2 - idle1;

    if (total_delta == 0) {
        return 0.0;
    }

    double usage = 100.0 * (1.0 - static_cast<double>(idle_delta) / total_delta);
    return std::max(0.0, std::min(100.0, usage));
}

struct memory_info {
    uint64_t total_kb;
    uint64_t free_kb;
    uint64_t available_kb;
    uint64_t buffers_kb;
    uint64_t cached_kb;
};

std::optional<memory_info> read_memory_info() {
    std::ifstream meminfo_file("/proc/meminfo");
    if (!meminfo_file.is_open()) {
        return std::nullopt;
    }

    memory_info info{};
    std::string line;

    while (std::getline(meminfo_file, line)) {
        std::istringstream iss(line);
        std::string key;
        uint64_t value;
        std::string unit;

        if (iss >> key >> value >> unit) {
            if (key == "MemTotal:") {
                info.total_kb = value;
            } else if (key == "MemFree:") {
                info.free_kb = value;
            } else if (key == "MemAvailable:") {
                info.available_kb = value;
            } else if (key == "Buffers:") {
                info.buffers_kb = value;
            } else if (key == "Cached:") {
                info.cached_kb = value;
            }
        }
    }

    return info;
}

uint64_t count_threads() {
    // Count thread directories in /proc/self/task/
    std::filesystem::path task_dir("/proc/self/task");

    if (!std::filesystem::exists(task_dir)) {
        return 1; // At least the main thread exists
    }

    try {
        uint64_t count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(task_dir)) {
            if (entry.is_directory()) {
                ++count;
            }
        }
        return count > 0 ? count : 1;
    } catch (const std::filesystem::filesystem_error&) {
        return 1;
    }
}

} // anonymous namespace

common::Result<system_metrics> get_linux_system_metrics() {
    system_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();

    // CPU usage
    metrics.cpu_usage_percent = calculate_cpu_usage();

    // Memory usage
    auto mem_info = read_memory_info();
    if (mem_info) {
        // Convert KB to bytes
        uint64_t total_bytes = mem_info->total_kb * 1024;
        uint64_t available_bytes = mem_info->available_kb * 1024;

        metrics.memory_usage_bytes = total_bytes - available_bytes;
        metrics.available_memory_bytes = available_bytes;

        if (total_bytes > 0) {
            metrics.memory_usage_percent =
                100.0 * (static_cast<double>(metrics.memory_usage_bytes) / total_bytes);
        }
    }

    // Thread count
    metrics.thread_count = count_threads();

    return common::ok(metrics);
}

} // namespace monitoring
} // namespace kcenon

#endif // defined(__linux__)
