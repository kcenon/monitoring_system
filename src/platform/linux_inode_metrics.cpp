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

#include <kcenon/monitoring/collectors/inode_collector.h>

#if defined(__linux__)

#include <sys/statvfs.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace kcenon {
namespace monitoring {

namespace {

/**
 * Pseudo-filesystems to skip (they don't have meaningful inode metrics)
 */
const std::unordered_set<std::string> PSEUDO_FILESYSTEMS = {
    "proc",     "sysfs",    "devtmpfs", "devpts",  "tmpfs",
    "securityfs", "cgroup",  "cgroup2",  "pstore",  "debugfs",
    "hugetlbfs", "mqueue",   "fusectl",  "configfs", "binfmt_misc",
    "autofs",   "rpc_pipefs", "nfsd",    "tracefs", "overlay"
};

/**
 * Check if a filesystem type should be skipped
 */
bool should_skip_filesystem(const std::string& fs_type) {
    return PSEUDO_FILESYSTEMS.find(fs_type) != PSEUDO_FILESYSTEMS.end();
}

/**
 * Parse /proc/mounts to get list of mounted filesystems
 */
struct mount_entry {
    std::string device;
    std::string mount_point;
    std::string fs_type;
};

std::vector<mount_entry> get_mount_entries() {
    std::vector<mount_entry> entries;
    std::ifstream mounts_file("/proc/mounts");
    
    if (!mounts_file.is_open()) {
        return entries;
    }

    std::string line;
    while (std::getline(mounts_file, line)) {
        std::istringstream iss(line);
        mount_entry entry;
        std::string options, dump, pass;
        
        if (iss >> entry.device >> entry.mount_point >> entry.fs_type >> options >> dump >> pass) {
            entries.push_back(std::move(entry));
        }
    }

    return entries;
}

/**
 * Get inode info for a single filesystem using statvfs
 */
filesystem_inode_info get_filesystem_inode_info(const mount_entry& mount) {
    filesystem_inode_info info;
    info.mount_point = mount.mount_point;
    info.filesystem_type = mount.fs_type;
    info.device = mount.device;

    struct statvfs stat;
    if (statvfs(mount.mount_point.c_str(), &stat) == 0) {
        info.inodes_total = stat.f_files;
        info.inodes_free = stat.f_ffree;
        
        // Some filesystems (like XFS) may have dynamic inode allocation
        // where f_files is 0 or represents something different
        if (info.inodes_total > 0) {
            info.inodes_used = info.inodes_total - info.inodes_free;
            info.inodes_usage_percent = 100.0 * static_cast<double>(info.inodes_used) /
                                        static_cast<double>(info.inodes_total);
        }
    }

    return info;
}

}  // anonymous namespace

inode_info_collector::inode_info_collector() = default;
inode_info_collector::~inode_info_collector() = default;

bool inode_info_collector::check_availability_impl() const {
    // Check if we can read /proc/mounts and call statvfs
    std::ifstream mounts_file("/proc/mounts");
    if (!mounts_file.is_open()) {
        return false;
    }

    // Try statvfs on root filesystem
    struct statvfs stat;
    return statvfs("/", &stat) == 0;
}

inode_metrics inode_info_collector::collect_metrics_impl() {
    inode_metrics metrics;
    metrics.timestamp = std::chrono::system_clock::now();
    metrics.metrics_available = true;

    auto mounts = get_mount_entries();
    
    for (const auto& mount : mounts) {
        // Skip pseudo-filesystems
        if (should_skip_filesystem(mount.fs_type)) {
            continue;
        }

        auto fs_info = get_filesystem_inode_info(mount);
        
        // Skip if no inode info available (e.g., statvfs failed)
        if (fs_info.inodes_total == 0) {
            continue;
        }

        // Update aggregates
        metrics.total_inodes += fs_info.inodes_total;
        metrics.total_inodes_used += fs_info.inodes_used;
        metrics.total_inodes_free += fs_info.inodes_free;

        // Track max usage
        if (fs_info.inodes_usage_percent > metrics.max_usage_percent) {
            metrics.max_usage_percent = fs_info.inodes_usage_percent;
            metrics.max_usage_mount_point = fs_info.mount_point;
        }

        metrics.filesystems.push_back(std::move(fs_info));
    }

    // Calculate average usage
    if (!metrics.filesystems.empty()) {
        double sum = 0.0;
        for (const auto& fs : metrics.filesystems) {
            sum += fs.inodes_usage_percent;
        }
        metrics.average_usage_percent = sum / static_cast<double>(metrics.filesystems.size());
    }

    return metrics;
}

bool inode_info_collector::is_inode_monitoring_available() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!availability_checked_) {
        available_ = check_availability_impl();
        availability_checked_ = true;
    }
    return available_;
}

inode_metrics inode_info_collector::collect_metrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    return collect_metrics_impl();
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(__linux__)
