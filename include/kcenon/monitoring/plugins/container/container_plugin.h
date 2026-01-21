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

#pragma once

/**
 * @file container_plugin.h
 * @brief Container monitoring plugin for Docker and Kubernetes metrics
 *
 * This plugin extracts container-related collectors from the core library,
 * making them optional for non-containerized environments. The plugin supports:
 * - Docker container metrics via Docker API or cgroups
 * - Kubernetes pod/deployment metrics (when K8s support is enabled)
 * - cgroup-based metrics for any container runtime
 *
 * Usage:
 * @code
 * #include <kcenon/monitoring/plugins/container/container_plugin.h>
 *
 * // Create plugin with default configuration
 * auto plugin = container_plugin::create();
 *
 * // Or with custom configuration
 * container_plugin_config config;
 * config.enable_docker = true;
 * config.enable_kubernetes = false;
 * config.docker_socket = "/var/run/docker.sock";
 * auto plugin = container_plugin::create(config);
 *
 * // Check if running in container before loading
 * if (container_plugin::is_running_in_container()) {
 *     collector.register_plugin(std::move(plugin));
 * }
 * @endcode
 */

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../collectors/plugin_metric_collector.h"

namespace kcenon {
namespace monitoring {

// Forward declarations
class container_collector;

namespace plugins {

/**
 * @enum container_runtime
 * @brief Supported container runtimes
 */
enum class container_runtime {
    auto_detect,  ///< Automatically detect the container runtime
    docker,       ///< Docker runtime
    containerd,   ///< containerd runtime
    podman,       ///< Podman runtime
    cri_o         ///< CRI-O runtime
};

/**
 * @struct container_plugin_config
 * @brief Configuration options for the container monitoring plugin
 */
struct container_plugin_config {
    /// Container runtime to use (default: auto_detect)
    container_runtime runtime = container_runtime::auto_detect;

    /// Enable Docker metrics collection (default: true)
    bool enable_docker = true;

    /// Enable Kubernetes metrics collection (default: false, requires K8s support)
    bool enable_kubernetes = false;

    /// Enable cgroup-based metrics collection (default: true)
    bool enable_cgroup = true;

    /// Docker socket path (default: /var/run/docker.sock)
    std::string docker_socket = "/var/run/docker.sock";

    /// Kubeconfig path (empty = in-cluster config)
    std::string kubeconfig_path;

    /// Kubernetes namespace filter (empty = all namespaces)
    std::string namespace_filter;

    /// Collect network metrics (default: true)
    bool collect_network_metrics = true;

    /// Collect block I/O metrics (default: true)
    bool collect_blkio_metrics = true;

    /// Collect process/PID metrics (default: true)
    bool collect_pid_metrics = true;
};

/**
 * @class container_plugin
 * @brief Container monitoring plugin aggregating Docker, Kubernetes, and cgroup collectors
 *
 * This plugin provides container-specific metrics collection for containerized deployments.
 * For bare-metal deployments, this plugin should not be loaded to reduce binary size and
 * avoid unnecessary collection overhead.
 *
 * Metrics provided:
 * - Docker: container CPU/memory/network/I/O, running containers count
 * - Kubernetes: pod count, restarts, deployment replicas, node resources
 * - cgroup: CPU time, memory usage/limits, I/O bytes
 */
class container_plugin : public metric_collector_plugin {
   public:
    /**
     * Create a container plugin instance with configuration
     * @param config Plugin configuration options
     * @return Unique pointer to container_plugin instance
     */
    static std::unique_ptr<container_plugin> create(const container_plugin_config& config = {});

    ~container_plugin() override;

    // Disable copy
    container_plugin(const container_plugin&) = delete;
    container_plugin& operator=(const container_plugin&) = delete;

    // metric_collector_plugin interface
    bool initialize(const std::unordered_map<std::string, std::string>& config) override;
    std::vector<metric> collect() override;
    std::string get_name() const override;
    std::vector<std::string> get_metric_types() const override;
    bool is_healthy() const override;
    std::unordered_map<std::string, double> get_statistics() const override;

    /**
     * Check if running inside a container
     * @return True if the current process is running inside a container
     */
    static bool is_running_in_container();

    /**
     * Check if running in a Kubernetes environment
     * @return True if Kubernetes environment variables are detected
     */
    static bool is_kubernetes_environment();

    /**
     * Detect the container runtime in use
     * @return Detected container runtime type
     */
    static container_runtime detect_runtime();

    /**
     * Check if Docker metrics are available
     * @return True if Docker daemon is accessible
     */
    bool is_docker_available() const;

    /**
     * Check if Kubernetes metrics are available
     * @return True if Kubernetes API is accessible
     */
    bool is_kubernetes_available() const;

    /**
     * Check if cgroup metrics are available
     * @return True if cgroup filesystem is accessible
     */
    bool is_cgroup_available() const;

    /**
     * Get the current configuration
     * @return Copy of current configuration
     */
    container_plugin_config get_config() const;

   private:
    explicit container_plugin(const container_plugin_config& config);

    // Internal collectors
    std::unique_ptr<container_collector> container_collector_;

    // Configuration
    container_plugin_config config_;
    bool initialized_{false};

    // Statistics
    mutable std::mutex stats_mutex_;
    std::atomic<size_t> total_collections_{0};
    std::atomic<size_t> collection_errors_{0};
    std::atomic<size_t> containers_found_{0};

    // Helper methods
    void initialize_collectors();
};

}  // namespace plugins
}  // namespace monitoring
}  // namespace kcenon
