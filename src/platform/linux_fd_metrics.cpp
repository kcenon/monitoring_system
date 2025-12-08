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

#include <kcenon/monitoring/collectors/fd_collector.h>

#if defined(__linux__)

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace kcenon {
namespace monitoring {

namespace {

/**
 * Read system-wide FD usage from /proc/sys/fs/file-nr
 * Format: "allocated  free  maximum"
 */
struct system_fd_info {
    uint64_t allocated{0};
    uint64_t free{0};
    uint64_t maximum{0};
    bool valid{false};
};

system_fd_info read_system_fd_info() {
    system_fd_info info;
    std::ifstream file("/proc/sys/fs/file-nr");
    if (!file.is_open()) {
        return info;
    }

    file >> info.allocated >> info.free >> info.maximum;
    if (!file.fail()) {
        info.valid = true;
    }
    return info;
}

/**
 * Read process FD limits from /proc/self/limits
 */
struct process_limits {
    uint64_t soft_limit{0};
    uint64_t hard_limit{0};
    bool valid{false};
};

process_limits read_process_limits() {
    process_limits limits;
    std::ifstream file("/proc/self/limits");
    if (!file.is_open()) {
        return limits;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("Max open files") != std::string::npos) {
            // Line format: "Max open files            1024                 1048576 files"
            std::istringstream iss(line);
            std::string dummy;
            // Skip "Max open files"
            iss >> dummy >> dummy >> dummy;
            iss >> limits.soft_limit >> limits.hard_limit;
            if (!iss.fail()) {
                limits.valid = true;
            }
            break;
        }
    }
    return limits;
}

/**
 * Count FDs in /proc/self/fd/
 */
uint64_t count_process_fds() {
    std::filesystem::path fd_dir("/proc/self/fd");
    if (!std::filesystem::exists(fd_dir)) {
        return 0;
    }

    try {
        uint64_t count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(fd_dir)) {
            if (entry.is_symlink() || entry.is_regular_file() || entry.is_directory()) {
                ++count;
            }
        }
        // Subtract 1 for the directory iterator's own FD
        return count > 0 ? count - 1 : 0;
    } catch (const std::filesystem::filesystem_error&) {
        return 0;
    }
}

}  // anonymous namespace

fd_info_collector::fd_info_collector() = default;
fd_info_collector::~fd_info_collector() = default;

bool fd_info_collector::check_availability_impl() const {
    // Check if we can read at least one source of FD info
    std::filesystem::path proc_fd("/proc/self/fd");
    return std::filesystem::exists(proc_fd);
}

fd_metrics fd_info_collector::collect_metrics_impl() {
    fd_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();

    // System-wide FD info (Linux-specific)
    auto sys_info = read_system_fd_info();
    if (sys_info.valid) {
        metrics.fd_used_system = sys_info.allocated - sys_info.free;
        metrics.fd_max_system = sys_info.maximum;
        metrics.system_metrics_available = true;
    }

    // Process FD limits
    auto limits = read_process_limits();
    if (limits.valid) {
        metrics.fd_soft_limit = limits.soft_limit;
        metrics.fd_hard_limit = limits.hard_limit;
    }

    // Process FD count
    metrics.fd_used_process = count_process_fds();

    // Calculate usage percentage based on soft limit
    if (metrics.fd_soft_limit > 0) {
        metrics.fd_usage_percent = 100.0 * static_cast<double>(metrics.fd_used_process) /
                                   static_cast<double>(metrics.fd_soft_limit);
    }

    return metrics;
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(__linux__)
