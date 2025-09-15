/**
 * @file performance_monitor.cpp
 * @brief Performance monitoring implementation
 */

#include <kcenon/monitoring/core/performance_monitor.h>
#include <shared_mutex>
#include <fstream>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <tlhelp32.h>
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")
#elif __linux__
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#elif __APPLE__
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/vm_map.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

namespace kcenon::monitoring {

// Performance Profiler Implementation
result<bool> performance_profiler::record_sample(
    const std::string& operation_name,
    std::chrono::nanoseconds duration,
    bool success) {
    
    if (!enabled_) {
        return result<bool>(true);
    }
    
    std::unique_lock lock(profiles_mutex_);
    
    auto& profile = profiles_[operation_name];
    if (!profile) {
        profile = std::make_unique<profile_data>();
    }
    
    lock.unlock();
    
    // Update counters
    profile->call_count.fetch_add(1);
    if (!success) {
        profile->error_count.fetch_add(1);
    }
    
    // Record sample
    std::lock_guard sample_lock(profile->mutex);
    
    // Limit samples to prevent unbounded growth
    if (profile->samples.size() >= max_samples_per_operation_) {
        // Remove oldest sample (simple ring buffer behavior)
        profile->samples.erase(profile->samples.begin());
    }
    
    profile->samples.push_back(duration);
    
    return result<bool>(true);
}

result<performance_metrics> performance_profiler::get_metrics(
    const std::string& operation_name) const {
    
    std::shared_lock lock(profiles_mutex_);
    
    auto it = profiles_.find(operation_name);
    if (it == profiles_.end()) {
        return make_error<performance_metrics>(
            monitoring_error_code::not_found
        );
    }
    
    const auto& profile = it->second;
    lock.unlock();
    
    std::lock_guard sample_lock(profile->mutex);
    
    performance_metrics metrics;
    metrics.operation_name = operation_name;
    metrics.call_count = profile->call_count.load();
    metrics.error_count = profile->error_count.load();
    
    if (!profile->samples.empty()) {
        metrics.update_statistics(profile->samples);
        
        // Calculate throughput (operations per second)
        if (metrics.total_duration.count() > 0) {
            double seconds = metrics.total_duration.count() / 1e9;
            metrics.throughput = metrics.call_count / seconds;
        }
    }
    
    return metrics;
}

std::vector<performance_metrics> performance_profiler::get_all_metrics() const {
    std::vector<performance_metrics> all_metrics;
    
    std::shared_lock lock(profiles_mutex_);
    
    for (const auto& [name, profile] : profiles_) {
        auto result = get_metrics(name);
        if (result) {
            all_metrics.push_back(result.value());
        }
    }
    
    return all_metrics;
}

result<bool> performance_profiler::clear_samples(
    const std::string& operation_name) {
    
    std::unique_lock lock(profiles_mutex_);
    
    auto it = profiles_.find(operation_name);
    if (it == profiles_.end()) {
        return make_error<bool>(
            monitoring_error_code::not_found
        );
    }
    
    profiles_.erase(it);
    return result<bool>(true);
}

void performance_profiler::clear_all_samples() {
    std::unique_lock lock(profiles_mutex_);
    profiles_.clear();
}

// System Monitor Implementation
struct system_monitor::monitor_impl {
    std::atomic<bool> monitoring{false};
    std::thread monitor_thread;
    std::chrono::milliseconds interval{1000};
    std::vector<system_metrics> history;
    mutable std::mutex history_mutex;
    std::size_t max_history_size{3600}; // 1 hour at 1 second interval
    
    ~monitor_impl() {
        stop();
    }
    
    void stop() {
        if (monitoring.exchange(false)) {
            if (monitor_thread.joinable()) {
                monitor_thread.join();
            }
        }
    }
    
    void monitor_loop() {
        while (monitoring) {
            auto metrics = collect_system_metrics();
            
            {
                std::lock_guard lock(history_mutex);
                if (history.size() >= max_history_size) {
                    history.erase(history.begin());
                }
                history.push_back(metrics);
            }
            
            std::this_thread::sleep_for(interval);
        }
    }
    
    system_metrics collect_system_metrics() {
        system_metrics metrics;
        metrics.timestamp = std::chrono::system_clock::now();
        
#ifdef _WIN32
        // Windows implementation
        MEMORYSTATUSEX mem_status;
        mem_status.dwLength = sizeof(mem_status);
        if (GlobalMemoryStatusEx(&mem_status)) {
            metrics.memory_usage_percent = static_cast<double>(mem_status.dwMemoryLoad);
            metrics.memory_usage_bytes = mem_status.ullTotalPhys - mem_status.ullAvailPhys;
            metrics.available_memory_bytes = mem_status.ullAvailPhys;
        }
        
        // Get process info
        HANDLE process = GetCurrentProcess();
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(process, &pmc, sizeof(pmc))) {
            metrics.memory_usage_bytes = pmc.WorkingSetSize;
        }
        
        // Thread count
        DWORD thread_count = 0;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            THREADENTRY32 entry;
            entry.dwSize = sizeof(entry);
            DWORD process_id = GetCurrentProcessId();
            
            if (Thread32First(snapshot, &entry)) {
                do {
                    if (entry.th32OwnerProcessID == process_id) {
                        thread_count++;
                    }
                } while (Thread32Next(snapshot, &entry));
            }
            CloseHandle(snapshot);
            metrics.thread_count = thread_count;
        }
        
#elif __linux__
        // Linux implementation
        struct sysinfo sys_info;
        if (sysinfo(&sys_info) == 0) {
            unsigned long total_mem = sys_info.totalram * sys_info.mem_unit;
            unsigned long used_mem = (sys_info.totalram - sys_info.freeram) * sys_info.mem_unit;
            
            metrics.memory_usage_percent = (static_cast<double>(used_mem) / total_mem) * 100.0;
            metrics.memory_usage_bytes = used_mem;
            metrics.available_memory_bytes = sys_info.freeram * sys_info.mem_unit;
        }
        
        // CPU usage from /proc/stat
        std::ifstream stat_file("/proc/stat");
        if (stat_file.is_open()) {
            std::string line;
            if (std::getline(stat_file, line)) {
                std::istringstream iss(line);
                std::string cpu;
                long user, nice, system, idle;
                iss >> cpu >> user >> nice >> system >> idle;
                
                static long prev_idle = idle;
                static long prev_total = user + nice + system + idle;
                
                long total = user + nice + system + idle;
                long total_diff = total - prev_total;
                long idle_diff = idle - prev_idle;
                
                if (total_diff > 0) {
                    metrics.cpu_usage_percent = 100.0 * (1.0 - static_cast<double>(idle_diff) / total_diff);
                }
                
                prev_idle = idle;
                prev_total = total;
            }
        }
        
        // Thread count from /proc/self/status
        std::ifstream status_file("/proc/self/status");
        if (status_file.is_open()) {
            std::string line;
            while (std::getline(status_file, line)) {
                if (line.find("Threads:") == 0) {
                    std::istringstream iss(line.substr(8));
                    iss >> metrics.thread_count;
                    break;
                }
            }
        }
        
#elif __APPLE__
        // macOS implementation
        vm_size_t page_size;
        vm_statistics64_data_t vm_stat;
        mach_msg_type_number_t host_size = sizeof(vm_stat) / sizeof(natural_t);
        
        if (host_page_size(mach_host_self(), &page_size) == KERN_SUCCESS &&
            host_statistics64(mach_host_self(), HOST_VM_INFO64,
                            (host_info64_t)&vm_stat, &host_size) == KERN_SUCCESS) {
            
            uint64_t total_pages = vm_stat.free_count + vm_stat.active_count +
                                  vm_stat.inactive_count + vm_stat.wire_count;
            uint64_t used_pages = total_pages - vm_stat.free_count;
            
            metrics.memory_usage_bytes = used_pages * page_size;
            metrics.available_memory_bytes = vm_stat.free_count * page_size;
            metrics.memory_usage_percent = (static_cast<double>(used_pages) / total_pages) * 100.0;
        }
        
        // Get thread count
        thread_array_t thread_list;
        mach_msg_type_number_t thread_count;
        if (task_threads(mach_task_self(), &thread_list, &thread_count) == KERN_SUCCESS) {
            metrics.thread_count = thread_count;
            vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                         thread_count * sizeof(thread_t));
        }
#endif
        
        return metrics;
    }
};

system_monitor::system_monitor()
    : impl_(std::make_unique<monitor_impl>()) {}

system_monitor::~system_monitor() = default;

system_monitor::system_monitor(system_monitor&&) noexcept = default;
system_monitor& system_monitor::operator=(system_monitor&&) noexcept = default;

result<system_metrics> system_monitor::get_current_metrics() const {
    if (!impl_) {
        return make_error<system_metrics>(
            monitoring_error_code::invalid_state
        );
    }
    
    return impl_->collect_system_metrics();
}

result<bool> system_monitor::start_monitoring(
    std::chrono::milliseconds interval) {
    
    if (!impl_) {
        return make_error<bool>(
            monitoring_error_code::invalid_state
        );
    }
    
    if (impl_->monitoring.exchange(true)) {
        // Already monitoring
        return result<bool>(true);
    }
    
    impl_->interval = interval;
    impl_->monitor_thread = std::thread(&monitor_impl::monitor_loop, impl_.get());
    
    return result<bool>(true);
}

result<bool> system_monitor::stop_monitoring() {
    if (!impl_) {
        return make_error<bool>(
            monitoring_error_code::invalid_state
        );
    }
    
    impl_->stop();
    return result<bool>(true);
}

bool system_monitor::is_monitoring() const {
    return impl_ && impl_->monitoring.load();
}

std::vector<system_metrics> system_monitor::get_history(
    std::chrono::seconds duration) const {
    
    if (!impl_) {
        return {};
    }
    
    std::lock_guard lock(impl_->history_mutex);
    
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - duration;
    
    std::vector<system_metrics> filtered;
    for (const auto& metrics : impl_->history) {
        if (metrics.timestamp >= cutoff) {
            filtered.push_back(metrics);
        }
    }
    
    return filtered;
}

// Performance Monitor Implementation
result<metrics_snapshot> performance_monitor::collect() {
    metrics_snapshot snapshot;
    snapshot.capture_time = std::chrono::system_clock::now();
    snapshot.source_id = name_;
    
    // Collect performance metrics
    auto perf_metrics = profiler_.get_all_metrics();
    for (const auto& pm : perf_metrics) {
        snapshot.add_metric(pm.operation_name + ".mean_duration_ms", 
                          pm.mean_duration.count() / 1e6);
        
        snapshot.add_metric(pm.operation_name + ".p95_duration_ms",
                          pm.p95_duration.count() / 1e6);
        
        snapshot.add_metric(pm.operation_name + ".p99_duration_ms",
                          pm.p99_duration.count() / 1e6);
        
        snapshot.add_metric(pm.operation_name + ".throughput",
                          pm.throughput);
        
        double error_rate = pm.call_count > 0 ? 
            static_cast<double>(pm.error_count) / pm.call_count : 0.0;
        snapshot.add_metric(pm.operation_name + ".error_rate", error_rate);
    }
    
    // Collect system metrics
    auto sys_result = system_monitor_.get_current_metrics();
    if (sys_result) {
        const auto& sm = sys_result.value();
        
        snapshot.add_metric("system.cpu_usage", sm.cpu_usage_percent);
        snapshot.add_metric("system.memory_usage", sm.memory_usage_percent);
        snapshot.add_metric("system.memory_bytes", static_cast<double>(sm.memory_usage_bytes));
        snapshot.add_metric("system.thread_count", static_cast<double>(sm.thread_count));
    }
    
    return snapshot;
}

result<bool> performance_monitor::check_thresholds() const {
    auto sys_result = system_monitor_.get_current_metrics();
    if (!sys_result) {
        return make_error<bool>(sys_result.get_error().code);
    }
    
    const auto& metrics = sys_result.value();
    
    bool threshold_exceeded = false;
    
    if (metrics.cpu_usage_percent > thresholds_.cpu_threshold) {
        threshold_exceeded = true;
    }
    
    if (metrics.memory_usage_percent > thresholds_.memory_threshold) {
        threshold_exceeded = true;
    }
    
    // Check latency thresholds
    auto perf_metrics = profiler_.get_all_metrics();
    for (const auto& pm : perf_metrics) {
        if (pm.p95_duration > thresholds_.latency_threshold) {
            threshold_exceeded = true;
            break;
        }
    }
    
    return result<bool>(threshold_exceeded);
}

// Global instance
performance_monitor& global_performance_monitor() {
    static performance_monitor instance;
    return instance;
}

} // namespace kcenon::monitoring