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

#if defined(__APPLE__)

#include <dirent.h>
#include <sys/resource.h>
#include <unistd.h>

namespace kcenon {
namespace monitoring {

namespace {

/**
 * Get process FD limits using getrlimit
 */
struct process_limits {
    uint64_t soft_limit{0};
    uint64_t hard_limit{0};
    bool valid{false};
};

process_limits get_process_limits() {
    process_limits limits;
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        limits.soft_limit = static_cast<uint64_t>(rl.rlim_cur);
        limits.hard_limit = static_cast<uint64_t>(rl.rlim_max);
        limits.valid = true;
    }
    return limits;
}

/**
 * Count FDs by enumerating /dev/fd directory
 */
uint64_t count_process_fds() {
    DIR* dir = opendir("/dev/fd");
    if (dir == nullptr) {
        return 0;
    }

    uint64_t count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        // Skip "." and ".."
        if (entry->d_name[0] != '.') {
            ++count;
        }
    }
    closedir(dir);

    // Subtract 1 for the directory iterator's own FD
    return count > 0 ? count - 1 : 0;
}

}  // anonymous namespace

fd_info_collector::fd_info_collector() = default;
fd_info_collector::~fd_info_collector() = default;

bool fd_info_collector::check_availability_impl() const {
    // Check if we can access /dev/fd
    DIR* dir = opendir("/dev/fd");
    if (dir != nullptr) {
        closedir(dir);
        return true;
    }
    return false;
}

fd_metrics fd_info_collector::collect_metrics_impl() {
    fd_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();

    // System-wide FD metrics not available on macOS
    metrics.system_metrics_available = false;
    metrics.fd_used_system = 0;
    metrics.fd_max_system = 0;

    // Process FD limits
    auto limits = get_process_limits();
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

#endif  // defined(__APPLE__)
