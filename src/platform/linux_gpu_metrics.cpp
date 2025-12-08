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

#if defined(__linux__)

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace kcenon {
namespace monitoring {
namespace {

// PCI Vendor IDs
constexpr uint16_t VENDOR_NVIDIA = 0x10de;
constexpr uint16_t VENDOR_AMD = 0x1002;
constexpr uint16_t VENDOR_INTEL = 0x8086;

// DRM base path
const std::string DRM_PATH = "/sys/class/drm";

/**
 * Read a single value from a sysfs file
 * @param path Path to the sysfs file
 * @return String content or empty string on failure
 */
std::string read_sysfs_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::string content;
    std::getline(file, content);
    return content;
}

/**
 * Read a numeric value from a sysfs file
 * @param path Path to the sysfs file
 * @param value Output value
 * @return true if successful
 */
template <typename T>
bool read_sysfs_value(const std::string& path, T& value) {
    std::string content = read_sysfs_file(path);
    if (content.empty()) {
        return false;
    }
    try {
        if constexpr (std::is_floating_point_v<T>) {
            value = static_cast<T>(std::stod(content));
        } else if constexpr (std::is_signed_v<T>) {
            value = static_cast<T>(std::stoll(content));
        } else {
            value = static_cast<T>(std::stoull(content));
        }
        return true;
    } catch (...) {
        return false;
    }
}

/**
 * Parse PCI vendor ID from sysfs vendor file
 * @param vendor_path Path to vendor file
 * @return Vendor ID or 0 on failure
 */
uint16_t parse_vendor_id(const std::string& vendor_path) {
    std::string content = read_sysfs_file(vendor_path);
    if (content.empty()) {
        return 0;
    }
    try {
        // Format: 0x1234
        return static_cast<uint16_t>(std::stoul(content, nullptr, 16));
    } catch (...) {
        return 0;
    }
}

/**
 * Convert vendor ID to gpu_vendor enum
 */
gpu_vendor vendor_id_to_enum(uint16_t vendor_id) {
    switch (vendor_id) {
        case VENDOR_NVIDIA:
            return gpu_vendor::nvidia;
        case VENDOR_AMD:
            return gpu_vendor::amd;
        case VENDOR_INTEL:
            return gpu_vendor::intel;
        default:
            return vendor_id != 0 ? gpu_vendor::other : gpu_vendor::unknown;
    }
}

/**
 * Find hwmon directory for a GPU device
 * @param device_path Path to the device
 * @return Path to hwmon directory or empty string
 */
std::string find_hwmon_path(const std::string& device_path) {
    std::string hwmon_base = device_path + "/hwmon";
    if (!std::filesystem::exists(hwmon_base)) {
        return "";
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator(hwmon_base)) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().string();
                if (name.find("hwmon") == 0) {
                    return entry.path().string();
                }
            }
        }
    } catch (...) {
        // Directory enumeration failed
    }

    return "";
}

/**
 * Get GPU name from device path
 */
std::string get_gpu_name(const std::string& device_path, gpu_vendor vendor) {
    // Try to read product name from various sources
    std::string name = read_sysfs_file(device_path + "/product_name");
    if (!name.empty()) {
        return name;
    }

    // Try label from hwmon
    std::string hwmon_path = find_hwmon_path(device_path);
    if (!hwmon_path.empty()) {
        name = read_sysfs_file(hwmon_path + "/name");
        if (!name.empty()) {
            return name;
        }
    }

    // Generate a default name based on vendor
    switch (vendor) {
        case gpu_vendor::nvidia:
            return "NVIDIA GPU";
        case gpu_vendor::amd:
            return "AMD GPU";
        case gpu_vendor::intel:
            return "Intel GPU";
        default:
            return "Unknown GPU";
    }
}

/**
 * Determine if GPU is discrete or integrated
 */
gpu_type determine_gpu_type(const std::string& device_path, gpu_vendor vendor) {
    // Intel GPUs in laptops/desktops are typically integrated
    if (vendor == gpu_vendor::intel) {
        return gpu_type::integrated;
    }

    // Check for boot_vga - primary GPU is usually discrete
    std::string boot_vga = read_sysfs_file(device_path + "/boot_vga");
    if (boot_vga == "1") {
        // Could be either, but NVIDIA/AMD with boot_vga are typically discrete
        if (vendor == gpu_vendor::nvidia || vendor == gpu_vendor::amd) {
            return gpu_type::discrete;
        }
    }

    // Default: NVIDIA and AMD are usually discrete
    if (vendor == gpu_vendor::nvidia || vendor == gpu_vendor::amd) {
        return gpu_type::discrete;
    }

    return gpu_type::unknown;
}

/**
 * Read AMD GPU utilization from gpu_busy_percent
 */
bool read_amd_utilization(const std::string& device_path, double& utilization) {
    return read_sysfs_value(device_path + "/gpu_busy_percent", utilization);
}

/**
 * Read AMD GPU memory info
 */
bool read_amd_memory(const std::string& device_path, uint64_t& used, uint64_t& total) {
    bool success = false;
    if (read_sysfs_value(device_path + "/mem_info_vram_used", used)) {
        success = true;
    }
    if (read_sysfs_value(device_path + "/mem_info_vram_total", total)) {
        success = true;
    }
    return success;
}

/**
 * Read temperature from hwmon
 */
bool read_hwmon_temperature(const std::string& hwmon_path, double& temperature) {
    int64_t temp_millicelsius = 0;
    // Try temp1_input first, then temp2_input
    if (read_sysfs_value(hwmon_path + "/temp1_input", temp_millicelsius) ||
        read_sysfs_value(hwmon_path + "/temp2_input", temp_millicelsius)) {
        temperature = static_cast<double>(temp_millicelsius) / 1000.0;
        return true;
    }
    return false;
}

/**
 * Read power from hwmon (in microwatts)
 */
bool read_hwmon_power(const std::string& hwmon_path, double& power) {
    int64_t power_microwatts = 0;
    if (read_sysfs_value(hwmon_path + "/power1_average", power_microwatts) ||
        read_sysfs_value(hwmon_path + "/power1_input", power_microwatts)) {
        power = static_cast<double>(power_microwatts) / 1000000.0;
        return true;
    }
    return false;
}

/**
 * Read fan speed from hwmon (PWM value 0-255)
 */
bool read_hwmon_fan(const std::string& hwmon_path, double& fan_percent) {
    int64_t pwm = 0;
    if (read_sysfs_value(hwmon_path + "/pwm1", pwm)) {
        fan_percent = (static_cast<double>(pwm) / 255.0) * 100.0;
        return true;
    }
    // Try fan RPM if PWM not available
    int64_t rpm = 0;
    int64_t rpm_max = 0;
    if (read_sysfs_value(hwmon_path + "/fan1_input", rpm)) {
        if (read_sysfs_value(hwmon_path + "/fan1_max", rpm_max) && rpm_max > 0) {
            fan_percent = (static_cast<double>(rpm) / static_cast<double>(rpm_max)) * 100.0;
        } else {
            // Estimate based on typical max RPM (5000)
            fan_percent = std::min(100.0, (static_cast<double>(rpm) / 5000.0) * 100.0);
        }
        return true;
    }
    return false;
}

/**
 * Parse AMD clock from pp_dpm_sclk
 * Format: "0: 300Mhz\n1: 500Mhz *\n2: 800Mhz"
 * The line with '*' is the current frequency
 */
bool read_amd_clock(const std::string& device_path, double& clock_mhz) {
    std::ifstream file(device_path + "/pp_dpm_sclk");
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.find('*') != std::string::npos) {
            // Parse frequency from line like "1: 500Mhz *"
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                std::string freq_str = line.substr(pos + 1);
                // Remove whitespace and parse
                try {
                    clock_mhz = std::stod(freq_str);
                    return true;
                } catch (...) {
                    return false;
                }
            }
        }
    }
    return false;
}

}  // anonymous namespace

// gpu_info_collector implementation for Linux

gpu_info_collector::gpu_info_collector() {}
gpu_info_collector::~gpu_info_collector() {}

bool gpu_info_collector::is_gpu_available() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (gpu_checked_) {
        return gpu_available_;
    }

    gpu_checked_ = true;
    gpu_available_ = false;

    // Check if DRM subsystem exists
    if (!std::filesystem::exists(DRM_PATH)) {
        return false;
    }

    // Look for any card* directories with a PCI device
    try {
        for (const auto& entry : std::filesystem::directory_iterator(DRM_PATH)) {
            std::string name = entry.path().filename().string();
            if (name.find("card") == 0 && name.find('-') == std::string::npos) {
                std::string device_path = entry.path().string() + "/device";
                if (std::filesystem::exists(device_path + "/vendor")) {
                    uint16_t vendor_id = parse_vendor_id(device_path + "/vendor");
                    if (vendor_id == VENDOR_NVIDIA || vendor_id == VENDOR_AMD ||
                        vendor_id == VENDOR_INTEL) {
                        gpu_available_ = true;
                        return true;
                    }
                }
            }
        }
    } catch (...) {
        // Directory enumeration failed
    }

    return false;
}

std::vector<gpu_device_info> gpu_info_collector::enumerate_gpus() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!cached_devices_.empty()) {
        return cached_devices_;
    }

    cached_devices_ = enumerate_gpus_impl();
    return cached_devices_;
}

std::vector<gpu_device_info> gpu_info_collector::enumerate_gpus_impl() {
    std::vector<gpu_device_info> devices;

    if (!std::filesystem::exists(DRM_PATH)) {
        return devices;
    }

    uint32_t device_index = 0;

    try {
        // Collect card directories and sort them
        std::vector<std::filesystem::path> card_paths;
        for (const auto& entry : std::filesystem::directory_iterator(DRM_PATH)) {
            std::string name = entry.path().filename().string();
            // Match card0, card1, etc. but not card0-DP-1, etc.
            if (name.find("card") == 0 && name.find('-') == std::string::npos) {
                card_paths.push_back(entry.path());
            }
        }

        // Sort by card number
        std::sort(card_paths.begin(), card_paths.end());

        for (const auto& card_path : card_paths) {
            std::string device_path = card_path.string() + "/device";
            std::string vendor_path = device_path + "/vendor";

            if (!std::filesystem::exists(vendor_path)) {
                continue;
            }

            uint16_t vendor_id = parse_vendor_id(vendor_path);
            gpu_vendor vendor = vendor_id_to_enum(vendor_id);

            // Only include known GPU vendors
            if (vendor == gpu_vendor::unknown) {
                continue;
            }

            gpu_device_info info;
            info.id = "gpu" + std::to_string(device_index);
            info.device_path = device_path;
            info.vendor = vendor;
            info.type = determine_gpu_type(device_path, vendor);
            info.device_index = device_index;
            info.name = get_gpu_name(device_path, vendor);

            // Try to get driver version
            std::string driver_path = device_path + "/driver/module/version";
            info.driver_version = read_sysfs_file(driver_path);

            devices.push_back(info);
            ++device_index;
        }
    } catch (...) {
        // Directory enumeration failed
    }

    return devices;
}

gpu_reading gpu_info_collector::read_gpu_metrics(const gpu_device_info& device) {
    std::lock_guard<std::mutex> lock(mutex_);
    return read_gpu_metrics_impl(device);
}

gpu_reading gpu_info_collector::read_gpu_metrics_impl(const gpu_device_info& device) {
    gpu_reading reading;
    reading.device = device;
    reading.timestamp = std::chrono::system_clock::now();

    std::string hwmon_path = find_hwmon_path(device.device_path);

    // Read vendor-specific metrics
    switch (device.vendor) {
        case gpu_vendor::amd: {
            // AMD-specific metrics
            if (read_amd_utilization(device.device_path, reading.utilization_percent)) {
                reading.utilization_available = true;
            }

            if (read_amd_memory(device.device_path, reading.memory_used_bytes,
                                reading.memory_total_bytes)) {
                reading.memory_available = true;
            }

            if (read_amd_clock(device.device_path, reading.clock_mhz)) {
                reading.clock_available = true;
            }
            break;
        }

        case gpu_vendor::nvidia: {
            // NVIDIA metrics via sysfs (limited without NVML)
            // Most NVIDIA metrics require NVML, but we can try hwmon
            break;
        }

        case gpu_vendor::intel: {
            // Intel i915 metrics (limited)
            break;
        }

        default:
            break;
    }

    // Common hwmon metrics (temperature, power, fan)
    if (!hwmon_path.empty()) {
        if (read_hwmon_temperature(hwmon_path, reading.temperature_celsius)) {
            reading.temperature_available = true;
        }

        if (read_hwmon_power(hwmon_path, reading.power_watts)) {
            reading.power_available = true;
        }

        if (read_hwmon_fan(hwmon_path, reading.fan_speed_percent)) {
            reading.fan_available = true;
        }
    }

    return reading;
}

std::vector<gpu_reading> gpu_info_collector::read_all_gpu_metrics() {
    std::vector<gpu_reading> readings;

    auto devices = enumerate_gpus();
    readings.reserve(devices.size());

    for (const auto& device : devices) {
        readings.push_back(read_gpu_metrics(device));
    }

    return readings;
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // __linux__
