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

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <mach/mach.h>

#include <cstring>
#include <vector>

namespace kcenon {
namespace monitoring {
namespace {

// PCI Vendor IDs
constexpr uint16_t VENDOR_NVIDIA = 0x10de;
constexpr uint16_t VENDOR_AMD = 0x1002;
constexpr uint16_t VENDOR_INTEL = 0x8086;
constexpr uint16_t VENDOR_APPLE = 0x106b;

// SMC structures for temperature reading (same as temperature_collector)
constexpr uint32_t SMC_KEY_TYPE_FPE2 = 0x66706532;  // 'fpe2' - fixed point 2
constexpr uint32_t SMC_KEY_TYPE_SP78 = 0x73703738;  // 'sp78' - fixed point 7.8
constexpr uint32_t SMC_KEY_TYPE_FLT = 0x666c7400;   // 'flt ' - float

constexpr uint8_t SMC_CMD_READ_KEYINFO = 9;
constexpr uint8_t SMC_CMD_READ_BYTES = 5;

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

/**
 * SMC connection for reading GPU temperature
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

        size_t inputSize = sizeof(input);
        size_t outputSize = sizeof(output);

        kern_return_t result = IOConnectCallStructMethod(connection_, 2, &input, inputSize,
                                                          &output, &outputSize);
        if (result != KERN_SUCCESS) {
            return 0.0;
        }

        input.selector = SMC_CMD_READ_BYTES;
        input.keyInfo.key = key;
        input.keyInfo.keyInfo = output.keyInfo.keyInfo;

        result = IOConnectCallStructMethod(connection_, 2, &input, inputSize, &output, &outputSize);
        if (result != KERN_SUCCESS) {
            return 0.0;
        }

        uint32_t dataType = output.val.dataType;
        uint8_t* bytes = output.val.bytes;

        if (dataType == SMC_KEY_TYPE_FPE2) {
            return static_cast<double>((bytes[0] << 8) | bytes[1]) / 256.0;
        } else if (dataType == SMC_KEY_TYPE_SP78) {
            int16_t val = static_cast<int16_t>((bytes[0] << 8) | bytes[1]);
            return static_cast<double>(val) / 256.0;
        } else if (dataType == SMC_KEY_TYPE_FLT) {
            float val;
            std::memcpy(&val, bytes, sizeof(float));
            return static_cast<double>(val);
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
 * Convert 4-character key string to uint32_t
 */
uint32_t str_to_key(const char* key_str) {
    return (static_cast<uint32_t>(key_str[0]) << 24) | (static_cast<uint32_t>(key_str[1]) << 16) |
           (static_cast<uint32_t>(key_str[2]) << 8) | static_cast<uint32_t>(key_str[3]);
}

// GPU SMC temperature keys
// TG0D = GPU Die, TG0P = GPU Proximity, TG0T = GPU TGPU
const std::vector<std::pair<const char*, const char*>> gpu_temp_keys = {
    {"TG0D", "GPU Die"},
    {"TG0P", "GPU Proximity"},
    {"TG0T", "GPU TGPU"},
    {"TG1D", "GPU 1 Die"},
    {"TGDD", "GPU Diode"},
};

/**
 * CFString helper for automatic release
 */
class cf_string {
   public:
    explicit cf_string(const char* str)
        : ref_(CFStringCreateWithCString(kCFAllocatorDefault, str, kCFStringEncodingUTF8)) {}
    ~cf_string() {
        if (ref_) CFRelease(ref_);
    }
    CFStringRef get() const { return ref_; }

   private:
    CFStringRef ref_;
};

/**
 * Get string from CFDictionary
 */
std::string cf_dict_get_string(CFDictionaryRef dict, CFStringRef key) {
    CFTypeRef value = CFDictionaryGetValue(dict, key);
    if (!value || CFGetTypeID(value) != CFStringGetTypeID()) {
        return "";
    }

    char buffer[256];
    if (CFStringGetCString(static_cast<CFStringRef>(value), buffer, sizeof(buffer),
                            kCFStringEncodingUTF8)) {
        return std::string(buffer);
    }
    return "";
}

/**
 * Get 32-bit integer from CFDictionary
 */
bool cf_dict_get_int32(CFDictionaryRef dict, CFStringRef key, int32_t& value) {
    CFTypeRef cfvalue = CFDictionaryGetValue(dict, key);
    if (!cfvalue || CFGetTypeID(cfvalue) != CFNumberGetTypeID()) {
        return false;
    }
    return CFNumberGetValue(static_cast<CFNumberRef>(cfvalue), kCFNumberSInt32Type, &value);
}

/**
 * Enumerate GPUs via IOKit
 */
std::vector<gpu_device_info> enumerate_gpus_iokit() {
    std::vector<gpu_device_info> devices;
    uint32_t device_index = 0;

    // Match accelerators (GPUs)
    CFMutableDictionaryRef matching = IOServiceMatching(kIOAcceleratorClassName);
    if (!matching) {
        return devices;
    }

    io_iterator_t iterator;
    kern_return_t result =
        IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator);
    if (result != KERN_SUCCESS) {
        return devices;
    }

    io_object_t service;
    while ((service = IOIteratorNext(iterator)) != 0) {
        CFMutableDictionaryRef properties = nullptr;
        if (IORegistryEntryCreateCFProperties(service, &properties, kCFAllocatorDefault, 0) ==
            KERN_SUCCESS) {
            gpu_device_info info;
            info.id = "gpu" + std::to_string(device_index);
            info.device_index = device_index;

            // Get vendor ID
            int32_t vendor_id = 0;
            cf_string vendor_key("vendor-id");
            CFTypeRef vendor_data = CFDictionaryGetValue(properties, vendor_key.get());
            if (vendor_data && CFGetTypeID(vendor_data) == CFDataGetTypeID()) {
                CFDataRef data = static_cast<CFDataRef>(vendor_data);
                if (CFDataGetLength(data) >= 2) {
                    const uint8_t* bytes = CFDataGetBytePtr(data);
                    vendor_id = bytes[0] | (bytes[1] << 8);
                }
            }

            // Map vendor ID
            switch (vendor_id) {
                case VENDOR_NVIDIA:
                    info.vendor = gpu_vendor::nvidia;
                    info.type = gpu_type::discrete;
                    info.name = "NVIDIA GPU";
                    break;
                case VENDOR_AMD:
                    info.vendor = gpu_vendor::amd;
                    info.type = gpu_type::discrete;
                    info.name = "AMD GPU";
                    break;
                case VENDOR_INTEL:
                    info.vendor = gpu_vendor::intel;
                    info.type = gpu_type::integrated;
                    info.name = "Intel GPU";
                    break;
                case VENDOR_APPLE:
                    info.vendor = gpu_vendor::apple;
                    info.type = gpu_type::integrated;
                    info.name = "Apple GPU";
                    break;
                default:
                    if (vendor_id != 0) {
                        info.vendor = gpu_vendor::other;
                        info.name = "GPU";
                    }
                    break;
            }

            // Try to get model name
            cf_string model_key("model");
            std::string model = cf_dict_get_string(properties, model_key.get());
            if (!model.empty()) {
                info.name = model;
            }

            // Only add if we identified a vendor
            if (info.vendor != gpu_vendor::unknown) {
                devices.push_back(info);
                ++device_index;
            }

            CFRelease(properties);
        }
        IOObjectRelease(service);
    }
    IOObjectRelease(iterator);

    return devices;
}

/**
 * Read GPU temperature via SMC
 */
bool read_gpu_temperature(double& temperature) {
    smc_connection* smc = get_smc();
    if (!smc || !smc->is_valid()) {
        return false;
    }

    // Try each GPU temperature key
    for (const auto& [key, name] : gpu_temp_keys) {
        double temp = smc->read_temperature(str_to_key(key));
        if (temp > 0.0 && temp < 150.0) {  // Reasonable temperature range
            temperature = temp;
            return true;
        }
    }

    return false;
}

}  // anonymous namespace

// gpu_info_collector implementation for macOS

gpu_info_collector::gpu_info_collector() {}
gpu_info_collector::~gpu_info_collector() {}

bool gpu_info_collector::is_gpu_available() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (gpu_checked_) {
        return gpu_available_;
    }

    gpu_checked_ = true;
    gpu_available_ = false;

    // Try to enumerate GPUs
    auto devices = enumerate_gpus_iokit();
    gpu_available_ = !devices.empty();

    return gpu_available_;
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
    return enumerate_gpus_iokit();
}

gpu_reading gpu_info_collector::read_gpu_metrics(const gpu_device_info& device) {
    std::lock_guard<std::mutex> lock(mutex_);
    return read_gpu_metrics_impl(device);
}

gpu_reading gpu_info_collector::read_gpu_metrics_impl(const gpu_device_info& device) {
    gpu_reading reading;
    reading.device = device;
    reading.timestamp = std::chrono::system_clock::now();

    // On macOS, we can only reliably get temperature via SMC
    // Utilization and memory metrics require Metal Performance Shaders or vendor APIs
    // which are not available without additional frameworks

    // Try to read GPU temperature
    double temperature = 0.0;
    if (read_gpu_temperature(temperature)) {
        reading.temperature_celsius = temperature;
        reading.temperature_available = true;
    }

    // Note: Other metrics (utilization, memory, power, clock, fan) are not
    // available via public macOS APIs without vendor-specific libraries
    // (CUDA for NVIDIA, Metal for Apple, etc.)

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

#endif  // __APPLE__
