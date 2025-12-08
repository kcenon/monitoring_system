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

#include <kcenon/monitoring/collectors/temperature_collector.h>

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include <algorithm>
#include <array>
#include <string>

namespace kcenon {
namespace monitoring {

namespace {

// SMC key types
constexpr uint32_t SMC_KEY_TYPE_SP78 = 0x73703738;  // 'sp78' - signed fixed point 7.8
constexpr uint32_t SMC_KEY_TYPE_FLT = 0x666c7400;   // 'flt ' - float

// SMC commands
constexpr uint8_t SMC_CMD_READ_KEYINFO = 9;
constexpr uint8_t SMC_CMD_READ_BYTES = 5;

// SMC structures
#pragma pack(push, 1)
struct smc_key {
    uint32_t key;
    uint8_t vers[6];
    uint8_t pLimitData;
    uint8_t keyInfo;
};

struct smc_val {
    uint32_t key;
    uint32_t dataSize;
    uint32_t dataType;
    uint8_t bytes[32];
};

struct smc_result {
    uint8_t result;
    uint8_t status;
    uint8_t data8;
    uint32_t data32;
    smc_val val;
};

struct smc_param {
    uint32_t key;
    smc_val val;
    smc_result result;
    uint8_t status;
    uint8_t selector;
    uint8_t data8;
    uint32_t data32;
    smc_key keyInfo;
};
#pragma pack(pop)

// Common SMC temperature keys
// TC0P = CPU Proximity, TC0H = CPU Heatsink, TC0D = CPU Die
// TG0P = GPU Proximity, TG0D = GPU Die
// TA0P = Ambient temperature
const std::array<std::pair<std::string, std::pair<const char*, sensor_type>>, 10> SMC_TEMP_KEYS = {{
    {"TC0P", {"CPU Proximity", sensor_type::cpu}},
    {"TC0D", {"CPU Die", sensor_type::cpu}},
    {"TC0H", {"CPU Heatsink", sensor_type::cpu}},
    {"TCXC", {"CPU Core", sensor_type::cpu}},
    {"TCSA", {"CPU System Agent", sensor_type::cpu}},
    {"TG0P", {"GPU Proximity", sensor_type::gpu}},
    {"TG0D", {"GPU Die", sensor_type::gpu}},
    {"TA0P", {"Ambient", sensor_type::ambient}},
    {"TM0P", {"Memory Proximity", sensor_type::motherboard}},
    {"TPCD", {"Platform Controller Hub", sensor_type::motherboard}},
}};

/**
 * Convert 4-character key string to uint32_t
 */
uint32_t str_to_key(const char* key_str) {
    return (static_cast<uint32_t>(key_str[0]) << 24) | (static_cast<uint32_t>(key_str[1]) << 16) |
           (static_cast<uint32_t>(key_str[2]) << 8) | static_cast<uint32_t>(key_str[3]);
}

/**
 * SMC connection wrapper
 */
class smc_connection {
   public:
    smc_connection() : connection_(0), service_(0) {
        CFMutableDictionaryRef matching = IOServiceMatching("AppleSMC");
        if (!matching) {
            return;
        }

        service_ = IOServiceGetMatchingService(kIOMasterPortDefault, matching);
        if (!service_) {
            return;
        }

        kern_return_t result = IOServiceOpen(service_, mach_task_self(), 0, &connection_);
        if (result != KERN_SUCCESS) {
            IOObjectRelease(service_);
            service_ = 0;
            connection_ = 0;
        }
    }

    ~smc_connection() {
        if (connection_) {
            IOServiceClose(connection_);
        }
        if (service_) {
            IOObjectRelease(service_);
        }
    }

    bool is_valid() const { return connection_ != 0; }

    double read_temperature(uint32_t key) {
        if (!is_valid()) {
            return 0.0;
        }

        smc_param input{};
        smc_param output{};

        input.key = key;
        input.selector = SMC_CMD_READ_KEYINFO;
        input.data32 = 0;

        size_t input_size = sizeof(smc_param);
        size_t output_size = sizeof(smc_param);

        kern_return_t result = IOConnectCallStructMethod(connection_,
                                                         2,  // kSMCHandleYPCEvent
                                                         &input, input_size, &output, &output_size);

        if (result != KERN_SUCCESS || output.result.result != 0) {
            return 0.0;
        }

        // Now read the actual value
        input.key = key;
        input.selector = SMC_CMD_READ_BYTES;
        input.keyInfo.keyInfo = output.keyInfo.keyInfo;
        input.val.dataSize = output.keyInfo.keyInfo;

        result =
            IOConnectCallStructMethod(connection_, 2, &input, input_size, &output, &output_size);

        if (result != KERN_SUCCESS || output.result.result != 0) {
            return 0.0;
        }

        // Parse the value based on data type
        if (output.result.val.dataSize >= 2) {
            // Most temperature values are SP78 (signed fixed-point 7.8)
            int16_t raw_value = (static_cast<int16_t>(output.result.val.bytes[0]) << 8) |
                                static_cast<int16_t>(output.result.val.bytes[1]);
            return static_cast<double>(raw_value) / 256.0;
        }

        return 0.0;
    }

   private:
    io_connect_t connection_;
    io_service_t service_;
};

// Global SMC connection (lazy-initialized)
static std::unique_ptr<smc_connection> g_smc;
static std::mutex g_smc_mutex;

smc_connection* get_smc() {
    std::lock_guard<std::mutex> lock(g_smc_mutex);
    if (!g_smc) {
        g_smc = std::make_unique<smc_connection>();
    }
    return g_smc.get();
}

}  // anonymous namespace

// temperature_info_collector implementation for macOS

temperature_info_collector::temperature_info_collector() = default;
temperature_info_collector::~temperature_info_collector() = default;

bool temperature_info_collector::is_thermal_available() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (thermal_checked_) {
        return thermal_available_;
    }

    thermal_checked_ = true;

    auto* smc = get_smc();
    thermal_available_ = (smc != nullptr && smc->is_valid());

    return thermal_available_;
}

std::vector<temperature_sensor_info> temperature_info_collector::enumerate_sensors() {
    std::lock_guard<std::mutex> lock(mutex_);
    return enumerate_sensors_impl();
}

std::vector<temperature_sensor_info> temperature_info_collector::enumerate_sensors_impl() {
    std::vector<temperature_sensor_info> sensors;

    auto* smc = get_smc();
    if (!smc || !smc->is_valid()) {
        return sensors;
    }

    // Check which keys are available
    for (const auto& [key_str, info] : SMC_TEMP_KEYS) {
        uint32_t key = str_to_key(key_str.c_str());
        double temp = smc->read_temperature(key);

        // If we got a valid temperature, the sensor exists
        if (temp > 0.0 && temp < 200.0) {
            temperature_sensor_info sensor;
            sensor.id = key_str;
            sensor.name = info.first;
            sensor.zone_path = key_str;  // Use key as path on macOS
            sensor.type = info.second;
            sensors.push_back(sensor);
        }
    }

    cached_sensors_ = sensors;
    return sensors;
}

temperature_reading temperature_info_collector::read_temperature(
    const temperature_sensor_info& sensor) {
    std::lock_guard<std::mutex> lock(mutex_);
    return read_temperature_impl(sensor);
}

temperature_reading temperature_info_collector::read_temperature_impl(
    const temperature_sensor_info& sensor) {
    temperature_reading reading;
    reading.sensor = sensor;
    reading.timestamp = std::chrono::system_clock::now();

    auto* smc = get_smc();
    if (!smc || !smc->is_valid()) {
        return reading;
    }

    // The zone_path contains the SMC key
    if (sensor.zone_path.size() == 4) {
        uint32_t key = str_to_key(sensor.zone_path.c_str());
        double temp = smc->read_temperature(key);
        if (temp > 0.0 && temp < 200.0) {
            reading.temperature_celsius = temp;
        }
    }

    // macOS SMC doesn't typically expose thresholds directly
    // Use reasonable defaults for Macs
    if (reading.sensor.type == sensor_type::cpu) {
        reading.thresholds_available = true;
        reading.critical_threshold_celsius = 105.0;  // Intel/Apple Silicon TJmax
        reading.warning_threshold_celsius = 90.0;
    } else if (reading.sensor.type == sensor_type::gpu) {
        reading.thresholds_available = true;
        reading.critical_threshold_celsius = 95.0;
        reading.warning_threshold_celsius = 85.0;
    }

    // Check threshold status
    if (reading.thresholds_available) {
        if (reading.critical_threshold_celsius > 0.0 &&
            reading.temperature_celsius >= reading.critical_threshold_celsius) {
            reading.is_critical = true;
        }
        if (reading.warning_threshold_celsius > 0.0 &&
            reading.temperature_celsius >= reading.warning_threshold_celsius) {
            reading.is_warning = true;
        }
    }

    return reading;
}

std::vector<temperature_reading> temperature_info_collector::read_all_temperatures() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<temperature_reading> readings;

    // Refresh sensor list if empty
    if (cached_sensors_.empty()) {
        enumerate_sensors_impl();
    }

    for (const auto& sensor : cached_sensors_) {
        readings.push_back(read_temperature_impl(sensor));
    }

    return readings;
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(__APPLE__)
