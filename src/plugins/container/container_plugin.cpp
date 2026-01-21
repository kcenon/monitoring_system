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

#include <kcenon/monitoring/plugins/container/container_plugin.h>

#include <kcenon/monitoring/collectors/container_collector.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#ifdef _MSC_VER
#include <cstring>
#endif

namespace kcenon {
namespace monitoring {
namespace plugins {

namespace {

// Cross-platform safe environment variable access
// Returns empty string if the variable is not set
std::string safe_getenv(const char* name) {
#ifdef _MSC_VER
    char* buffer = nullptr;
    size_t size = 0;
    if (_dupenv_s(&buffer, &size, name) == 0 && buffer != nullptr) {
        std::string result(buffer);
        free(buffer);
        return result;
    }
    return "";
#else
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : "";
#endif
}

}  // anonymous namespace

std::unique_ptr<container_plugin> container_plugin::create(const container_plugin_config& config) {
    return std::unique_ptr<container_plugin>(new container_plugin(config));
}

container_plugin::container_plugin(const container_plugin_config& config) : config_(config) {
    initialize_collectors();
}

container_plugin::~container_plugin() = default;

void container_plugin::initialize_collectors() {
    // Initialize container collector
    if (config_.enable_docker || config_.enable_cgroup) {
        container_collector_ = std::make_unique<container_collector>();

        // Configure the collector
        std::unordered_map<std::string, std::string> collector_config;
        collector_config["enabled"] = "true";
        collector_config["collect_network"] = config_.collect_network_metrics ? "true" : "false";
        collector_config["collect_blkio"] = config_.collect_blkio_metrics ? "true" : "false";

        container_collector_->initialize(collector_config);
    }

    initialized_ = true;
}

bool container_plugin::initialize(const std::unordered_map<std::string, std::string>& config) {
    // Parse configuration from map
    auto it = config.find("enable_docker");
    if (it != config.end()) {
        config_.enable_docker = (it->second == "true" || it->second == "1");
    }

    it = config.find("enable_kubernetes");
    if (it != config.end()) {
        config_.enable_kubernetes = (it->second == "true" || it->second == "1");
    }

    it = config.find("enable_cgroup");
    if (it != config.end()) {
        config_.enable_cgroup = (it->second == "true" || it->second == "1");
    }

    it = config.find("docker_socket");
    if (it != config.end()) {
        config_.docker_socket = it->second;
    }

    it = config.find("kubeconfig_path");
    if (it != config.end()) {
        config_.kubeconfig_path = it->second;
    }

    it = config.find("namespace_filter");
    if (it != config.end()) {
        config_.namespace_filter = it->second;
    }

    it = config.find("collect_network");
    if (it != config.end()) {
        config_.collect_network_metrics = (it->second == "true" || it->second == "1");
    }

    it = config.find("collect_blkio");
    if (it != config.end()) {
        config_.collect_blkio_metrics = (it->second == "true" || it->second == "1");
    }

    // Re-initialize collectors with new configuration
    initialize_collectors();

    return true;
}

std::vector<metric> container_plugin::collect() {
    std::vector<metric> metrics;

    if (!initialized_) {
        return metrics;
    }

    try {
        // Collect from container collector
        if (container_collector_) {
            auto container_metrics = container_collector_->collect();
            metrics.insert(metrics.end(), container_metrics.begin(), container_metrics.end());

            // Update statistics
            auto last_metrics = container_collector_->get_last_metrics();
            containers_found_.store(last_metrics.size());
        }

        total_collections_++;

    } catch (const std::exception&) {
        collection_errors_++;
    }

    return metrics;
}

std::string container_plugin::get_name() const { return "container_plugin"; }

std::vector<std::string> container_plugin::get_metric_types() const {
    std::vector<std::string> types;

    if (container_collector_) {
        auto collector_types = container_collector_->get_metric_types();
        types.insert(types.end(), collector_types.begin(), collector_types.end());
    }

    return types;
}

bool container_plugin::is_healthy() const {
    if (!initialized_) {
        return false;
    }

    // At least one collector should be available
    return (container_collector_ && container_collector_->is_healthy());
}

std::unordered_map<std::string, double> container_plugin::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    std::unordered_map<std::string, double> stats;
    stats["total_collections"] = static_cast<double>(total_collections_.load());
    stats["collection_errors"] = static_cast<double>(collection_errors_.load());
    stats["containers_found"] = static_cast<double>(containers_found_.load());

    // Add statistics from individual collectors
    if (container_collector_) {
        auto collector_stats = container_collector_->get_statistics();
        for (const auto& [key, value] : collector_stats) {
            stats["container_collector." + key] = value;
        }
    }

    return stats;
}

bool container_plugin::is_running_in_container() {
#if defined(__linux__)
    // Check for /.dockerenv file (Docker-specific)
    if (std::filesystem::exists("/.dockerenv")) {
        return true;
    }

    // Check for /run/.containerenv file (Podman-specific)
    if (std::filesystem::exists("/run/.containerenv")) {
        return true;
    }

    // Check cgroup for container indicators
    std::ifstream cgroup("/proc/1/cgroup");
    if (cgroup.is_open()) {
        std::string line;
        while (std::getline(cgroup, line)) {
            if (line.find("docker") != std::string::npos ||
                line.find("kubepods") != std::string::npos ||
                line.find("containerd") != std::string::npos ||
                line.find("cri-o") != std::string::npos) {
                return true;
            }
        }
    }

    // Check for container-specific environment variables
    auto container_env = safe_getenv("container");
    if (!container_env.empty()) {
        return true;
    }

    return false;
#else
    // Non-Linux platforms: container detection not supported
    return false;
#endif
}

bool container_plugin::is_kubernetes_environment() {
    // Check for Kubernetes service account
    if (std::filesystem::exists("/var/run/secrets/kubernetes.io/serviceaccount")) {
        return true;
    }

    // Check for Kubernetes environment variables
    auto k8s_host = safe_getenv("KUBERNETES_SERVICE_HOST");
    auto k8s_port = safe_getenv("KUBERNETES_SERVICE_PORT");

    return (!k8s_host.empty() && !k8s_port.empty());
}

container_runtime container_plugin::detect_runtime() {
#if defined(__linux__)
    // Check for Docker socket
    if (std::filesystem::exists("/var/run/docker.sock")) {
        return container_runtime::docker;
    }

    // Check for containerd socket
    if (std::filesystem::exists("/run/containerd/containerd.sock")) {
        return container_runtime::containerd;
    }

    // Check for Podman socket
    if (std::filesystem::exists("/run/podman/podman.sock")) {
        return container_runtime::podman;
    }

    // Check for CRI-O socket
    if (std::filesystem::exists("/var/run/crio/crio.sock")) {
        return container_runtime::cri_o;
    }
#endif

    return container_runtime::auto_detect;
}

bool container_plugin::is_docker_available() const {
    return std::filesystem::exists(config_.docker_socket);
}

bool container_plugin::is_kubernetes_available() const {
    if (!config_.enable_kubernetes) {
        return false;
    }

    // Check for in-cluster config
    if (config_.kubeconfig_path.empty()) {
        return is_kubernetes_environment();
    }

    // Check for kubeconfig file
    return std::filesystem::exists(config_.kubeconfig_path);
}

bool container_plugin::is_cgroup_available() const {
#if defined(__linux__)
    // Check for cgroup v2 unified hierarchy
    if (std::filesystem::exists("/sys/fs/cgroup/cgroup.controllers")) {
        return true;
    }

    // Check for cgroup v1
    if (std::filesystem::exists("/sys/fs/cgroup/cpu") ||
        std::filesystem::exists("/sys/fs/cgroup/memory")) {
        return true;
    }
#endif

    return false;
}

container_plugin_config container_plugin::get_config() const { return config_; }

}  // namespace plugins
}  // namespace monitoring
}  // namespace kcenon
