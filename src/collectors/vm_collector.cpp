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

#include "kcenon/monitoring/collectors/vm_collector.h"
#include "kcenon/monitoring/utils/config_parser.h"

#include <fstream>
#include <sstream>
#include <iostream>

#if defined(__linux__)
#include <sys/stat.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

namespace kcenon {
namespace monitoring {

vm_info_collector::vm_info_collector() { detect_vm_environment(); }
vm_info_collector::~vm_info_collector() = default;

void vm_info_collector::detect_vm_environment() {
    if (info_cached_) return;

    cached_metrics_ = vm_metrics{}; // Reset
    
#if defined(__linux__)
    // Method 1: Check /sys/class/dmi/id/product_name
    std::ifstream dmi_file("/sys/class/dmi/id/product_name");
    if (dmi_file.good()) {
        std::string line;
        std::getline(dmi_file, line);
        if (!line.empty()) {
            if (line.find("KVM") != std::string::npos) {
                cached_metrics_.type = vm_type::kvm;
                cached_metrics_.is_virtualized = true;
                cached_metrics_.hypervisor_vendor = "KVM";
            } else if (line.find("VMware") != std::string::npos) {
                cached_metrics_.type = vm_type::vmware;
                cached_metrics_.is_virtualized = true;
                cached_metrics_.hypervisor_vendor = "VMware";
            } else if (line.find("VirtualBox") != std::string::npos) {
                cached_metrics_.type = vm_type::virtualbox;
                cached_metrics_.is_virtualized = true;
                cached_metrics_.hypervisor_vendor = "VirtualBox";
            }
        }
    }
    
    // Method 2: Check /proc/cpuinfo for "hypervisor" flag only if not yet detected
    if (!cached_metrics_.is_virtualized) {
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("flags") != std::string::npos) {
                if (line.find("hypervisor") != std::string::npos) {
                    cached_metrics_.is_virtualized = true;
                    if (cached_metrics_.hypervisor_vendor.empty()) {
                        cached_metrics_.hypervisor_vendor = "Unknown (Generic Hypervisor)";
                        cached_metrics_.type = vm_type::other;
                    }
                    break;
                }
            }
        }
    }

#elif defined(__APPLE__)
    // Check kern.hv_vmm_present using sysctl (Available on newer macOS for Apple Silicon/Hypervisor framework)
    // Also check machdep.cpu.features for VMM on Intel.
    
    int mib[2];
    size_t len;
    
    // Try machdep.cpu.features
    mib[0] = CTL_MACHDEP;
    // mib[1] was using CPU_SUBTYPE_INTEL_FAMILY_MAX which might be missing.
    // However, we are not using mib for sysctlbyname below.
    // Ideally we should use sysctlbyname entirely.
    
    char value[1024];
    len = sizeof(value);
    if (sysctlbyname("machdep.cpu.features", value, &len, NULL, 0) == 0) {
        std::string features(value);
        if (features.find("VMM") != std::string::npos) {
            cached_metrics_.is_virtualized = true;
            cached_metrics_.hypervisor_vendor = "Apple Hypervisor";
            cached_metrics_.type = vm_type::other; // Could be Parallels, VMware etc.
        }
    }
    
    // Apple Silicon specifics could be checked via kern.hv_vmm_present
    int vmm_present = 0;
    len = sizeof(vmm_present);
    if (sysctlbyname("kern.hv_vmm_present", &vmm_present, &len, NULL, 0) == 0) {
        if (vmm_present) {
            cached_metrics_.is_virtualized = true;
            cached_metrics_.hypervisor_vendor = "Apple Silicon Hypervisor";
             cached_metrics_.type = vm_type::other;
        }
    }

#elif defined(_WIN32)
    // Stub for Windows
    // Would typically use CPUID instruction or WMI
    cached_metrics_.is_virtualized = false;
#endif

    info_cached_ = true;
}

double vm_info_collector::get_steal_time() {
#if defined(__linux__)
    // Parse /proc/stat
    // cpu  user nice system idle iowait irq softirq steal guest guest_nice
    std::ifstream stat_file("/proc/stat");
    std::string line;
    if (std::getline(stat_file, line)) {
        if (line.substr(0, 3) == "cpu") {
            std::istringstream iss(line);
            std::string cpu_label;
            long long user, nice, system, idle, iowait, irq, softirq, steal;
            iss >> cpu_label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
            
            // This is cumulative. To get percentage we need delta.
            // For a simple collector we might return the raw value or 0 if we don't track state here.
            // The vm_info_collector in this design is simple.
            // Ideally we should track previous values to calculate rate, effectively handled by bigger collectors.
            // But here we are returning a snapshot.
            // If the user wants % steal time, we need state.
            // Let's just return a placeholder or raw steal ticks for now, 
            // OR if the requirement implies instantaneous %, we need to keep state.
            
            // Since vm_collector is higher level, we can keep state there, or here.
            // Let's assume we return raw ticks or 0.
            // BUT, `vm_metrics` has `guest_cpu_steal_time` as double, implying %.
            // Implementing % calculation requires previous snapshot.
            
            // NOTE: For simplicity in this iteration without major refactor of base classes:
            // We will return 0.0 unless we implement delta tracking.
            // Let's implement simple delta tracking here.
            
            static long long prev_total = 0;
            static long long prev_steal = 0;
            
            long long total = user + nice + system + idle + iowait + irq + softirq + steal;
            
            double steal_percent = 0.0;
            if (prev_total > 0 && total > prev_total) {
                long long total_delta = total - prev_total;
                long long steal_delta = steal - prev_steal;
                steal_percent = (static_cast<double>(steal_delta) / total_delta) * 100.0;
            }
            
            prev_total = total;
            prev_steal = steal;
            
            return steal_percent;
        }
    }
#endif
    return 0.0;
}

vm_metrics vm_info_collector::collect_metrics() {
    if (!info_cached_) {
        detect_vm_environment();
    }
    
    vm_metrics m = cached_metrics_;
    m.guest_cpu_steal_time = get_steal_time();
    return m;
}

// =========================================================================================

vm_collector::vm_collector() : collector_(std::make_unique<vm_info_collector>()) {}

bool vm_collector::initialize(const std::unordered_map<std::string, std::string>& config) {
    enabled_ = config_parser::get<bool>(config, "enabled", true);
    return true;
}

std::vector<metric> vm_collector::collect() {
    std::vector<metric> metrics;
    if (!enabled_) return metrics;

    collection_count_++;
    
    try {
        vm_metrics vm_data = collector_->collect_metrics();
        
        // Metric: is_virtualized
        metrics.push_back(create_metric(
            "system.vm.is_virtualized",
            vm_data.is_virtualized ? 1.0 : 0.0,
            {{"detected_type", vm_type_to_string(vm_data.type)},
             {"vendor", vm_data.hypervisor_vendor}}
        ));
        
        if (vm_data.is_virtualized) {
             metrics.push_back(create_metric(
                "system.vm.steal_time",
                vm_data.guest_cpu_steal_time,
                {},
                "%"
            ));
        }
        
    } catch (...) {
        collection_errors_++;
    }

    return metrics;
}

std::vector<std::string> vm_collector::get_metric_types() const {
    return {"system.vm.is_virtualized", "system.vm.steal_time"};
}

bool vm_collector::is_healthy() const {
    return true;
}

bool vm_collector::is_available() const {
    // VM detection is available on all platforms
    return true;
}

std::unordered_map<std::string, double> vm_collector::get_statistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return {
        {"collection_count", static_cast<double>(collection_count_)},
        {"collection_errors", static_cast<double>(collection_errors_)}
    };
}

metric vm_collector::create_metric(const std::string& name, double value,
                                         const std::unordered_map<std::string, std::string>& tags,
                                         const std::string& unit) const {
    metric m;
    m.name = name;
    m.value = value;
    m.timestamp = std::chrono::system_clock::now();
    m.tags = tags;
    if (!unit.empty()) {
        m.tags["unit"] = unit;
    }
    return m;
}

}  // namespace monitoring
}  // namespace kcenon
