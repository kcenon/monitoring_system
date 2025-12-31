
#include <kcenon/monitoring/collectors/system_resource_collector.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>

#if defined(__APPLE__) || defined(__linux__)
#include <unistd.h>
#endif

#ifdef __APPLE__
    #include <libproc.h>
    #include <sys/proc_info.h>
#endif

namespace kcenon { namespace monitoring {

// -----------------------------------------------------------------------------
// system_info_collector Implementation
// -----------------------------------------------------------------------------

system_info_collector::system_info_collector() {
    last_collection_time_ = std::chrono::steady_clock::now();
}

system_info_collector::~system_info_collector() = default;

system_resources system_info_collector::collect() {
    system_resources resources{};

    // Collect all stats
    collect_cpu_stats(resources);
    collect_memory_stats(resources);
    collect_disk_stats(resources);
    collect_network_stats(resources);
    collect_process_stats(resources);

    last_collection_time_ = std::chrono::steady_clock::now();

    return resources;
}

std::chrono::seconds system_info_collector::get_uptime() const {
#ifdef __linux__
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        return std::chrono::seconds(si.uptime);
    }
    return std::chrono::seconds(0);
#elif __APPLE__
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    if (sysctl(mib, 2, &boottime, &len, NULL, 0) < 0) {
        return std::chrono::seconds(0);
    }
    time_t bsec = boottime.tv_sec;
    time_t csec = time(NULL);
    return std::chrono::seconds(csec - bsec);
#elif _WIN32
    return std::chrono::seconds(GetTickCount64() / 1000);
#else
    return std::chrono::seconds(0);
#endif
}

std::string system_info_collector::get_hostname() const {
    char hostname[256];
#ifdef _WIN32
    DWORD size = sizeof(hostname);
    if (GetComputerNameA(hostname, &size)) {
        return std::string(hostname);
    }
#else
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    }
#endif
    return "unknown";
}

std::string system_info_collector::get_os_info() const {
#ifdef __linux__
    return "Linux";
#elif __APPLE__
    return "macOS";
#elif _WIN32
    return "Windows";
#else
    return "Unknown";
#endif
}

void system_info_collector::collect_cpu_stats(system_resources& resources) {
#ifdef __APPLE__
    collect_macos_cpu_stats(resources);
#elif __linux__
    collect_linux_cpu_stats(resources);
#elif _WIN32
    collect_windows_cpu_stats(resources);
#endif
}

void system_info_collector::collect_memory_stats(system_resources& resources) {
#ifdef __APPLE__
    collect_macos_memory_stats(resources);
#elif __linux__
    collect_linux_memory_stats(resources);
#elif _WIN32
    collect_windows_memory_stats(resources);
#endif
}

void system_info_collector::collect_disk_stats(system_resources& resources) {
    (void)resources;
    // Basic implementation or platform specific if needed
    // For now leaving basic stub logic or simple counters
    // Actual implementation would read /proc/diskstats or IOKit
}

void system_info_collector::collect_network_stats(system_resources& resources) {
    (void)resources;
    // Basic implementation or platform specific if needed
}

void system_info_collector::collect_process_stats([[maybe_unused]] system_resources& resources) {
#ifdef __linux__
    // Linux process stats - context switches are already collected in collect_linux_cpu_stats
    // from /proc/stat. Process count can be read from /proc directory.
    // Context switches already handled in CPU stats
#elif __APPLE__
    // macOS context switches are best found by iterating processes
    int numberOfProcesses = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    if (numberOfProcesses <= 0) return;

    std::vector<pid_t> pids(numberOfProcesses + 10); // slightly more
    numberOfProcesses = proc_listpids(PROC_ALL_PIDS, 0, pids.data(), pids.size() * sizeof(pid_t));

    uint64_t total_switches = 0;

    for (int i = 0; i < numberOfProcesses; ++i) {
        if (pids[i] == 0) continue;

        struct proc_taskinfo taskinfo;
        if (proc_pidinfo(pids[i], PROC_PIDTASKINFO, 0, &taskinfo, sizeof(taskinfo)) == sizeof(taskinfo)) {
             total_switches += taskinfo.pti_csw;
             // taskinfo doesn't split voluntary/nonvoluntary unfortunately, 
             // but pti_csw is total. 
             // Some threads might have info, but let's stick to task level for efficiency.
        }
    }

    resources.context_switches.total = total_switches;

    // Calculate rate
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - last_collection_time_).count();

    if (duration > 0 && last_context_switches_total_ > 0 && total_switches >= last_context_switches_total_) {
        double seconds = duration / 1000000.0;
        resources.context_switches.per_sec = static_cast<uint64_t>((total_switches - last_context_switches_total_) / seconds);
    }

    last_context_switches_total_ = total_switches;

    resources.process.count = numberOfProcesses;

#elif _WIN32
    // Windows implementation TODO
#endif
}

#ifdef __APPLE__
void system_info_collector::collect_macos_cpu_stats(system_resources& resources) {
    natural_t processor_count;
    processor_info_array_t cpu_info;
    mach_msg_type_number_t cpu_info_count;

    if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
                            &processor_count,
                            &cpu_info,
                            &cpu_info_count) == KERN_SUCCESS) {

        uint64_t total_sys = 0, total_user = 0, total_idle = 0, total_nice = 0;

        for (natural_t i = 0; i < processor_count; ++i) {
            processor_cpu_load_info_t cpu_load = &((processor_cpu_load_info_t)cpu_info)[i];
            total_sys += cpu_load->cpu_ticks[CPU_STATE_SYSTEM];
            total_user += cpu_load->cpu_ticks[CPU_STATE_USER];
            total_idle += cpu_load->cpu_ticks[CPU_STATE_IDLE];
            total_nice += cpu_load->cpu_ticks[CPU_STATE_NICE];
        }

        uint64_t total = total_sys + total_user + total_idle + total_nice;
        
        // Use last_cpu_stats_ to calculate delta
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        uint64_t prev_total = last_cpu_stats_.system + last_cpu_stats_.user + 
                              last_cpu_stats_.idle + last_cpu_stats_.nice;
                              
        if (total > prev_total) {
            uint64_t total_delta = total - prev_total;
            resources.cpu.user_percent = 100.0 * (total_user - last_cpu_stats_.user) / total_delta;
            resources.cpu.system_percent = 100.0 * (total_sys - last_cpu_stats_.system) / total_delta;
            resources.cpu.idle_percent = 100.0 * (total_idle - last_cpu_stats_.idle) / total_delta;
            resources.cpu.usage_percent = resources.cpu.user_percent + resources.cpu.system_percent;
        }

        last_cpu_stats_.system = total_sys;
        last_cpu_stats_.user = total_user;
        last_cpu_stats_.idle = total_idle;
        last_cpu_stats_.nice = total_nice;

        vm_deallocate(mach_task_self(),
                     reinterpret_cast<vm_address_t>(cpu_info),
                     cpu_info_count * sizeof(int));
    }
    
    // Load Average
    struct host_load_info load_info;
    mach_msg_type_number_t count = HOST_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_LOAD_INFO, (host_info_t)&load_info, &count) == KERN_SUCCESS) {
        resources.cpu.load.one_min = (double)load_info.avenrun[0] / LOAD_SCALE;
        resources.cpu.load.five_min = (double)load_info.avenrun[1] / LOAD_SCALE;
        resources.cpu.load.fifteen_min = (double)load_info.avenrun[2] / LOAD_SCALE;
    }
}

void system_info_collector::collect_macos_memory_stats(system_resources& resources) {
    vm_statistics64_data_t vm_stats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

    if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                         reinterpret_cast<host_info64_t>(&vm_stats),
                         &count) == KERN_SUCCESS) {

        vm_size_t page_size = 4096; // Default fallback
        host_page_size(mach_host_self(), &page_size);

        uint64_t total_memory = 0;
        int mib[2] = {CTL_HW, HW_MEMSIZE};
        size_t length = sizeof(total_memory);
        sysctl(mib, 2, &total_memory, &length, NULL, 0);

        resources.memory.total_bytes = total_memory;
        resources.memory.available_bytes = vm_stats.free_count * page_size;
        resources.memory.used_bytes = (vm_stats.active_count + vm_stats.wire_count + vm_stats.compressor_page_count) * page_size;

        if (total_memory > 0) {
            resources.memory.usage_percent = 100.0 * resources.memory.used_bytes / total_memory;
        }
    }
}

#elif __linux__
system_info_collector::cpu_stats system_info_collector::parse_proc_stat() {
    cpu_stats stats{};
    std::ifstream file("/proc/stat");
    std::string line;
    
    while (std::getline(file, line)) {
        if (line.substr(0, 4) == "cpu ") {
            std::istringstream iss(line);
            std::string cpu;
            iss >> cpu >> stats.user >> stats.nice >> stats.system >> stats.idle 
                >> stats.iowait >> stats.irq >> stats.softirq >> stats.steal;
        } else if (line.substr(0, 5) == "ctxt ") {
             // We can store context switches here temporarily or return it
             // But the struct cpu_stats doesn't have it.
             // We will handle it in collect_linux_cpu_stats
        }
    }
    return stats;
}

void system_info_collector::collect_linux_cpu_stats(system_resources& resources) {
    std::ifstream file("/proc/stat");
    std::string line;
    uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
    uint64_t context_switches = 0;

    while (std::getline(file, line)) {
        if (line.substr(0, 4) == "cpu ") {
            std::istringstream iss(line);
            std::string cpu;
            iss >> cpu >> user >> nice >> system >> idle 
                >> iowait >> irq >> softirq >> steal;
        } else if (line.substr(0, 5) == "ctxt ") {
            std::istringstream iss(line);
            std::string label;
            iss >> label >> context_switches;
        }
    }

    // Context Switch Metrics
    resources.context_switches.total = context_switches;

    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - last_collection_time_).count();

    if (duration > 0 && last_context_switches_total_ > 0 && context_switches >= last_context_switches_total_) {
        double seconds = duration / 1000000.0;
        resources.context_switches.per_sec = static_cast<uint64_t>((context_switches - last_context_switches_total_) / seconds);
    }
    last_context_switches_total_ = context_switches;

    // CPU Metrics
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
    uint64_t prev_total = last_cpu_stats_.user + last_cpu_stats_.nice + last_cpu_stats_.system + 
                          last_cpu_stats_.idle + last_cpu_stats_.iowait + last_cpu_stats_.irq + 
                          last_cpu_stats_.softirq + last_cpu_stats_.steal;

    if (total > prev_total) {
        uint64_t total_delta = total - prev_total;
        uint64_t idle_delta = (idle + iowait) - (last_cpu_stats_.idle + last_cpu_stats_.iowait);

        resources.cpu.usage_percent = 100.0 * (1.0 - (double)idle_delta / total_delta);
        resources.cpu.user_percent = 100.0 * (user - last_cpu_stats_.user) / total_delta;
        resources.cpu.system_percent = 100.0 * (system - last_cpu_stats_.system) / total_delta;
        resources.cpu.idle_percent = 100.0 * (idle - last_cpu_stats_.idle) / total_delta;
    }

    last_cpu_stats_ = {user, nice, system, idle, iowait, irq, softirq, steal};

    // Load Average
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        resources.cpu.load.one_min = si.loads[0] / 65536.0;
        resources.cpu.load.five_min = si.loads[1] / 65536.0;
        resources.cpu.load.fifteen_min = si.loads[2] / 65536.0;
    }
}

void system_info_collector::collect_linux_memory_stats(system_resources& resources) {
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        resources.memory.total_bytes = si.totalram * si.mem_unit;
        resources.memory.available_bytes = si.freeram * si.mem_unit; // buffers/cache not included in freeram usually
        resources.memory.used_bytes = resources.memory.total_bytes - resources.memory.available_bytes;
        // More precise meminfo parsing would be better but this is sufficient for now

        if (resources.memory.total_bytes > 0) {
            resources.memory.usage_percent = 100.0 * resources.memory.used_bytes / resources.memory.total_bytes;
        }
    }
}

#elif _WIN32
void system_info_collector::collect_windows_cpu_stats([[maybe_unused]] system_resources& resources) {
    // TODO: Implement Windows CPU stats collection
}
void system_info_collector::collect_windows_memory_stats([[maybe_unused]] system_resources& resources) {
    // TODO: Implement Windows memory stats collection
}
#endif

#ifndef LOAD_SCALE
#define LOAD_SCALE 1000
#endif

// -----------------------------------------------------------------------------
// system_resource_collector Plugin Implementation
// -----------------------------------------------------------------------------

system_resource_collector::system_resource_collector()
    : collector_(std::make_unique<system_info_collector>())
    , load_history_(std::make_unique<load_average_history>(1000)) {
}

bool system_resource_collector::initialize(const std::unordered_map<std::string, std::string>& config) {
    auto it = config.find("load_history_max_samples");
    if (it != config.end()) {
        try {
            size_t max_samples = std::stoull(it->second);
            if (max_samples > 0) {
                load_history_ = std::make_unique<load_average_history>(max_samples);
            }
        } catch (...) {
            // Ignore invalid config, use default
        }
    }

    it = config.find("enable_load_history");
    if (it != config.end()) {
        enable_load_history_ = (it->second == "true" || it->second == "1");
    }

    return true;
}

std::vector<metric> system_resource_collector::collect() {
    collection_count_++;
    // auto start_time = std::chrono::steady_clock::now();

    system_resources resources = collector_->collect();

    // Convert to shared_ptr for storage
    last_resources_ = std::make_shared<system_resources>(resources);

    // Track load average history
    if (enable_load_history_ && load_history_) {
        load_history_->add_sample(resources.cpu.load.one_min, resources.cpu.load.five_min,
                                  resources.cpu.load.fifteen_min);
    }
    
    std::vector<metric> metrics;
    
    if (collect_cpu_metrics_) {
        add_cpu_metrics(metrics, resources);
    }
    if (collect_memory_metrics_) {
        add_memory_metrics(metrics, resources);
    }
    if (collect_disk_metrics_) {
        add_disk_metrics(metrics, resources);
    }
    if (collect_network_metrics_) {
        add_network_metrics(metrics, resources);
    }
    if (collect_process_metrics_) {
        add_process_metrics(metrics, resources);
    }

    return metrics;
}

std::vector<std::string> system_resource_collector::get_metric_types() const {
    return {
        "cpu_usage", "memory_usage", "context_switches_total", "context_switches_rate"
    };
}

bool system_resource_collector::is_healthy() const {
    return true;
}

std::unordered_map<std::string, double> system_resource_collector::get_statistics() const {
    return {
        {"collection_count", (double)collection_count_},
        {"errors", (double)collection_errors_}
    };
}

void system_resource_collector::set_collection_filters(bool enable_cpu, bool enable_memory,
                                                      bool enable_disk, bool enable_network) {
    collect_cpu_metrics_ = enable_cpu;
    collect_memory_metrics_ = enable_memory;
    collect_disk_metrics_ = enable_disk;
    collect_network_metrics_ = enable_network;
}

system_resources system_resource_collector::get_last_resources() const {
    if (last_resources_) {
        return *last_resources_;
    }
    return system_resources{};
}

std::vector<load_average_sample> system_resource_collector::get_all_load_history() const {
    if (load_history_) {
        return load_history_->get_all_samples();
    }
    return {};
}

load_average_statistics system_resource_collector::get_all_load_statistics() const {
    if (load_history_) {
        return load_history_->get_statistics();
    }
    return {};
}

void system_resource_collector::configure_load_history(size_t max_samples) {
    if (max_samples > 0) {
        load_history_ = std::make_unique<load_average_history>(max_samples);
    }
}

bool system_resource_collector::is_load_history_enabled() const {
    return enable_load_history_ && load_history_ != nullptr;
}

metric system_resource_collector::create_metric(const std::string& name, double value, 
                                               const std::string& unit,
                                               const std::unordered_map<std::string, std::string>& labels) const {
    metric m;
    m.name = name;
    m.value = value;
    if (!unit.empty()) {
        m.tags = labels;
        m.tags["unit"] = unit;
    } else {
        m.tags = labels;
    }
    m.timestamp = std::chrono::system_clock::now();
    return m;
}

void system_resource_collector::add_cpu_metrics(std::vector<metric>& metrics, const system_resources& resources) {
    metrics.push_back(create_metric("cpu_usage_percent", resources.cpu.usage_percent, "%"));
    metrics.push_back(create_metric("cpu_user_percent", resources.cpu.user_percent, "%"));
    metrics.push_back(create_metric("cpu_system_percent", resources.cpu.system_percent, "%"));
    metrics.push_back(create_metric("load_average_1min", resources.cpu.load.one_min));

    // Add context switch metrics here as they are closely related to CPU
    metrics.push_back(create_metric("context_switches_total", (double)resources.context_switches.total));
    metrics.push_back(create_metric("context_switches_per_sec", (double)resources.context_switches.per_sec, "ops/s"));
}

void system_resource_collector::add_memory_metrics(std::vector<metric>& metrics, const system_resources& resources) {
    metrics.push_back(create_metric("memory_usage_percent", resources.memory.usage_percent, "%"));
    metrics.push_back(create_metric("memory_used_bytes", (double)resources.memory.used_bytes, "bytes"));
    metrics.push_back(create_metric("memory_available_bytes", (double)resources.memory.available_bytes, "bytes"));
}

void system_resource_collector::add_disk_metrics(std::vector<metric>& metrics, const system_resources& resources) {
    (void)metrics; (void)resources;
    // TODO using empty for now or implementing if needed
}

void system_resource_collector::add_network_metrics(std::vector<metric>& metrics, const system_resources& resources) {
    (void)metrics; (void)resources;
    // TODO
}

void system_resource_collector::add_process_metrics(std::vector<metric>& metrics, const system_resources& resources) {
    metrics.push_back(create_metric("process_count", (double)resources.process.count));
}

// -----------------------------------------------------------------------------
// resource_threshold_monitor Implementation
// -----------------------------------------------------------------------------

resource_threshold_monitor::resource_threshold_monitor(const thresholds& config) 
    : config_(config) {}

std::vector<resource_threshold_monitor::alert> resource_threshold_monitor::check_thresholds(const system_resources& resources) {
    std::vector<alert> alerts;
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    check_cpu_usage(alerts, resources);
    check_memory_usage(alerts, resources);
    // others...
    
    return alerts;
}

void resource_threshold_monitor::update_thresholds(const thresholds& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;
}

resource_threshold_monitor::thresholds resource_threshold_monitor::get_thresholds() const {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

std::vector<resource_threshold_monitor::alert> resource_threshold_monitor::get_alert_history(size_t max_count) const {
    (void)max_count;
    std::lock_guard<std::mutex> lock(history_mutex_);
    return alert_history_;
}

void resource_threshold_monitor::clear_history() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    alert_history_.clear();
}

void resource_threshold_monitor::check_cpu_usage(std::vector<alert>& alerts, const system_resources& resources) {
    if (resources.cpu.usage_percent >= config_.cpu_usage_critical) {
        add_alert(alerts, "cpu", alert::severity::critical, resources.cpu.usage_percent, config_.cpu_usage_critical, "CPU usage critical");
    } else if (resources.cpu.usage_percent >= config_.cpu_usage_warn) {
        add_alert(alerts, "cpu", alert::severity::warning, resources.cpu.usage_percent, config_.cpu_usage_warn, "CPU usage warning");
    }
}

void resource_threshold_monitor::check_memory_usage(std::vector<alert>& alerts, const system_resources& resources) {
    if (resources.memory.usage_percent >= config_.memory_usage_critical) {
        add_alert(alerts, "memory", alert::severity::critical, resources.memory.usage_percent, config_.memory_usage_critical, "Memory usage critical");
    } else if (resources.memory.usage_percent >= config_.memory_usage_warn) {
        add_alert(alerts, "memory", alert::severity::warning, resources.memory.usage_percent, config_.memory_usage_warn, "Memory usage warning");
    }
}

void resource_threshold_monitor::check_disk_usage(std::vector<alert>& alerts, const system_resources& resources) {
    (void)alerts; (void)resources;
    // TODO
}

void resource_threshold_monitor::check_swap_usage(std::vector<alert>& alerts, const system_resources& resources) {
    (void)alerts; (void)resources;
    // TODO
}

void resource_threshold_monitor::add_alert(std::vector<alert>& alerts, const std::string& resource,
                                         alert::severity level, double value, double threshold, const std::string& message) {
    alert a;
    a.resource = resource;
    a.level = level;
    a.current_value = value;
    a.threshold = threshold;
    a.message = message;
    a.timestamp = std::chrono::steady_clock::now();
    alerts.push_back(a);
    
    std::lock_guard<std::mutex> lock(history_mutex_);
    alert_history_.push_back(a);
    if (alert_history_.size() > max_history_size_) {
        alert_history_.erase(alert_history_.begin());
    }
}

} } // namespace monitoring_system
