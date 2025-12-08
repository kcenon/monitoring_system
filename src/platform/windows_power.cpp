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
 * WMI connection manager for power/battery queries
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

    struct battery_status {
        std::string device_id;
        uint16_t battery_percent;
        bool is_charging;
        bool is_ac_online;
        uint32_t estimated_runtime;  // seconds
        uint32_t voltage;            // millivolts
    };

    std::vector<battery_status> query_batteries() {
        std::vector<battery_status> batteries;

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

            battery_status bat;

            // Get DeviceID
            VARIANT var_id;
            VariantInit(&var_id);
            hr = obj->Get(L"DeviceID", 0, &var_id, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_id.vt == VT_BSTR) {
                bat.device_id = wide_to_narrow(var_id.bstrVal);
            }
            VariantClear(&var_id);

            // Get EstimatedChargeRemaining (percentage)
            VARIANT var_charge;
            VariantInit(&var_charge);
            hr = obj->Get(L"EstimatedChargeRemaining", 0, &var_charge, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_charge.vt == VT_I4) {
                bat.battery_percent = static_cast<uint16_t>(var_charge.lVal);
            }
            VariantClear(&var_charge);

            // Get BatteryStatus
            // 1 = discharging, 2 = AC, 3 = fully charged, etc.
            VARIANT var_status;
            VariantInit(&var_status);
            hr = obj->Get(L"BatteryStatus", 0, &var_status, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_status.vt == VT_I4) {
                bat.is_charging = (var_status.lVal == 2 || var_status.lVal == 6 ||
                                   var_status.lVal == 7 || var_status.lVal == 8);
                bat.is_ac_online = (var_status.lVal != 1);  // 1 = discharging (no AC)
            }
            VariantClear(&var_status);

            // Get EstimatedRunTime (minutes, -1 = unlimited/charging)
            VARIANT var_runtime;
            VariantInit(&var_runtime);
            hr = obj->Get(L"EstimatedRunTime", 0, &var_runtime, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_runtime.vt == VT_I4 && var_runtime.lVal > 0) {
                bat.estimated_runtime = static_cast<uint32_t>(var_runtime.lVal) * 60;  // Convert to seconds
            }
            VariantClear(&var_runtime);

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

// power_info_collector implementation for Windows

power_info_collector::power_info_collector()
    : last_reading_time_(std::chrono::steady_clock::now()) {}

power_info_collector::~power_info_collector() = default;

bool power_info_collector::is_power_available() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (power_checked_) {
        return power_available_;
    }

    power_checked_ = true;

    // Check if we can get battery info via WMI
    auto* wmi = get_wmi();
    if (wmi && wmi->is_valid()) {
        auto batteries = wmi->query_batteries();
        power_available_ = !batteries.empty();
    }

    // Also check system power status
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        power_available_ = true;
    }

    return power_available_;
}

std::vector<power_source_info> power_info_collector::enumerate_sources() {
    std::lock_guard<std::mutex> lock(mutex_);
    return enumerate_sources_impl();
}

std::vector<power_source_info> power_info_collector::enumerate_sources_impl() {
    std::vector<power_source_info> sources;

    // Add AC adapter source (always present for detection)
    power_source_info ac_source;
    ac_source.id = "ac_adapter";
    ac_source.name = "AC Adapter";
    ac_source.path = "system:ac";
    ac_source.type = power_source_type::ac;
    sources.push_back(ac_source);

    // Query batteries via WMI
    auto* wmi = get_wmi();
    if (wmi && wmi->is_valid()) {
        auto batteries = wmi->query_batteries();
        for (size_t i = 0; i < batteries.size(); ++i) {
            power_source_info battery_source;
            battery_source.id = "battery_" + std::to_string(i);
            battery_source.name = batteries[i].device_id.empty() 
                ? ("Battery " + std::to_string(i))
                : batteries[i].device_id;
            battery_source.path = "wmi:battery:" + std::to_string(i);
            battery_source.type = power_source_type::battery;
            sources.push_back(battery_source);
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

    // Handle AC adapter source
    if (source.type == power_source_type::ac) {
        SYSTEM_POWER_STATUS sps;
        if (GetSystemPowerStatus(&sps)) {
            reading.power_available = (sps.ACLineStatus == 1);  // 1 = AC online
        }
        return reading;
    }

    // Handle battery source
    if (source.type == power_source_type::battery) {
        // Extract battery index from path
        size_t battery_index = 0;
        size_t pos = source.path.find_last_of(':');
        if (pos != std::string::npos && pos + 1 < source.path.size()) {
            try {
                battery_index = std::stoul(source.path.substr(pos + 1));
            } catch (...) {
                // Keep battery_index = 0
            }
        }

        auto* wmi = get_wmi();
        if (wmi && wmi->is_valid()) {
            auto batteries = wmi->query_batteries();
            if (battery_index < batteries.size()) {
                const auto& bat = batteries[battery_index];
                reading.battery_available = true;
                reading.battery_percent = static_cast<double>(bat.battery_percent);
                reading.is_charging = bat.is_charging;
                reading.is_discharging = !bat.is_ac_online;
                reading.is_full = (bat.battery_percent >= 100 && bat.is_ac_online);
            }
        }

        // Fallback to GetSystemPowerStatus
        SYSTEM_POWER_STATUS sps;
        if (GetSystemPowerStatus(&sps) && sps.BatteryFlag != 128) {  // 128 = no battery
            reading.battery_available = true;
            if (sps.BatteryLifePercent != 255) {  // 255 = unknown
                reading.battery_percent = static_cast<double>(sps.BatteryLifePercent);
            }
            reading.is_charging = (sps.BatteryFlag & 8) != 0;  // 8 = charging
            reading.is_discharging = (sps.ACLineStatus == 0);
            reading.is_full = (sps.BatteryFlag & 8) == 0 && sps.ACLineStatus == 1 &&
                              sps.BatteryLifePercent >= 95;
        }
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

#endif  // defined(_WIN32)
