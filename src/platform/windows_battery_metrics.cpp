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

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Wbemidl.h>
#include <comdef.h>
#include <windows.h>

#include <algorithm>
#include <string>
#include <vector>

#pragma comment(lib, "wbemuuid.lib")

namespace kcenon {
namespace monitoring {

namespace {

/**
 * Convert wide string to narrow string
 */
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

/**
 * WMI connection manager for battery queries
 */
class wmi_battery_connection {
   public:
    wmi_battery_connection() {
        // Initialize COM
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            return;
        }
        com_initialized_ = true;

        // Set security
        hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
                                  RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

        if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
            return;
        }

        // Create WMI locator
        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                              reinterpret_cast<LPVOID*>(&locator_));

        if (FAILED(hr)) {
            return;
        }

        // Connect to WMI - use root\CIMV2 for battery
        hr = locator_->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr,
                                     nullptr, &services_);

        if (FAILED(hr)) {
            locator_->Release();
            locator_ = nullptr;
            return;
        }

        // Set proxy security
        hr = CoSetProxyBlanket(services_, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                               RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr,
                               EOAC_NONE);

        valid_ = SUCCEEDED(hr);
    }

    ~wmi_battery_connection() {
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

    struct wmi_battery_data {
        std::string device_id;
        std::string name;
        std::string manufacturer;
        std::string chemistry;

        uint16_t battery_percent{0};
        uint16_t battery_status{0};
        uint32_t design_capacity{0};
        uint32_t full_charge_capacity{0};
        uint32_t design_voltage{0};
        int32_t estimated_runtime{-1};  // minutes
        int32_t estimated_charge_time{-1};  // minutes

        bool found{false};
    };

    std::vector<wmi_battery_data> query_batteries() {
        std::vector<wmi_battery_data> batteries;

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

            wmi_battery_data bat;
            bat.found = true;

            // Get DeviceID
            VARIANT var_val;
            VariantInit(&var_val);
            hr = obj->Get(L"DeviceID", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_BSTR) {
                bat.device_id = wide_to_narrow(var_val.bstrVal);
            }
            VariantClear(&var_val);

            // Get Name
            VariantInit(&var_val);
            hr = obj->Get(L"Name", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_BSTR) {
                bat.name = wide_to_narrow(var_val.bstrVal);
            }
            VariantClear(&var_val);

            // Get Manufacturer (may not be available)
            VariantInit(&var_val);
            hr = obj->Get(L"Manufacturer", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_BSTR) {
                bat.manufacturer = wide_to_narrow(var_val.bstrVal);
            }
            VariantClear(&var_val);

            // Get Chemistry
            VariantInit(&var_val);
            hr = obj->Get(L"Chemistry", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_I4) {
                switch (var_val.lVal) {
                    case 1:
                        bat.chemistry = "Other";
                        break;
                    case 2:
                        bat.chemistry = "Unknown";
                        break;
                    case 3:
                        bat.chemistry = "Lead Acid";
                        break;
                    case 4:
                        bat.chemistry = "NiCd";
                        break;
                    case 5:
                        bat.chemistry = "NiMH";
                        break;
                    case 6:
                        bat.chemistry = "Li-ion";
                        break;
                    case 7:
                        bat.chemistry = "Zinc-Air";
                        break;
                    case 8:
                        bat.chemistry = "Li-polymer";
                        break;
                    default:
                        bat.chemistry = "";
                        break;
                }
            }
            VariantClear(&var_val);

            // Get EstimatedChargeRemaining (percentage)
            VariantInit(&var_val);
            hr = obj->Get(L"EstimatedChargeRemaining", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_I4) {
                bat.battery_percent = static_cast<uint16_t>(var_val.lVal);
            }
            VariantClear(&var_val);

            // Get BatteryStatus
            // 1=Discharging, 2=AC, 3=Fully Charged, 4=Low, 5=Critical,
            // 6=Charging, 7=Charging High, 8=Charging Low, 9=Charging Critical
            VariantInit(&var_val);
            hr = obj->Get(L"BatteryStatus", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_I4) {
                bat.battery_status = static_cast<uint16_t>(var_val.lVal);
            }
            VariantClear(&var_val);

            // Get DesignCapacity
            VariantInit(&var_val);
            hr = obj->Get(L"DesignCapacity", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_I4) {
                bat.design_capacity = static_cast<uint32_t>(var_val.lVal);
            }
            VariantClear(&var_val);

            // Get FullChargeCapacity
            VariantInit(&var_val);
            hr = obj->Get(L"FullChargeCapacity", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_I4) {
                bat.full_charge_capacity = static_cast<uint32_t>(var_val.lVal);
            }
            VariantClear(&var_val);

            // Get DesignVoltage
            VariantInit(&var_val);
            hr = obj->Get(L"DesignVoltage", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_I4) {
                bat.design_voltage = static_cast<uint32_t>(var_val.lVal);
            }
            VariantClear(&var_val);

            // Get EstimatedRunTime (minutes, -1 = unlimited/charging)
            VariantInit(&var_val);
            hr = obj->Get(L"EstimatedRunTime", 0, &var_val, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_val.vt == VT_I4) {
                bat.estimated_runtime = var_val.lVal;
            }
            VariantClear(&var_val);

            // Get TimeToFullCharge (minutes)
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

// Global WMI connection (lazy-initialized)
static std::unique_ptr<wmi_battery_connection> g_wmi;
static std::mutex g_wmi_mutex;

wmi_battery_connection* get_wmi() {
    std::lock_guard<std::mutex> lock(g_wmi_mutex);
    if (!g_wmi) {
        g_wmi = std::make_unique<wmi_battery_connection>();
    }
    return g_wmi.get();
}

}  // anonymous namespace

// battery_info_collector implementation for Windows

battery_info_collector::battery_info_collector() = default;
battery_info_collector::~battery_info_collector() = default;

bool battery_info_collector::is_battery_available() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (battery_checked_) {
        return battery_available_;
    }

    battery_checked_ = true;

    // Check via GetSystemPowerStatus first (fast check)
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        // BatteryFlag 128 = no battery present
        if (sps.BatteryFlag != 128) {
            battery_available_ = true;
            return true;
        }
    }

    // Check via WMI
    auto* wmi = get_wmi();
    if (wmi && wmi->is_valid()) {
        auto batteries = wmi->query_batteries();
        battery_available_ = !batteries.empty();
    }

    return battery_available_;
}

std::vector<battery_info> battery_info_collector::enumerate_batteries() {
    std::lock_guard<std::mutex> lock(mutex_);
    return enumerate_batteries_impl();
}

std::vector<battery_info> battery_info_collector::enumerate_batteries_impl() {
    std::vector<battery_info> batteries;

    // Query via WMI for detailed info
    auto* wmi = get_wmi();
    if (wmi && wmi->is_valid()) {
        auto wmi_batteries = wmi->query_batteries();
        for (size_t i = 0; i < wmi_batteries.size(); ++i) {
            const auto& wmi_bat = wmi_batteries[i];

            battery_info info;
            info.id = wmi_bat.device_id.empty() ?
                      ("BAT" + std::to_string(i)) : wmi_bat.device_id;
            info.name = wmi_bat.name.empty() ?
                        ("Battery " + std::to_string(i)) : wmi_bat.name;
            info.path = "wmi:battery:" + std::to_string(i);
            info.manufacturer = wmi_bat.manufacturer;
            info.model = wmi_bat.name;
            info.technology = wmi_bat.chemistry;

            batteries.push_back(info);
        }
    }

    // Fallback: check via GetSystemPowerStatus if no WMI results
    if (batteries.empty()) {
        SYSTEM_POWER_STATUS sps;
        if (GetSystemPowerStatus(&sps) && sps.BatteryFlag != 128) {
            battery_info info;
            info.id = "BAT0";
            info.name = "System Battery";
            info.path = "system:battery:0";
            batteries.push_back(info);
        }
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

    // Check if using system path (fallback)
    bool use_system_api = (battery.path.find("system:") == 0);

    // Extract battery index from WMI path
    size_t battery_index = 0;
    if (!use_system_api) {
        size_t pos = battery.path.find_last_of(':');
        if (pos != std::string::npos && pos + 1 < battery.path.size()) {
            try {
                battery_index = std::stoul(battery.path.substr(pos + 1));
            } catch (...) {
                // Keep battery_index = 0
            }
        }
    }

    // Get data from WMI
    auto* wmi = get_wmi();
    if (wmi && wmi->is_valid() && !use_system_api) {
        auto batteries = wmi->query_batteries();
        if (battery_index < batteries.size()) {
            const auto& wmi_bat = batteries[battery_index];

            reading.battery_present = true;
            reading.metrics_available = true;
            reading.level_percent = static_cast<double>(wmi_bat.battery_percent);

            // Determine status from BatteryStatus
            switch (wmi_bat.battery_status) {
                case 1:  // Discharging
                    reading.status = battery_status::discharging;
                    reading.is_charging = false;
                    reading.ac_connected = false;
                    break;
                case 2:  // AC (not charging, not discharging)
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
        }
    }

    // Supplement with GetSystemPowerStatus (always available)
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps) && sps.BatteryFlag != 128) {
        if (!reading.battery_present) {
            reading.battery_present = true;
            reading.metrics_available = true;
        }

        // Update level if WMI didn't provide it
        if (reading.level_percent == 0.0 && sps.BatteryLifePercent != 255) {
            reading.level_percent = static_cast<double>(sps.BatteryLifePercent);
        }

        // AC status
        reading.ac_connected = (sps.ACLineStatus == 1);

        // Charging status
        bool is_charging_flag = (sps.BatteryFlag & 8) != 0;
        if (is_charging_flag) {
            reading.is_charging = true;
            reading.status = battery_status::charging;
        }

        // Time to empty from system (if not from WMI)
        if (reading.time_to_empty_seconds < 0 && sps.BatteryLifeTime != 0xFFFFFFFF) {
            reading.time_to_empty_seconds = static_cast<int64_t>(sps.BatteryLifeTime);
        }

        // Time to full from system (if not from WMI)
        if (reading.time_to_full_seconds < 0 && sps.BatteryFullLifeTime != 0xFFFFFFFF) {
            // This is actually "full battery life time", not time to full
            // Windows doesn't directly provide time to full via GetSystemPowerStatus
        }

        // Determine final status if not set
        if (reading.status == battery_status::unknown) {
            if (is_charging_flag) {
                reading.status = battery_status::charging;
            } else if (sps.BatteryLifePercent >= 95 && sps.ACLineStatus == 1) {
                reading.status = battery_status::full;
            } else if (sps.ACLineStatus == 0) {
                reading.status = battery_status::discharging;
            } else {
                reading.status = battery_status::not_charging;
            }
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

#endif  // defined(_WIN32)
