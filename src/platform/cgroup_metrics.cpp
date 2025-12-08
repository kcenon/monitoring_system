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

#include <kcenon/monitoring/collectors/container_collector.h>

#if defined(__linux__)

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace kcenon {
namespace monitoring {

namespace {

// Constants for cgroup paths
constexpr const char* CGROUP_V2_ROOT = "/sys/fs/cgroup";
constexpr const char* CGROUP_V1_CPU = "/sys/fs/cgroup/cpu";
constexpr const char* CGROUP_V1_MEMORY = "/sys/fs/cgroup/memory";
constexpr const char* CGROUP_V1_BLKIO = "/sys/fs/cgroup/blkio";
constexpr const char* CGROUP_V1_PIDS = "/sys/fs/cgroup/pids";

// Container ID pattern (Docker uses 64-char hex IDs)
const std::regex CONTAINER_ID_REGEX("[a-f0-9]{12,64}");

// Read first line from a file
std::string read_first_line(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::string line;
    std::getline(file, line);
    return line;
}

// Read entire file content
std::string read_file_content(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Parse uint64 from string with "max" handling
uint64_t parse_uint64_or_max(const std::string& str, uint64_t default_val = 0) {
    if (str.empty() || str == "max") {
        return default_val;
    }
    try {
        return std::stoull(str);
    } catch (...) {
        return default_val;
    }
}

// Check if path looks like a container cgroup path
bool is_container_cgroup(const std::string& path) {
    // Check for Docker-style paths
    if (path.find("docker") != std::string::npos || path.find("containerd") != std::string::npos ||
        path.find("cri-o") != std::string::npos || path.find("podman") != std::string::npos) {
        return true;
    }

    // Check for container ID pattern in the path
    std::smatch match;
    if (std::regex_search(path, match, CONTAINER_ID_REGEX)) {
        return true;
    }

    return false;
}

// Extract container ID from cgroup path
std::string extract_container_id(const std::string& path) {
    std::smatch match;
    if (std::regex_search(path, match, CONTAINER_ID_REGEX)) {
        std::string full_id = match[0].str();
        // Return short ID (first 12 chars)
        return full_id.substr(0, std::min(size_t(12), full_id.length()));
    }
    return "";
}

}  // anonymous namespace

container_info_collector::container_info_collector() = default;
container_info_collector::~container_info_collector() = default;

cgroup_version container_info_collector::detect_cgroup_version() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (version_detected_) {
        return cached_version_;
    }

    auto* self = const_cast<container_info_collector*>(this);
    self->cached_version_ = detect_cgroup_version_linux();
    self->version_detected_ = true;

    return cached_version_;
}

cgroup_version container_info_collector::detect_cgroup_version_linux() const {
    namespace fs = std::filesystem;

    // Check cgroup v2 unified hierarchy
    // In cgroups v2, /sys/fs/cgroup/cgroup.controllers exists
    if (fs::exists("/sys/fs/cgroup/cgroup.controllers")) {
        return cgroup_version::v2;
    }

    // Check cgroups v1 hierarchy
    if (fs::exists(CGROUP_V1_CPU) || fs::exists(CGROUP_V1_MEMORY)) {
        return cgroup_version::v1;
    }

    return cgroup_version::none;
}

bool container_info_collector::is_containerized() const {
    namespace fs = std::filesystem;

    // Check for /.dockerenv (Docker)
    if (fs::exists("/.dockerenv")) {
        return true;
    }

    // Check for /run/.containerenv (Podman)
    if (fs::exists("/run/.containerenv")) {
        return true;
    }

    // Check cgroup for container signatures
    std::string cgroup_content = read_file_content("/proc/1/cgroup");
    return is_container_cgroup(cgroup_content);
}

std::vector<container_info> container_info_collector::enumerate_containers() {
    cgroup_version version = detect_cgroup_version();

    switch (version) {
        case cgroup_version::v2:
            return enumerate_containers_cgroup_v2();
        case cgroup_version::v1:
            return enumerate_containers_cgroup_v1();
        default:
            return {};
    }
}

std::vector<container_info> container_info_collector::enumerate_containers_cgroup_v2() {
    namespace fs = std::filesystem;
    std::vector<container_info> containers;

    try {
        // In cgroups v2, look for container directories under /sys/fs/cgroup/
        // Common patterns: /sys/fs/cgroup/system.slice/docker-<id>.scope
        //                  /sys/fs/cgroup/docker/<id>

        std::vector<std::string> search_paths = {"/sys/fs/cgroup/docker",
                                                 "/sys/fs/cgroup/system.slice",
                                                 "/sys/fs/cgroup/kubepods.slice"};

        for (const auto& search_path : search_paths) {
            if (!fs::exists(search_path)) {
                continue;
            }

            for (const auto& entry : fs::directory_iterator(search_path)) {
                if (!entry.is_directory()) {
                    continue;
                }

                std::string dir_name = entry.path().filename().string();
                if (is_container_cgroup(dir_name)) {
                    container_info info;
                    info.container_id = extract_container_id(dir_name);
                    info.cgroup_path = entry.path().string();
                    info.is_running = fs::exists(entry.path() / "cgroup.procs");

                    if (!info.container_id.empty()) {
                        containers.push_back(std::move(info));
                    }
                }
            }
        }

        // If no external containers found, check if we're running inside a container
        if (containers.empty() && is_containerized()) {
            // Collect metrics for the current container
            container_info self_info;
            self_info.container_id = "self";
            self_info.container_name = "current";
            self_info.cgroup_path = CGROUP_V2_ROOT;
            self_info.is_running = true;
            containers.push_back(std::move(self_info));
        }

    } catch (const std::exception&) {
        // Graceful degradation on errors
    }

    return containers;
}

std::vector<container_info> container_info_collector::enumerate_containers_cgroup_v1() {
    namespace fs = std::filesystem;
    std::vector<container_info> containers;

    try {
        // In cgroups v1, look for container directories under /sys/fs/cgroup/cpu/docker/
        std::vector<std::string> search_paths = {"/sys/fs/cgroup/cpu/docker",
                                                 "/sys/fs/cgroup/memory/docker",
                                                 "/sys/fs/cgroup/cpu/kubepods"};

        for (const auto& search_path : search_paths) {
            if (!fs::exists(search_path)) {
                continue;
            }

            for (const auto& entry : fs::directory_iterator(search_path)) {
                if (!entry.is_directory()) {
                    continue;
                }

                std::string dir_name = entry.path().filename().string();
                std::string container_id = extract_container_id(dir_name);

                if (!container_id.empty()) {
                    // Check if we already have this container
                    auto it = std::find_if(containers.begin(), containers.end(),
                                           [&container_id](const container_info& c) {
                                               return c.container_id == container_id;
                                           });

                    if (it == containers.end()) {
                        container_info info;
                        info.container_id = container_id;
                        info.cgroup_path = entry.path().string();
                        info.is_running = fs::exists(entry.path() / "cgroup.procs") ||
                                          fs::exists(entry.path() / "tasks");
                        containers.push_back(std::move(info));
                    }
                }
            }
        }

        // If no external containers found, check if we're running inside a container
        if (containers.empty() && is_containerized()) {
            container_info self_info;
            self_info.container_id = "self";
            self_info.container_name = "current";
            self_info.cgroup_path = CGROUP_V1_CPU;
            self_info.is_running = true;
            containers.push_back(std::move(self_info));
        }

    } catch (const std::exception&) {
        // Graceful degradation on errors
    }

    return containers;
}

container_metrics container_info_collector::collect_container_metrics(const container_info& info) {
    cgroup_version version = detect_cgroup_version();

    switch (version) {
        case cgroup_version::v2:
            return collect_metrics_cgroup_v2(info);
        case cgroup_version::v1:
            return collect_metrics_cgroup_v1(info);
        default:
            return container_metrics{};
    }
}

uint64_t container_info_collector::read_cgroup_value(const std::string& path,
                                                     const std::string& key) const {
    if (key.empty()) {
        // Read single value from file
        std::string content = read_first_line(path);
        return parse_uint64_or_max(content, 0);
    }

    // Read key-value from file
    std::ifstream file(path);
    if (!file.is_open()) {
        return 0;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string k;
        uint64_t v;
        if (iss >> k >> v && k == key) {
            return v;
        }
    }

    return 0;
}

std::unordered_map<std::string, uint64_t> container_info_collector::read_cgroup_stat(
    const std::string& path) const {
    std::unordered_map<std::string, uint64_t> stats;
    std::ifstream file(path);
    if (!file.is_open()) {
        return stats;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        uint64_t value;
        if (iss >> key >> value) {
            stats[key] = value;
        }
    }

    return stats;
}

container_metrics container_info_collector::collect_metrics_cgroup_v2(const container_info& info) {
    container_metrics metrics;
    metrics.container_id = info.container_id;
    metrics.container_name = info.container_name;
    metrics.image_name = info.image_name;
    metrics.timestamp = std::chrono::system_clock::now();

    namespace fs = std::filesystem;
    const std::string& cg = info.cgroup_path;

    try {
        // CPU metrics - cpu.stat
        auto cpu_stat = read_cgroup_stat(cg + "/cpu.stat");
        uint64_t usage_usec = cpu_stat["usage_usec"];
        metrics.cpu_usage_ns = usage_usec * 1000;  // Convert to nanoseconds

        // Calculate CPU percentage based on previous reading
        auto now = std::chrono::steady_clock::now();
        auto it = prev_cpu_stats_.find(info.container_id);
        if (it != prev_cpu_stats_.end()) {
            auto duration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(now - it->second.timestamp)
                    .count();
            if (duration > 0) {
                uint64_t cpu_delta = metrics.cpu_usage_ns - it->second.usage_ns;
                metrics.cpu_usage_percent = 100.0 * static_cast<double>(cpu_delta) / duration;
            }
        }
        prev_cpu_stats_[info.container_id] = {metrics.cpu_usage_ns, now};

        // Memory metrics
        metrics.memory_usage_bytes = read_cgroup_value(cg + "/memory.current");
        std::string max_str = read_first_line(cg + "/memory.max");
        metrics.memory_limit_bytes = parse_uint64_or_max(max_str, UINT64_MAX);
        if (metrics.memory_limit_bytes > 0 && metrics.memory_limit_bytes != UINT64_MAX) {
            metrics.memory_usage_percent = 100.0 * static_cast<double>(metrics.memory_usage_bytes) /
                                           metrics.memory_limit_bytes;
        }

        // I/O metrics - io.stat
        if (fs::exists(cg + "/io.stat")) {
            auto io_stat = read_cgroup_stat(cg + "/io.stat");
            metrics.blkio_read_bytes = io_stat["rbytes"];
            metrics.blkio_write_bytes = io_stat["wbytes"];
        }

        // PIDs
        if (fs::exists(cg + "/pids.current")) {
            metrics.pids_current = read_cgroup_value(cg + "/pids.current");
        }
        if (fs::exists(cg + "/pids.max")) {
            std::string pids_max = read_first_line(cg + "/pids.max");
            metrics.pids_limit = parse_uint64_or_max(pids_max, 0);
        }

    } catch (const std::exception&) {
        // Graceful degradation
    }

    return metrics;
}

container_metrics container_info_collector::collect_metrics_cgroup_v1(const container_info& info) {
    container_metrics metrics;
    metrics.container_id = info.container_id;
    metrics.container_name = info.container_name;
    metrics.image_name = info.image_name;
    metrics.timestamp = std::chrono::system_clock::now();

    namespace fs = std::filesystem;

    try {
        // For cgroups v1, we need to look in different subsystem paths
        // The info.cgroup_path typically points to the cpu cgroup
        std::string base_path = info.cgroup_path;

        // Extract the container path relative to the subsystem root
        std::string relative_path;
        size_t docker_pos = base_path.find("/docker/");
        if (docker_pos != std::string::npos) {
            relative_path = base_path.substr(docker_pos);
        }

        // CPU metrics - cpuacct.usage (nanoseconds)
        std::string cpu_path = std::string(CGROUP_V1_CPU) + relative_path;
        if (fs::exists(cpu_path + "/cpuacct.usage")) {
            metrics.cpu_usage_ns = read_cgroup_value(cpu_path + "/cpuacct.usage");
        } else if (fs::exists(base_path + "/cpuacct.usage")) {
            metrics.cpu_usage_ns = read_cgroup_value(base_path + "/cpuacct.usage");
        }

        // Calculate CPU percentage
        auto now = std::chrono::steady_clock::now();
        auto it = prev_cpu_stats_.find(info.container_id);
        if (it != prev_cpu_stats_.end()) {
            auto duration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(now - it->second.timestamp)
                    .count();
            if (duration > 0) {
                uint64_t cpu_delta = metrics.cpu_usage_ns - it->second.usage_ns;
                metrics.cpu_usage_percent = 100.0 * static_cast<double>(cpu_delta) / duration;
            }
        }
        prev_cpu_stats_[info.container_id] = {metrics.cpu_usage_ns, now};

        // Memory metrics
        std::string mem_path = std::string(CGROUP_V1_MEMORY) + relative_path;
        if (fs::exists(mem_path + "/memory.usage_in_bytes")) {
            metrics.memory_usage_bytes = read_cgroup_value(mem_path + "/memory.usage_in_bytes");
            metrics.memory_limit_bytes = read_cgroup_value(mem_path + "/memory.limit_in_bytes");
        } else if (fs::exists(base_path + "/memory.usage_in_bytes")) {
            metrics.memory_usage_bytes = read_cgroup_value(base_path + "/memory.usage_in_bytes");
            metrics.memory_limit_bytes = read_cgroup_value(base_path + "/memory.limit_in_bytes");
        }

        if (metrics.memory_limit_bytes > 0 &&
            metrics.memory_limit_bytes < static_cast<uint64_t>(1) << 62) {  // Check for "unlimited"
            metrics.memory_usage_percent = 100.0 * static_cast<double>(metrics.memory_usage_bytes) /
                                           metrics.memory_limit_bytes;
        }

        // Block I/O metrics
        std::string blkio_path = std::string(CGROUP_V1_BLKIO) + relative_path;
        if (fs::exists(blkio_path + "/blkio.io_service_bytes_recursive")) {
            std::ifstream file(blkio_path + "/blkio.io_service_bytes_recursive");
            std::string line;
            while (std::getline(file, line)) {
                std::istringstream iss(line);
                std::string device, op;
                uint64_t bytes;
                if (iss >> device >> op >> bytes) {
                    if (op == "Read") {
                        metrics.blkio_read_bytes += bytes;
                    } else if (op == "Write") {
                        metrics.blkio_write_bytes += bytes;
                    }
                }
            }
        }

        // PIDs
        std::string pids_path = std::string(CGROUP_V1_PIDS) + relative_path;
        if (fs::exists(pids_path + "/pids.current")) {
            metrics.pids_current = read_cgroup_value(pids_path + "/pids.current");
            metrics.pids_limit = read_cgroup_value(pids_path + "/pids.max");
        }

    } catch (const std::exception&) {
        // Graceful degradation
    }

    return metrics;
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(__linux__)
