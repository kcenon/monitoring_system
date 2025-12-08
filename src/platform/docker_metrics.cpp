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

/**
 * @file docker_metrics.cpp
 * @brief Docker API implementation for container metrics collection
 *
 * This file provides Docker API integration for collecting container metrics.
 * It uses Unix domain socket to communicate with the Docker daemon.
 *
 * Note: This is a stub implementation. Full Docker API support requires
 * additional dependencies (e.g., libcurl, nlohmann/json) and can be enabled
 * with the MONITORING_WITH_DOCKER_API compile-time flag.
 */

#include <kcenon/monitoring/collectors/container_collector.h>

// Docker API support is optional and only available when explicitly enabled
#if defined(MONITORING_WITH_DOCKER_API)

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// Unix socket support
#if defined(__linux__) || defined(__APPLE__)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace kcenon {
namespace monitoring {

namespace {

constexpr const char* DOCKER_SOCKET_PATH = "/var/run/docker.sock";

// Check if Docker socket is available
bool is_docker_available() {
    namespace fs = std::filesystem;
    return fs::exists(DOCKER_SOCKET_PATH);
}

// Simple HTTP request over Unix socket
// This is a minimal implementation. For production use, consider using libcurl.
std::string http_get_unix_socket(const std::string& path) {
#if defined(__linux__) || defined(__APPLE__)
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        return "";
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, DOCKER_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return "";
    }

    // Send HTTP request
    std::string request = "GET " + path +
                          " HTTP/1.0\r\n"
                          "Host: localhost\r\n"
                          "Accept: application/json\r\n\r\n";

    if (write(sock, request.c_str(), request.length()) < 0) {
        close(sock);
        return "";
    }

    // Read response
    std::string response;
    char buffer[4096];
    ssize_t bytes;
    while ((bytes = read(sock, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes] = '\0';
        response += buffer;
    }

    close(sock);

    // Extract body from HTTP response
    size_t body_start = response.find("\r\n\r\n");
    if (body_start != std::string::npos) {
        return response.substr(body_start + 4);
    }

    return "";
#else
    return "";
#endif
}

}  // anonymous namespace

// Docker-specific container enumeration would go here
// This requires JSON parsing of the Docker API response
// For now, we rely on cgroup enumeration which works without additional dependencies

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(MONITORING_WITH_DOCKER_API)

// Non-Linux platforms stub implementation
#if !defined(__linux__)

namespace kcenon {
namespace monitoring {

container_info_collector::container_info_collector() = default;
container_info_collector::~container_info_collector() = default;

cgroup_version container_info_collector::detect_cgroup_version() const {
    return cgroup_version::none;
}

bool container_info_collector::is_containerized() const { return false; }

std::vector<container_info> container_info_collector::enumerate_containers() { return {}; }

container_metrics container_info_collector::collect_container_metrics(const container_info&) {
    return container_metrics{};
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // !defined(__linux__)
