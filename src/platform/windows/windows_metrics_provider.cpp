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

#include "windows_metrics_provider.h"

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// winsock2.h must be included before windows.h
#include <winsock2.h>
#include <windows.h>
#include <Wbemidl.h>
#include <comdef.h>
#include <iphlpapi.h>
#include <tcpmib.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace kcenon {
namespace monitoring {
namespace platform {

namespace {

// =========================================================================
// Utility Functions
// =========================================================================

std::string wide_to_narrow(const std::wstring& wide_str) {
    if (wide_str.empty()) {
        return std::string();
    }

    int size_needed =
        WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), static_cast<int>(wide_str.length()),
                            nullptr, 0, nullptr, nullptr);
    std::string str_to(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), static_cast<int>(wide_str.length()),
                        &str_to[0], size_needed, nullptr, nullptr);
    return str_to;
}

// =========================================================================
// WMI Connection for Battery/Power
// =========================================================================

class wmi_cimv2_connection {
   public:
    wmi_cimv2_connection() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            return;
        }
        com_initialized_ = true;

        hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
                                  RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

        if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
            // Continue anyway, security may already be initialized
        }

        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                              reinterpret_cast<LPVOID*>(&locator_));

        if (FAILED(hr)) {
            return;
        }

        hr = locator_->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr,
                                     nullptr, &services_);

        if (FAILED(hr)) {
            locator_->Release();
            locator_ = nullptr;
            return;
        }

        hr = CoSetProxyBlanket(services_, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                               RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr,
                               EOAC_NONE);

        valid_ = SUCCEEDED(hr);
    }

    ~wmi_cimv2_connection() {
        if (services_) {
            services_->Release();
        }
        if (locator_) {
            locator_->Release();
        }
        if (com_initialized_) {
            CoUninitialize();
        }
    }

    bool is_valid() const { return valid_; }

    struct battery_data {
        std::string device_id;
        std::string name;
        std::string manufacturer;
        std::string chemistry;
        uint16_t battery_percent{0};
        uint16_t battery_status{0};
        uint32_t design_capacity{0};
        uint32_t full_charge_capacity{0};
        uint32_t design_voltage{0};
        int32_t estimated_runtime{-1};
        int32_t estimated_charge_time{-1};
        bool found{false};
    };

    std::vector<battery_data> query_batteries() {
        std::vector<battery_data> batteries;

        if (!valid_ || !services_) {
            return batteries;
        }

        IEnumWbemClassObject* enumerator = nullptr;
        HRESULT hr = services_->ExecQuery(
            _bstr_t(L"WQL"), _bstr_t(L"SELECT * FROM Win32_Battery"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);

        if (FAILED(hr)) {
            return batteries;
        }

        IWbemClassObject* obj = nullptr;
        ULONG returned = 0;

        while (enumerator) {
            hr = enumerator->Next(WBEM_INFINITE, 1, &obj, &returned);
            if (hr == WBEM_S_FALSE || returned == 0) {
                break;
            }

            battery_data bat;
            bat.found = true;

            VARIANT var_val;

            // DeviceID
            VariantInit(&var_val);
            hr = obj->Get(L"DeviceID", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_BSTR) {
                bat.device_id = wide_to_narrow(var_val.bstrVal);
            }
            VariantClear(&var_val);

            // Name
            VariantInit(&var_val);
            hr = obj->Get(L"Name", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_BSTR) {
                bat.name = wide_to_narrow(var_val.bstrVal);
            }
            VariantClear(&var_val);

            // Manufacturer
            VariantInit(&var_val);
            hr = obj->Get(L"Manufacturer", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_BSTR) {
                bat.manufacturer = wide_to_narrow(var_val.bstrVal);
            }
            VariantClear(&var_val);

            // Chemistry
            VariantInit(&var_val);
            hr = obj->Get(L"Chemistry", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_I4) {
                switch (var_val.lVal) {
                    case 1: bat.chemistry = "Other"; break;
                    case 2: bat.chemistry = "Unknown"; break;
                    case 3: bat.chemistry = "Lead Acid"; break;
                    case 4: bat.chemistry = "NiCd"; break;
                    case 5: bat.chemistry = "NiMH"; break;
                    case 6: bat.chemistry = "Li-ion"; break;
                    case 7: bat.chemistry = "Zinc-Air"; break;
                    case 8: bat.chemistry = "Li-polymer"; break;
                    default: bat.chemistry = ""; break;
                }
            }
            VariantClear(&var_val);

            // EstimatedChargeRemaining
            VariantInit(&var_val);
            hr = obj->Get(L"EstimatedChargeRemaining", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_I4) {
                bat.battery_percent = static_cast<uint16_t>(var_val.lVal);
            }
            VariantClear(&var_val);

            // BatteryStatus
            VariantInit(&var_val);
            hr = obj->Get(L"BatteryStatus", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_I4) {
                bat.battery_status = static_cast<uint16_t>(var_val.lVal);
            }
            VariantClear(&var_val);

            // DesignCapacity
            VariantInit(&var_val);
            hr = obj->Get(L"DesignCapacity", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_I4) {
                bat.design_capacity = static_cast<uint32_t>(var_val.lVal);
            }
            VariantClear(&var_val);

            // FullChargeCapacity
            VariantInit(&var_val);
            hr = obj->Get(L"FullChargeCapacity", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_I4) {
                bat.full_charge_capacity = static_cast<uint32_t>(var_val.lVal);
            }
            VariantClear(&var_val);

            // DesignVoltage
            VariantInit(&var_val);
            hr = obj->Get(L"DesignVoltage", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_I4) {
                bat.design_voltage = static_cast<uint32_t>(var_val.lVal);
            }
            VariantClear(&var_val);

            // EstimatedRunTime
            VariantInit(&var_val);
            hr = obj->Get(L"EstimatedRunTime", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_I4) {
                bat.estimated_runtime = var_val.lVal;
            }
            VariantClear(&var_val);

            // TimeToFullCharge
            VariantInit(&var_val);
            hr = obj->Get(L"TimeToFullCharge", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_I4) {
                bat.estimated_charge_time = var_val.lVal;
            }
            VariantClear(&var_val);

            batteries.push_back(bat);
            obj->Release();
        }

        if (enumerator) {
            enumerator->Release();
        }

        return batteries;
    }

   private:
    bool com_initialized_{false};
    bool valid_{false};
    IWbemLocator* locator_{nullptr};
    IWbemServices* services_{nullptr};
};

// =========================================================================
// WMI Connection for Temperature (ROOT\WMI)
// =========================================================================

class wmi_root_connection {
   public:
    wmi_root_connection() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            return;
        }
        com_initialized_ = true;

        hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
                                  RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

        if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
            // Continue anyway
        }

        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                              reinterpret_cast<LPVOID*>(&locator_));

        if (FAILED(hr)) {
            return;
        }

        hr = locator_->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, nullptr, 0, nullptr,
                                     nullptr, &services_);

        if (FAILED(hr)) {
            locator_->Release();
            locator_ = nullptr;
            return;
        }

        hr = CoSetProxyBlanket(services_, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                               RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr,
                               EOAC_NONE);

        valid_ = SUCCEEDED(hr);
    }

    ~wmi_root_connection() {
        if (services_) {
            services_->Release();
        }
        if (locator_) {
            locator_->Release();
        }
        if (com_initialized_) {
            CoUninitialize();
        }
    }

    bool is_valid() const { return valid_; }

    struct thermal_zone_data {
        std::string instance_name;
        double current_temperature{0.0};
        double critical_temperature{0.0};
        bool active{false};
    };

    std::vector<thermal_zone_data> query_thermal_zones() {
        std::vector<thermal_zone_data> zones;

        if (!valid_ || !services_) {
            return zones;
        }

        IEnumWbemClassObject* enumerator = nullptr;
        HRESULT hr = services_->ExecQuery(
            _bstr_t(L"WQL"), _bstr_t(L"SELECT * FROM MSAcpi_ThermalZoneTemperature"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);

        if (FAILED(hr)) {
            return zones;
        }

        IWbemClassObject* obj = nullptr;
        ULONG returned = 0;

        while (enumerator) {
            hr = enumerator->Next(WBEM_INFINITE, 1, &obj, &returned);
            if (hr == WBEM_S_FALSE || returned == 0) {
                break;
            }

            thermal_zone_data zone;
            VARIANT var;

            // InstanceName
            VariantInit(&var);
            hr = obj->Get(L"InstanceName", 0, &var, nullptr, nullptr);
            if (SUCCEEDED(hr) && var.vt == VT_BSTR) {
                zone.instance_name = wide_to_narrow(var.bstrVal);
            }
            VariantClear(&var);

            // CurrentTemperature (tenths of Kelvin)
            VariantInit(&var);
            hr = obj->Get(L"CurrentTemperature", 0, &var, nullptr, nullptr);
            if (SUCCEEDED(hr) && var.vt == VT_I4) {
                zone.current_temperature = (static_cast<double>(var.lVal) / 10.0) - 273.15;
            }
            VariantClear(&var);

            // CriticalTripPoint
            VariantInit(&var);
            hr = obj->Get(L"CriticalTripPoint", 0, &var, nullptr, nullptr);
            if (SUCCEEDED(hr) && var.vt == VT_I4) {
                zone.critical_temperature = (static_cast<double>(var.lVal) / 10.0) - 273.15;
            }
            VariantClear(&var);

            // Active
            VariantInit(&var);
            hr = obj->Get(L"Active", 0, &var, nullptr, nullptr);
            if (SUCCEEDED(hr) && var.vt == VT_BOOL) {
                zone.active = (var.boolVal != VARIANT_FALSE);
            }
            VariantClear(&var);

            zones.push_back(zone);
            obj->Release();
        }

        if (enumerator) {
            enumerator->Release();
        }

        return zones;
    }

   private:
    bool com_initialized_{false};
    bool valid_{false};
    IWbemLocator* locator_{nullptr};
    IWbemServices* services_{nullptr};
};

// Global WMI connections
static std::unique_ptr<wmi_cimv2_connection> g_wmi_cimv2;
static std::unique_ptr<wmi_root_connection> g_wmi_root;
static std::mutex g_wmi_mutex;

wmi_cimv2_connection* get_wmi_cimv2() {
    std::lock_guard<std::mutex> lock(g_wmi_mutex);
    if (!g_wmi_cimv2) {
        g_wmi_cimv2 = std::make_unique<wmi_cimv2_connection>();
    }
    return g_wmi_cimv2.get();
}

wmi_root_connection* get_wmi_root() {
    std::lock_guard<std::mutex> lock(g_wmi_mutex);
    if (!g_wmi_root) {
        g_wmi_root = std::make_unique<wmi_root_connection>();
    }
    return g_wmi_root.get();
}

}  // anonymous namespace

// =========================================================================
// windows_metrics_provider Implementation
// =========================================================================

windows_metrics_provider::windows_metrics_provider() = default;

// =========================================================================
// Battery
// =========================================================================

bool windows_metrics_provider::is_battery_available() const {
    if (!battery_checked_) {
        SYSTEM_POWER_STATUS sps;
        if (GetSystemPowerStatus(&sps)) {
            if (sps.BatteryFlag != 128) {
                battery_available_ = true;
                battery_checked_ = true;
                return true;
            }
        }

        auto* wmi = get_wmi_cimv2();
        if (wmi && wmi->is_valid()) {
            auto batteries = wmi->query_batteries();
            battery_available_ = !batteries.empty();
        }
        battery_checked_ = true;
    }
    return battery_available_;
}

std::vector<battery_reading> windows_metrics_provider::get_battery_readings() {
    std::vector<battery_reading> readings;

    auto* wmi = get_wmi_cimv2();
    std::vector<wmi_cimv2_connection::battery_data> wmi_batteries;
    if (wmi && wmi->is_valid()) {
        wmi_batteries = wmi->query_batteries();
    }

    SYSTEM_POWER_STATUS sps;
    bool has_system_status = GetSystemPowerStatus(&sps) && sps.BatteryFlag != 128;

    if (wmi_batteries.empty() && !has_system_status) {
        return readings;
    }

    if (!wmi_batteries.empty()) {
        for (size_t i = 0; i < wmi_batteries.size(); ++i) {
            const auto& wmi_bat = wmi_batteries[i];

            battery_reading reading;
            reading.timestamp = std::chrono::system_clock::now();
            reading.battery_present = true;
            reading.metrics_available = true;

            reading.info.id = wmi_bat.device_id.empty() ?
                              ("BAT" + std::to_string(i)) : wmi_bat.device_id;
            reading.info.name = wmi_bat.name.empty() ?
                                ("Battery " + std::to_string(i)) : wmi_bat.name;
            reading.info.path = "wmi:battery:" + std::to_string(i);
            reading.info.manufacturer = wmi_bat.manufacturer;
            reading.info.model = wmi_bat.name;
            reading.info.technology = wmi_bat.chemistry;

            reading.level_percent = static_cast<double>(wmi_bat.battery_percent);

            // Determine status from BatteryStatus
            switch (wmi_bat.battery_status) {
                case 1:  // Discharging
                    reading.status = battery_status::discharging;
                    reading.is_charging = false;
                    reading.ac_connected = false;
                    break;
                case 2:  // AC
                    reading.status = battery_status::not_charging;
                    reading.is_charging = false;
                    reading.ac_connected = true;
                    break;
                case 3:  // Fully Charged
                    reading.status = battery_status::full;
                    reading.is_charging = false;
                    reading.ac_connected = true;
                    break;
                case 6:  // Charging
                case 7:  // Charging High
                case 8:  // Charging Low
                case 9:  // Charging Critical
                    reading.status = battery_status::charging;
                    reading.is_charging = true;
                    reading.ac_connected = true;
                    break;
                default:
                    reading.status = battery_status::unknown;
                    break;
            }

            // Capacity in mWh
            if (wmi_bat.design_capacity > 0) {
                reading.design_capacity_wh = static_cast<double>(wmi_bat.design_capacity) / 1000.0;
            }
            if (wmi_bat.full_charge_capacity > 0) {
                reading.full_charge_capacity_wh = static_cast<double>(wmi_bat.full_charge_capacity) / 1000.0;
            }
            if (reading.full_charge_capacity_wh > 0.0 && reading.level_percent > 0.0) {
                reading.current_capacity_wh = reading.full_charge_capacity_wh * (reading.level_percent / 100.0);
            }

            // Health percentage
            if (reading.design_capacity_wh > 0.0) {
                reading.health_percent = (reading.full_charge_capacity_wh / reading.design_capacity_wh) * 100.0;
            }

            // Voltage (design voltage in mV)
            if (wmi_bat.design_voltage > 0) {
                reading.voltage_volts = static_cast<double>(wmi_bat.design_voltage) / 1000.0;
            }

            // Time estimates (convert minutes to seconds)
            if (wmi_bat.estimated_runtime > 0) {
                reading.time_to_empty_seconds = static_cast<int64_t>(wmi_bat.estimated_runtime) * 60;
            }
            if (wmi_bat.estimated_charge_time > 0) {
                reading.time_to_full_seconds = static_cast<int64_t>(wmi_bat.estimated_charge_time) * 60;
            }

            readings.push_back(reading);
        }
    } else if (has_system_status) {
        // Fallback to system status only
        battery_reading reading;
        reading.timestamp = std::chrono::system_clock::now();
        reading.battery_present = true;
        reading.metrics_available = true;

        reading.info.id = "BAT0";
        reading.info.name = "System Battery";
        reading.info.path = "system:battery:0";

        if (sps.BatteryLifePercent != 255) {
            reading.level_percent = static_cast<double>(sps.BatteryLifePercent);
        }

        reading.ac_connected = (sps.ACLineStatus == 1);
        bool is_charging_flag = (sps.BatteryFlag & 8) != 0;

        if (is_charging_flag) {
            reading.is_charging = true;
            reading.status = battery_status::charging;
        } else if (sps.BatteryLifePercent >= 95 && sps.ACLineStatus == 1) {
            reading.status = battery_status::full;
        } else if (sps.ACLineStatus == 0) {
            reading.status = battery_status::discharging;
        } else {
            reading.status = battery_status::not_charging;
        }

        if (sps.BatteryLifeTime != 0xFFFFFFFF) {
            reading.time_to_empty_seconds = static_cast<int64_t>(sps.BatteryLifeTime);
        }

        readings.push_back(reading);
    }

    return readings;
}

// =========================================================================
// Temperature
// =========================================================================

bool windows_metrics_provider::is_temperature_available() const {
    if (!temperature_checked_) {
        auto* wmi = get_wmi_root();
        temperature_available_ = (wmi != nullptr && wmi->is_valid());
        temperature_checked_ = true;
    }
    return temperature_available_;
}

std::vector<temperature_reading> windows_metrics_provider::get_temperature_readings() {
    std::vector<temperature_reading> readings;

    auto* wmi = get_wmi_root();
    if (!wmi || !wmi->is_valid()) {
        return readings;
    }

    auto zones = wmi->query_thermal_zones();

    for (size_t i = 0; i < zones.size(); ++i) {
        const auto& zone = zones[i];

        temperature_reading reading;
        reading.timestamp = std::chrono::system_clock::now();
        reading.sensor.id = "thermal_zone_" + std::to_string(i);
        reading.sensor.name = zone.instance_name.empty() ?
                              ("Thermal Zone " + std::to_string(i)) : zone.instance_name;
        reading.sensor.zone_path = reading.sensor.id;

        // Classify based on instance name
        std::string lower_name = zone.instance_name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (lower_name.find("cpu") != std::string::npos ||
            lower_name.find("proc") != std::string::npos) {
            reading.sensor.type = sensor_type::cpu;
        } else if (lower_name.find("gpu") != std::string::npos ||
                   lower_name.find("video") != std::string::npos) {
            reading.sensor.type = sensor_type::gpu;
        } else {
            reading.sensor.type = sensor_type::motherboard;
        }

        reading.temperature_celsius = zone.current_temperature;

        if (zone.critical_temperature > 0.0) {
            reading.thresholds_available = true;
            reading.critical_threshold_celsius = zone.critical_temperature;
            reading.warning_threshold_celsius = zone.critical_temperature - 10.0;

            if (reading.temperature_celsius >= reading.critical_threshold_celsius) {
                reading.is_critical = true;
            }
            if (reading.temperature_celsius >= reading.warning_threshold_celsius) {
                reading.is_warning = true;
            }
        }

        readings.push_back(reading);
    }

    return readings;
}

// =========================================================================
// Uptime
// =========================================================================

uptime_info windows_metrics_provider::get_uptime() {
    uptime_info info;

    ULONGLONG uptime_ms = GetTickCount64();
    info.uptime_seconds = static_cast<int64_t>(uptime_ms / 1000);

    auto now = std::chrono::system_clock::now();
    auto uptime_duration = std::chrono::seconds(info.uptime_seconds);
    info.boot_time = now - uptime_duration;
    info.available = true;

    return info;
}

// =========================================================================
// Context Switches
// =========================================================================

context_switch_info windows_metrics_provider::get_context_switches() {
    context_switch_info info;
    info.timestamp = std::chrono::system_clock::now();
    // Context switch monitoring not yet implemented on Windows
    // Future: Use Performance Counters
    info.available = false;
    return info;
}

// =========================================================================
// File Descriptors (Handles)
// =========================================================================

fd_info windows_metrics_provider::get_fd_stats() {
    fd_info info;

    DWORD handle_count = 0;
    if (GetProcessHandleCount(GetCurrentProcess(), &handle_count)) {
        info.open_fds = static_cast<uint64_t>(handle_count);
        // Default per-process handle limit on modern Windows
        info.max_fds = 16777216;

        if (info.max_fds > 0) {
            info.usage_percent = 100.0 * static_cast<double>(info.open_fds) /
                                 static_cast<double>(info.max_fds);
        }
        info.available = true;
    }

    return info;
}

// =========================================================================
// Inodes
// =========================================================================

std::vector<inode_info> windows_metrics_provider::get_inode_stats() {
    // Not applicable on Windows - NTFS uses MFT instead of inodes
    return {};
}

// =========================================================================
// TCP States
// =========================================================================

tcp_state_info windows_metrics_provider::get_tcp_states() {
    tcp_state_info info;

    // Get TCP table size
    DWORD size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);

    if (size == 0) {
        return info;
    }

    std::vector<char> buffer(size);
    PMIB_TCPTABLE_OWNER_PID tcp_table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());

    if (GetExtendedTcpTable(tcp_table, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
        for (DWORD i = 0; i < tcp_table->dwNumEntries; ++i) {
            switch (tcp_table->table[i].dwState) {
                case MIB_TCP_STATE_CLOSED:
                    break;
                case MIB_TCP_STATE_LISTEN:
                    info.listen++;
                    break;
                case MIB_TCP_STATE_SYN_SENT:
                    info.syn_sent++;
                    break;
                case MIB_TCP_STATE_SYN_RCVD:
                    info.syn_recv++;
                    break;
                case MIB_TCP_STATE_ESTAB:
                    info.established++;
                    break;
                case MIB_TCP_STATE_FIN_WAIT1:
                    info.fin_wait1++;
                    break;
                case MIB_TCP_STATE_FIN_WAIT2:
                    info.fin_wait2++;
                    break;
                case MIB_TCP_STATE_CLOSE_WAIT:
                    info.close_wait++;
                    break;
                case MIB_TCP_STATE_CLOSING:
                    info.closing++;
                    break;
                case MIB_TCP_STATE_LAST_ACK:
                    info.last_ack++;
                    break;
                case MIB_TCP_STATE_TIME_WAIT:
                    info.time_wait++;
                    break;
                default:
                    break;
            }
        }

        info.total = info.established + info.syn_sent + info.syn_recv +
                     info.fin_wait1 + info.fin_wait2 + info.time_wait +
                     info.close_wait + info.last_ack + info.listen + info.closing;
        info.available = true;
    }

    return info;
}

// =========================================================================
// Socket Buffers
// =========================================================================

socket_buffer_info windows_metrics_provider::get_socket_buffer_stats() {
    socket_buffer_info info;
    // Socket buffer stats not easily available on Windows
    info.available = false;
    return info;
}

// =========================================================================
// Interrupts
// =========================================================================

std::vector<interrupt_info> windows_metrics_provider::get_interrupt_stats() {
    // Interrupt monitoring not implemented on Windows
    // Future: Use Performance Counters (PDH API)
    return {};
}

// =========================================================================
// Power
// =========================================================================

bool windows_metrics_provider::is_power_available() const {
    if (!power_checked_) {
        SYSTEM_POWER_STATUS sps;
        power_available_ = (GetSystemPowerStatus(&sps) != 0);
        power_checked_ = true;
    }
    return power_available_;
}

power_info windows_metrics_provider::get_power_info() {
    power_info info;

    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps)) {
        return info;
    }

    info.available = true;
    info.source = (sps.ACLineStatus == 1) ? "ac" : "battery";

    // Get more details from WMI if available
    auto* wmi = get_wmi_cimv2();
    if (wmi && wmi->is_valid()) {
        auto batteries = wmi->query_batteries();
        if (!batteries.empty()) {
            const auto& bat = batteries[0];
            if (bat.design_voltage > 0) {
                info.voltage_volts = static_cast<double>(bat.design_voltage) / 1000.0;
            }
        }
    }

    return info;
}

// =========================================================================
// GPU
// =========================================================================

bool windows_metrics_provider::is_gpu_available() const {
    if (!gpu_checked_) {
        // GPU monitoring not yet implemented
        // Future: Use DXGI or WMI Win32_VideoController
        gpu_available_ = false;
        gpu_checked_ = true;
    }
    return gpu_available_;
}

std::vector<gpu_info> windows_metrics_provider::get_gpu_info() {
    // GPU monitoring not implemented on Windows
    // Future: Use DXGI, NVML, ADL
    return {};
}

// =========================================================================
// Security
// =========================================================================

security_info windows_metrics_provider::get_security_info() {
    security_info info;
    // Security monitoring not yet implemented on Windows
    // Future: Use Windows Security Center API
    info.available = false;
    return info;
}

}  // namespace platform
}  // namespace monitoring
}  // namespace kcenon

#endif  // _WIN32
