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

#include <kcenon/monitoring/collectors/battery_collector.h>

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>

#include <string>

namespace kcenon {
namespace monitoring {

namespace {

/**
 * Structure to hold raw battery data from IOKit
 */
struct iokit_battery_data {
    bool found{false};

    // Identity
    std::string name;
    std::string manufacturer;
    std::string device_name;
    std::string serial;

    // Status
    bool is_present{false};
    bool is_charging{false};
    bool is_charged{false};
    bool is_ac_attached{false};
    bool is_finishing_charge{false};

    // Capacity
    int64_t current_capacity{0};
    int64_t max_capacity{0};
    int64_t design_capacity{0};

    // Electrical
    int64_t voltage_mv{0};
    int64_t amperage_ma{0};
    int64_t instantaneous_amperage_ma{0};

    // Time
    int64_t time_to_empty_minutes{-1};
    int64_t time_to_full_minutes{-1};

    // Health
    int64_t cycle_count{-1};
    int64_t temperature_decikelvin{0};
    int64_t design_cycle_count{-1};
};

/**
 * Get detailed battery info from IORegistry
 */
iokit_battery_data get_iokit_battery_data() {
    iokit_battery_data data;

    // Find battery service
    io_service_t battery_service = IOServiceGetMatchingService(
        kIOMainPortDefault, IOServiceMatching("AppleSmartBattery"));

    if (!battery_service) {
        return data;
    }

    // Get properties dictionary
    CFMutableDictionaryRef props = nullptr;
    if (IORegistryEntryCreateCFProperties(battery_service, &props,
                                          kCFAllocatorDefault, 0) != KERN_SUCCESS) {
        IOObjectRelease(battery_service);
        return data;
    }

    data.found = true;
    data.is_present = true;

    // Get manufacturer
    CFStringRef manufacturer = static_cast<CFStringRef>(
        CFDictionaryGetValue(props, CFSTR("Manufacturer")));
    if (manufacturer && CFGetTypeID(manufacturer) == CFStringGetTypeID()) {
        char buffer[256];
        if (CFStringGetCString(manufacturer, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
            data.manufacturer = buffer;
        }
    }

    // Get device name
    CFStringRef device_name = static_cast<CFStringRef>(
        CFDictionaryGetValue(props, CFSTR("DeviceName")));
    if (device_name && CFGetTypeID(device_name) == CFStringGetTypeID()) {
        char buffer[256];
        if (CFStringGetCString(device_name, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
            data.device_name = buffer;
            data.name = buffer;
        }
    }

    // Get serial number
    CFStringRef serial = static_cast<CFStringRef>(
        CFDictionaryGetValue(props, CFSTR("BatterySerialNumber")));
    if (serial && CFGetTypeID(serial) == CFStringGetTypeID()) {
        char buffer[256];
        if (CFStringGetCString(serial, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
            data.serial = buffer;
        }
    }

    // Get status flags
    CFBooleanRef is_charging = static_cast<CFBooleanRef>(
        CFDictionaryGetValue(props, CFSTR("IsCharging")));
    if (is_charging && CFGetTypeID(is_charging) == CFBooleanGetTypeID()) {
        data.is_charging = CFBooleanGetValue(is_charging);
    }

    CFBooleanRef fully_charged = static_cast<CFBooleanRef>(
        CFDictionaryGetValue(props, CFSTR("FullyCharged")));
    if (fully_charged && CFGetTypeID(fully_charged) == CFBooleanGetTypeID()) {
        data.is_charged = CFBooleanGetValue(fully_charged);
    }

    CFBooleanRef external_connected = static_cast<CFBooleanRef>(
        CFDictionaryGetValue(props, CFSTR("ExternalConnected")));
    if (external_connected && CFGetTypeID(external_connected) == CFBooleanGetTypeID()) {
        data.is_ac_attached = CFBooleanGetValue(external_connected);
    }

    // Get capacity
    CFNumberRef current_cap = static_cast<CFNumberRef>(
        CFDictionaryGetValue(props, CFSTR("CurrentCapacity")));
    if (current_cap && CFGetTypeID(current_cap) == CFNumberGetTypeID()) {
        CFNumberGetValue(current_cap, kCFNumberSInt64Type, &data.current_capacity);
    }

    CFNumberRef max_cap = static_cast<CFNumberRef>(
        CFDictionaryGetValue(props, CFSTR("MaxCapacity")));
    if (max_cap && CFGetTypeID(max_cap) == CFNumberGetTypeID()) {
        CFNumberGetValue(max_cap, kCFNumberSInt64Type, &data.max_capacity);
    }

    CFNumberRef design_cap = static_cast<CFNumberRef>(
        CFDictionaryGetValue(props, CFSTR("DesignCapacity")));
    if (design_cap && CFGetTypeID(design_cap) == CFNumberGetTypeID()) {
        CFNumberGetValue(design_cap, kCFNumberSInt64Type, &data.design_capacity);
    }

    // Get electrical values
    CFNumberRef voltage = static_cast<CFNumberRef>(
        CFDictionaryGetValue(props, CFSTR("Voltage")));
    if (voltage && CFGetTypeID(voltage) == CFNumberGetTypeID()) {
        CFNumberGetValue(voltage, kCFNumberSInt64Type, &data.voltage_mv);
    }

    CFNumberRef amperage = static_cast<CFNumberRef>(
        CFDictionaryGetValue(props, CFSTR("Amperage")));
    if (amperage && CFGetTypeID(amperage) == CFNumberGetTypeID()) {
        CFNumberGetValue(amperage, kCFNumberSInt64Type, &data.amperage_ma);
    }

    CFNumberRef instant_amp = static_cast<CFNumberRef>(
        CFDictionaryGetValue(props, CFSTR("InstantAmperage")));
    if (instant_amp && CFGetTypeID(instant_amp) == CFNumberGetTypeID()) {
        CFNumberGetValue(instant_amp, kCFNumberSInt64Type, &data.instantaneous_amperage_ma);
    }

    // Get time estimates
    CFNumberRef time_remaining = static_cast<CFNumberRef>(
        CFDictionaryGetValue(props, CFSTR("TimeRemaining")));
    if (time_remaining && CFGetTypeID(time_remaining) == CFNumberGetTypeID()) {
        int64_t minutes = 0;
        CFNumberGetValue(time_remaining, kCFNumberSInt64Type, &minutes);
        if (data.is_charging) {
            data.time_to_full_minutes = minutes;
        } else {
            data.time_to_empty_minutes = minutes;
        }
    }

    CFNumberRef avg_time_full = static_cast<CFNumberRef>(
        CFDictionaryGetValue(props, CFSTR("AvgTimeToFull")));
    if (avg_time_full && CFGetTypeID(avg_time_full) == CFNumberGetTypeID()) {
        int64_t minutes = 0;
        CFNumberGetValue(avg_time_full, kCFNumberSInt64Type, &minutes);
        if (minutes > 0 && minutes < 65535) {
            data.time_to_full_minutes = minutes;
        }
    }

    CFNumberRef avg_time_empty = static_cast<CFNumberRef>(
        CFDictionaryGetValue(props, CFSTR("AvgTimeToEmpty")));
    if (avg_time_empty && CFGetTypeID(avg_time_empty) == CFNumberGetTypeID()) {
        int64_t minutes = 0;
        CFNumberGetValue(avg_time_empty, kCFNumberSInt64Type, &minutes);
        if (minutes > 0 && minutes < 65535) {
            data.time_to_empty_minutes = minutes;
        }
    }

    // Get health info
    CFNumberRef cycle_count = static_cast<CFNumberRef>(
        CFDictionaryGetValue(props, CFSTR("CycleCount")));
    if (cycle_count && CFGetTypeID(cycle_count) == CFNumberGetTypeID()) {
        CFNumberGetValue(cycle_count, kCFNumberSInt64Type, &data.cycle_count);
    }

    CFNumberRef temperature = static_cast<CFNumberRef>(
        CFDictionaryGetValue(props, CFSTR("Temperature")));
    if (temperature && CFGetTypeID(temperature) == CFNumberGetTypeID()) {
        CFNumberGetValue(temperature, kCFNumberSInt64Type, &data.temperature_decikelvin);
    }

    CFRelease(props);
    IOObjectRelease(battery_service);

    return data;
}

/**
 * Get battery info from IOPowerSources (simpler API)
 */
struct power_source_battery_info {
    bool available{false};
    bool is_charging{false};
    bool is_ac_attached{false};
    double capacity_percent{0.0};
    int64_t time_to_empty_minutes{-1};
    int64_t time_to_full_minutes{-1};
};

power_source_battery_info get_power_source_info() {
    power_source_battery_info info;

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

        // Get power source state
        CFStringRef power_source = static_cast<CFStringRef>(
            CFDictionaryGetValue(source_dict, CFSTR(kIOPSPowerSourceStateKey)));
        if (power_source) {
            info.is_ac_attached = (CFStringCompare(power_source,
                CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo);
        }

        // Get charging state
        CFBooleanRef is_charging_ref = static_cast<CFBooleanRef>(
            CFDictionaryGetValue(source_dict, CFSTR(kIOPSIsChargingKey)));
        if (is_charging_ref && CFGetTypeID(is_charging_ref) == CFBooleanGetTypeID()) {
            info.is_charging = CFBooleanGetValue(is_charging_ref);
        }

        // Get capacity
        CFNumberRef current_cap = static_cast<CFNumberRef>(
            CFDictionaryGetValue(source_dict, CFSTR(kIOPSCurrentCapacityKey)));
        CFNumberRef max_cap = static_cast<CFNumberRef>(
            CFDictionaryGetValue(source_dict, CFSTR(kIOPSMaxCapacityKey)));
        if (current_cap && max_cap) {
            int current_val = 0, max_val = 0;
            CFNumberGetValue(current_cap, kCFNumberIntType, &current_val);
            CFNumberGetValue(max_cap, kCFNumberIntType, &max_val);
            if (max_val > 0) {
                info.capacity_percent = (static_cast<double>(current_val) / max_val) * 100.0;
            }
        }

        // Get time estimates
        CFNumberRef time_empty = static_cast<CFNumberRef>(
            CFDictionaryGetValue(source_dict, CFSTR(kIOPSTimeToEmptyKey)));
        if (time_empty && CFGetTypeID(time_empty) == CFNumberGetTypeID()) {
            int minutes = 0;
            CFNumberGetValue(time_empty, kCFNumberIntType, &minutes);
            if (minutes > 0) {
                info.time_to_empty_minutes = minutes;
            }
        }

        CFNumberRef time_full = static_cast<CFNumberRef>(
            CFDictionaryGetValue(source_dict, CFSTR(kIOPSTimeToFullChargeKey)));
        if (time_full && CFGetTypeID(time_full) == CFNumberGetTypeID()) {
            int minutes = 0;
            CFNumberGetValue(time_full, kCFNumberIntType, &minutes);
            if (minutes > 0) {
                info.time_to_full_minutes = minutes;
            }
        }

        break;  // Only process first battery
    }

    CFRelease(sources);
    CFRelease(info_blob);

    return info;
}

}  // anonymous namespace

// battery_info_collector implementation for macOS

battery_info_collector::battery_info_collector() = default;
battery_info_collector::~battery_info_collector() = default;

bool battery_info_collector::is_battery_available() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (battery_checked_) {
        return battery_available_;
    }

    battery_checked_ = true;

    // Check via IOPowerSources
    auto ps_info = get_power_source_info();
    if (ps_info.available) {
        battery_available_ = true;
        return true;
    }

    // Check via IOKit
    auto iokit_data = get_iokit_battery_data();
    battery_available_ = iokit_data.found;

    return battery_available_;
}

std::vector<battery_info> battery_info_collector::enumerate_batteries() {
    std::lock_guard<std::mutex> lock(mutex_);
    return enumerate_batteries_impl();
}

std::vector<battery_info> battery_info_collector::enumerate_batteries_impl() {
    std::vector<battery_info> batteries;

    // Get battery data from IOKit
    auto iokit_data = get_iokit_battery_data();
    if (iokit_data.found) {
        battery_info info;
        info.id = "InternalBattery-0";
        info.name = iokit_data.name.empty() ? "Internal Battery" : iokit_data.name;
        info.path = "iokit:AppleSmartBattery";
        info.manufacturer = iokit_data.manufacturer;
        info.model = iokit_data.device_name;
        info.serial = iokit_data.serial;
        info.technology = "Li-ion";  // macOS batteries are typically Li-ion

        batteries.push_back(info);
    }

    cached_batteries_ = batteries;
    return batteries;
}

battery_reading battery_info_collector::read_battery(const battery_info& battery) {
    std::lock_guard<std::mutex> lock(mutex_);
    return read_battery_impl(battery);
}

battery_reading battery_info_collector::read_battery_impl(const battery_info& battery) {
    battery_reading reading;
    reading.info = battery;
    reading.timestamp = std::chrono::system_clock::now();

    // Get detailed data from IOKit
    auto iokit_data = get_iokit_battery_data();
    if (!iokit_data.found) {
        return reading;
    }

    reading.battery_present = iokit_data.is_present;
    reading.metrics_available = true;

    // Determine status
    if (iokit_data.is_charged) {
        reading.status = battery_status::full;
    } else if (iokit_data.is_charging) {
        reading.status = battery_status::charging;
    } else if (iokit_data.is_ac_attached) {
        reading.status = battery_status::not_charging;
    } else {
        reading.status = battery_status::discharging;
    }

    reading.is_charging = iokit_data.is_charging;
    reading.ac_connected = iokit_data.is_ac_attached;

    // Calculate level percentage
    if (iokit_data.max_capacity > 0) {
        reading.level_percent = (static_cast<double>(iokit_data.current_capacity) /
                                 iokit_data.max_capacity) * 100.0;
    }

    // Voltage (convert from mV to V)
    if (iokit_data.voltage_mv > 0) {
        reading.voltage_volts = static_cast<double>(iokit_data.voltage_mv) / 1000.0;
    }

    // Current (convert from mA to A)
    int64_t amperage = iokit_data.instantaneous_amperage_ma != 0 ?
                       iokit_data.instantaneous_amperage_ma : iokit_data.amperage_ma;
    if (amperage != 0) {
        reading.current_amps = static_cast<double>(amperage) / 1000.0;
    }

    // Power calculation
    if (reading.voltage_volts > 0.0 && reading.current_amps != 0.0) {
        reading.power_watts = reading.voltage_volts * std::abs(reading.current_amps);
    }

    // Capacity in Wh (macOS reports in mAh, need to convert)
    // Assume nominal voltage for Wh calculation (typically 11.4V for MacBook)
    double nominal_voltage = reading.voltage_volts > 0 ? reading.voltage_volts : 11.4;

    if (iokit_data.current_capacity > 0) {
        reading.current_capacity_wh = (static_cast<double>(iokit_data.current_capacity) / 1000.0) *
                                      nominal_voltage;
    }

    if (iokit_data.max_capacity > 0) {
        reading.full_charge_capacity_wh = (static_cast<double>(iokit_data.max_capacity) / 1000.0) *
                                          nominal_voltage;
    }

    if (iokit_data.design_capacity > 0) {
        reading.design_capacity_wh = (static_cast<double>(iokit_data.design_capacity) / 1000.0) *
                                     nominal_voltage;
    }

    // Health percentage
    if (reading.design_capacity_wh > 0.0) {
        reading.health_percent = (reading.full_charge_capacity_wh / reading.design_capacity_wh) * 100.0;
    }

    // Time estimates (convert from minutes to seconds)
    if (iokit_data.time_to_empty_minutes > 0) {
        reading.time_to_empty_seconds = iokit_data.time_to_empty_minutes * 60;
    }
    if (iokit_data.time_to_full_minutes > 0) {
        reading.time_to_full_seconds = iokit_data.time_to_full_minutes * 60;
    }

    // Cycle count
    reading.cycle_count = iokit_data.cycle_count;

    // Temperature (convert from deciKelvin to Celsius)
    // macOS reports temperature in units of 1/10 Kelvin (deciKelvin)
    // e.g., 3091 = 309.1 K = 35.95°C
    // Valid battery temperatures are typically between 0°C and 60°C (273.15 K to 333.15 K)
    // which corresponds to deciKelvin values of ~2731 to ~3331
    if (iokit_data.temperature_decikelvin > 2500) {  // > 250K (-23°C), sanity check
        double kelvin = static_cast<double>(iokit_data.temperature_decikelvin) / 10.0;
        double celsius = kelvin - 273.15;
        // Only set as available if the temperature is reasonable
        if (celsius > -40.0 && celsius < 100.0) {
            reading.temperature_celsius = celsius;
            reading.temperature_available = true;
        }
    }

    return reading;
}

std::vector<battery_reading> battery_info_collector::read_all_batteries() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<battery_reading> readings;

    // Refresh battery list if empty
    if (cached_batteries_.empty()) {
        enumerate_batteries_impl();
    }

    for (const auto& battery : cached_batteries_) {
        readings.push_back(read_battery_impl(battery));
    }

    return readings;
}

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(__APPLE__)
