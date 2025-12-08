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
 * WMI connection manager for temperature queries
 */
class wmi_connection {
   public:
    wmi_connection() {
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

        // Connect to WMI
        hr = locator_->ConnectServer(_bstr_t(L"ROOT\\WMI"), nullptr, nullptr, nullptr, 0, nullptr,
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

    ~wmi_connection() {
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

    struct thermal_zone_info {
        std::string instance_name;
        double current_temperature;
        double critical_temperature;
        bool active;
    };

    std::vector<thermal_zone_info> query_thermal_zones() {
        std::vector<thermal_zone_info> zones;

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

            thermal_zone_info zone;

            // Get InstanceName
            VARIANT var_instance;
            VariantInit(&var_instance);
            hr = obj->Get(L"InstanceName", 0, &var_instance, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_instance.vt == VT_BSTR) {
                zone.instance_name = wide_to_narrow(var_instance.bstrVal);
            }
            VariantClear(&var_instance);

            // Get CurrentTemperature (in tenths of Kelvin)
            VARIANT var_temp;
            VariantInit(&var_temp);
            hr = obj->Get(L"CurrentTemperature", 0, &var_temp, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_temp.vt == VT_I4) {
                // Convert from tenths of Kelvin to Celsius
                zone.current_temperature = (static_cast<double>(var_temp.lVal) / 10.0) - 273.15;
            }
            VariantClear(&var_temp);

            // Get CriticalTripPoint
            VARIANT var_crit;
            VariantInit(&var_crit);
            hr = obj->Get(L"CriticalTripPoint", 0, &var_crit, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_crit.vt == VT_I4) {
                zone.critical_temperature = (static_cast<double>(var_crit.lVal) / 10.0) - 273.15;
            }
            VariantClear(&var_crit);

            // Get Active
            VARIANT var_active;
            VariantInit(&var_active);
            hr = obj->Get(L"Active", 0, &var_active, nullptr, nullptr);
            if (SUCCEEDED(hr) && var_active.vt == VT_BOOL) {
                zone.active = (var_active.boolVal != VARIANT_FALSE);
            }
            VariantClear(&var_active);

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

// Global WMI connection (lazy-initialized)
static std::unique_ptr<wmi_connection> g_wmi;
static std::mutex g_wmi_mutex;

wmi_connection* get_wmi() {
    std::lock_guard<std::mutex> lock(g_wmi_mutex);
    if (!g_wmi) {
        g_wmi = std::make_unique<wmi_connection>();
    }
    return g_wmi.get();
}

}  // anonymous namespace

// temperature_info_collector implementation for Windows

temperature_info_collector::temperature_info_collector() = default;
temperature_info_collector::~temperature_info_collector() = default;

bool temperature_info_collector::is_thermal_available() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (thermal_checked_) {
        return thermal_available_;
    }

    thermal_checked_ = true;

    auto* wmi = get_wmi();
    thermal_available_ = (wmi != nullptr && wmi->is_valid());

    return thermal_available_;
}

std::vector<temperature_sensor_info> temperature_info_collector::enumerate_sensors() {
    std::lock_guard<std::mutex> lock(mutex_);
    return enumerate_sensors_impl();
}

std::vector<temperature_sensor_info> temperature_info_collector::enumerate_sensors_impl() {
    std::vector<temperature_sensor_info> sensors;

    auto* wmi = get_wmi();
    if (!wmi || !wmi->is_valid()) {
        return sensors;
    }

    auto zones = wmi->query_thermal_zones();

    for (size_t i = 0; i < zones.size(); ++i) {
        const auto& zone = zones[i];

        temperature_sensor_info info;
        info.id = "thermal_zone_" + std::to_string(i);
        info.name =
            zone.instance_name.empty() ? ("Thermal Zone " + std::to_string(i)) : zone.instance_name;
        info.zone_path = info.id;

        // Try to classify based on instance name
        std::string lower_name = zone.instance_name;
        std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (lower_name.find("cpu") != std::string::npos ||
            lower_name.find("proc") != std::string::npos) {
            info.type = sensor_type::cpu;
        } else if (lower_name.find("gpu") != std::string::npos ||
                   lower_name.find("video") != std::string::npos) {
            info.type = sensor_type::gpu;
        } else {
            info.type = sensor_type::motherboard;
        }

        sensors.push_back(info);
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

    auto* wmi = get_wmi();
    if (!wmi || !wmi->is_valid()) {
        return reading;
    }

    // Extract zone index from id
    size_t zone_index = 0;
    size_t pos = sensor.id.find_last_of('_');
    if (pos != std::string::npos && pos + 1 < sensor.id.size()) {
        try {
            zone_index = std::stoul(sensor.id.substr(pos + 1));
        } catch (...) {
            // Keep zone_index = 0
        }
    }

    auto zones = wmi->query_thermal_zones();

    if (zone_index < zones.size()) {
        const auto& zone = zones[zone_index];
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
    }

    return reading;
}

std::vector<temperature_reading> temperature_info_collector::read_all_temperatures() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<temperature_reading> readings;

    auto* wmi = get_wmi();
    if (!wmi || !wmi->is_valid()) {
        return readings;
    }

    // Refresh sensor list if empty
    if (cached_sensors_.empty()) {
        enumerate_sensors_impl();
    }

    auto zones = wmi->query_thermal_zones();

    for (size_t i = 0; i < cached_sensors_.size() && i < zones.size(); ++i) {
        temperature_reading reading;
        reading.sensor = cached_sensors_[i];
        reading.timestamp = std::chrono::system_clock::now();
        reading.temperature_celsius = zones[i].current_temperature;

        if (zones[i].critical_temperature > 0.0) {
            reading.thresholds_available = true;
            reading.critical_threshold_celsius = zones[i].critical_temperature;
            reading.warning_threshold_celsius = zones[i].critical_temperature - 10.0;

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

}  // namespace monitoring
}  // namespace kcenon

#endif  // defined(_WIN32)
