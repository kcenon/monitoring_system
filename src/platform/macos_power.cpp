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

#include <kcenon/monitoring/collectors/power_collector.h>

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>

#include <algorithm>
#include <array>
#include <string>

namespace kcenon {
namespace monitoring {

namespace {

// SMC key types for power
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

// SMC power-related keys
// PSTR = System Total Power, PCPT = CPU Package Power, etc.
const std::array<std::pair<std::string, std::pair<const char*, power_source_type>>, 6> SMC_POWER_KEYS = {{
    {"PSTR", {"System Total", power_source_type::platform}},
    {"PCPT", {"CPU Package", power_source_type::package}},
    {"PCPC", {"CPU Core", power_source_type::cpu}},
    {"PCPG", {"GPU", power_source_type::gpu}},
    {"PDTR", {"Platform Total", power_source_type::platform}},
    {"PM0R", {"Memory", power_source_type::memory}},
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

        service_ = IOServiceGetMatchingService(kIOMainPortDefault, matching);
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

    double read_power(uint32_t key) {
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

        // Parse the value - power values are typically floats or sp78
        if (output.result.val.dataSize >= 4) {
            // Try as float first
            float value;
            memcpy(&value, output.result.val.bytes, sizeof(float));
            if (value >= 0.0f && value < 1000.0f) {  // Sanity check
                return static_cast<double>(value);
            }
        }

        if (output.result.val.dataSize >= 2) {
            // Try as SP78 (signed fixed-point 7.8)
            int16_t raw_value = (static_cast<int16_t>(output.result.val.bytes[0]) << 8) |
                                static_cast<int16_t>(output.result.val.bytes[1]);
            double value = static_cast<double>(raw_value) / 256.0;
            if (value >= 0.0 && value < 1000.0) {  // Sanity check
                return value;
            }
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

/**
 * Get battery info from IOPowerSources
 */
struct battery_info {
    bool available;
    bool is_charging;
    bool is_ac_attached;
    double capacity_percent;
    double voltage;
    double amperage;
    double max_capacity;
    double current_capacity;
    double time_remaining;
};

battery_info get_battery_info() {
    battery_info info{};

    CFTypeRef info_blob = IOPSCopyPowerSourcesInfo();
    if (!info_blob) {
        return info;
    }

    CFArrayRef sources = IOPSCopyPowerSourcesList(info_blob);
    if (!sources) {
        CFRelease(info_blob);
        return info;
    }

    CFIndex count = CFArrayGetCount(sources);
    for (CFIndex i = 0; i < count; i++) {
        CFDictionaryRef source_dict = IOPSGetPowerSourceDescription(
            info_blob, CFArrayGetValueAtIndex(sources, i));
        if (!source_dict) {
            continue;
        }

        // Check if this is a battery
        CFStringRef type = static_cast<CFStringRef>(
            CFDictionaryGetValue(source_dict, CFSTR(kIOPSTypeKey)));
        if (!type || CFStringCompare(type, CFSTR(kIOPSInternalBatteryType), 0) != kCFCompareEqualTo) {
            continue;
        }

        info.available = true;

        // Get charging state
        CFStringRef power_source = static_cast<CFStringRef>(
            CFDictionaryGetValue(source_dict, CFSTR(kIOPSPowerSourceStateKey)));
        if (power_source) {
            info.is_ac_attached = (CFStringCompare(power_source, 
                CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo);
        }

        CFBooleanRef is_charging_ref = static_cast<CFBooleanRef>(
            CFDictionaryGetValue(source_dict, CFSTR(kIOPSIsChargingKey)));
        if (is_charging_ref) {
            info.is_charging = CFBooleanGetValue(is_charging_ref);
        }

        // Get capacity percent
        CFNumberRef current_cap = static_cast<CFNumberRef>(
            CFDictionaryGetValue(source_dict, CFSTR(kIOPSCurrentCapacityKey)));
        CFNumberRef max_cap = static_cast<CFNumberRef>(
            CFDictionaryGetValue(source_dict, CFSTR(kIOPSMaxCapacityKey)));
        if (current_cap && max_cap) {
            int current_val = 0, max_val = 0;
            CFNumberGetValue(current_cap, kCFNumberIntType, &current_val);
            CFNumberGetValue(max_cap, kCFNumberIntType, &max_val);
            info.current_capacity = static_cast<double>(current_val);
            info.max_capacity = static_cast<double>(max_val);
            if (max_val > 0) {
                info.capacity_percent = (static_cast<double>(current_val) / max_val) * 100.0;
            }
        }

        // Get time remaining
        CFNumberRef time_remaining = static_cast<CFNumberRef>(
            CFDictionaryGetValue(source_dict, CFSTR(kIOPSTimeToEmptyKey)));
        if (time_remaining) {
            int minutes = 0;
            CFNumberGetValue(time_remaining, kCFNumberIntType, &minutes);
            info.time_remaining = static_cast<double>(minutes);
        }

        break;  // Only get first battery
    }

    CFRelease(sources);
    CFRelease(info_blob);

    return info;
}

}  // anonymous namespace

// power_info_collector implementation for macOS

power_info_collector::power_info_collector()
    : last_reading_time_(std::chrono::steady_clock::now()) {}

power_info_collector::~power_info_collector() = default;

bool power_info_collector::is_power_available() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (power_checked_) {
        return power_available_;
    }

    power_checked_ = true;

    // Check SMC availability
    auto* smc = get_smc();
    if (smc && smc->is_valid()) {
        power_available_ = true;
        return true;
    }

    // Check battery availability
    battery_info bat = get_battery_info();
    power_available_ = bat.available;

    return power_available_;
}

std::vector<power_source_info> power_info_collector::enumerate_sources() {
    std::lock_guard<std::mutex> lock(mutex_);
    return enumerate_sources_impl();
}

std::vector<power_source_info> power_info_collector::enumerate_sources_impl() {
    std::vector<power_source_info> sources;

    // Add battery source if available
    battery_info bat = get_battery_info();
    if (bat.available) {
        power_source_info battery_source;
        battery_source.id = "battery_internal";
        battery_source.name = "Internal Battery";
        battery_source.path = "iokit:battery";
        battery_source.type = power_source_type::battery;
        sources.push_back(battery_source);

        // Add AC adapter source
        power_source_info ac_source;
        ac_source.id = "ac_adapter";
        ac_source.name = "AC Adapter";
        ac_source.path = "iokit:ac";
        ac_source.type = power_source_type::ac;
        sources.push_back(ac_source);
    }

    // Check SMC for power metrics
    auto* smc = get_smc();
    if (smc && smc->is_valid()) {
        for (const auto& [key_str, info] : SMC_POWER_KEYS) {
            uint32_t key = str_to_key(key_str.c_str());
            double power = smc->read_power(key);

            // If we got a valid power reading, the source exists
            if (power > 0.0 && power < 500.0) {  // Reasonable power range
                power_source_info source;
                source.id = "smc_" + key_str;
                source.name = info.first;
                source.path = key_str;  // Use key as path on macOS
                source.type = info.second;
                sources.push_back(source);
            }
        }
    }

    cached_sources_ = sources;
    return sources;
}

power_reading power_info_collector::read_power(const power_source_info& source) {
    std::lock_guard<std::mutex> lock(mutex_);
    return read_power_impl(source);
}

power_reading power_info_collector::read_power_impl(const power_source_info& source) {
    power_reading reading;
    reading.source = source;
    reading.timestamp = std::chrono::system_clock::now();

    // Handle SMC sources
    if (source.id.find("smc_") == 0) {
        auto* smc = get_smc();
        if (smc && smc->is_valid() && source.path.size() == 4) {
            uint32_t key = str_to_key(source.path.c_str());
            double power = smc->read_power(key);
            if (power > 0.0 && power < 500.0) {
                reading.power_watts = power;
                reading.power_available = true;
            }
        }
        return reading;
    }

    // Handle battery source
    if (source.type == power_source_type::battery) {
        battery_info bat = get_battery_info();
        if (bat.available) {
            reading.battery_available = true;
            reading.battery_percent = bat.capacity_percent;
            reading.is_charging = bat.is_charging;
            reading.is_discharging = !bat.is_charging && !bat.is_ac_attached;
            reading.is_full = (bat.capacity_percent >= 99.0 && bat.is_ac_attached);
        }
        return reading;
    }

    // Handle AC adapter source
    if (source.type == power_source_type::ac) {
        battery_info bat = get_battery_info();
        reading.power_available = bat.is_ac_attached;
        return reading;
    }

    return reading;
}

std::vector<power_reading> power_info_collector::read_all_power() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<power_reading> readings;

    // Refresh source list if empty
    if (cached_sources_.empty()) {
        enumerate_sources_impl();
    }

    auto now = std::chrono::steady_clock::now();
    for (const auto& source : cached_sources_) {
        readings.push_back(read_power_impl(source));
    }
    last_reading_time_ = now;

    return readings;
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(__APPLE__)
