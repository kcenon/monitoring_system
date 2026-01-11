
#include <kcenon/monitoring/collectors/system_resource_collector.h>
#include <kcenon/monitoring/utils/config_parser.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>

#if defined(__APPLE__) || defined(__linux__)
#include <unistd.h>
#include <sys/statvfs.h>
#include <ifaddrs.h>
#include <net/if.h>
#endif

#ifdef __APPLE__
    #include <libproc.h>
    #include <sys/proc_info.h>
    #include <IOKit/IOKitLib.h>
    #include <IOKit/storage/IOBlockStorageDriver.h>
    #include <CoreFoundation/CoreFoundation.h>
#elif _WIN32
    // Windows headers are already included via system_resource_collector.h
    // Additional headers for performance counters
    #include <pdh.h>
    #pragma comment(lib, "pdh.lib")
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
#if defined(__APPLE__) || defined(__linux__)
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - last_collection_time_).count();
    double seconds = duration > 0 ? duration / 1000000.0 : 1.0;
    // Get disk space usage for root filesystem
    struct statvfs stat;
    if (statvfs("/", &stat) == 0) {
        resources.disk.total_bytes = stat.f_blocks * stat.f_frsize;
        resources.disk.available_bytes = stat.f_bavail * stat.f_frsize;
        resources.disk.used_bytes = resources.disk.total_bytes - (stat.f_bfree * stat.f_frsize);
        if (resources.disk.total_bytes > 0) {
            resources.disk.usage_percent = 100.0 * static_cast<double>(resources.disk.used_bytes) /
                                           static_cast<double>(resources.disk.total_bytes);
        }
    }
#endif

#ifdef __APPLE__
    // Get disk I/O stats using IOKit
    io_iterator_t disk_iter;
    if (IOServiceGetMatchingServices(kIOMainPortDefault,
                                      IOServiceMatching(kIOBlockStorageDriverClass),
                                      &disk_iter) == KERN_SUCCESS) {
        io_object_t disk;
        uint64_t total_read_bytes = 0;
        uint64_t total_write_bytes = 0;
        uint64_t total_read_ops = 0;
        uint64_t total_write_ops = 0;

        while ((disk = IOIteratorNext(disk_iter)) != 0) {
            CFMutableDictionaryRef props = nullptr;
            if (IORegistryEntryCreateCFProperties(disk, &props, kCFAllocatorDefault, 0) == KERN_SUCCESS && props) {
                CFDictionaryRef stats = static_cast<CFDictionaryRef>(
                    CFDictionaryGetValue(props, CFSTR(kIOBlockStorageDriverStatisticsKey)));
                if (stats) {
                    CFNumberRef num;
                    int64_t value;

                    num = static_cast<CFNumberRef>(CFDictionaryGetValue(stats,
                        CFSTR(kIOBlockStorageDriverStatisticsBytesReadKey)));
                    if (num && CFNumberGetValue(num, kCFNumberSInt64Type, &value)) {
                        total_read_bytes += static_cast<uint64_t>(value);
                    }

                    num = static_cast<CFNumberRef>(CFDictionaryGetValue(stats,
                        CFSTR(kIOBlockStorageDriverStatisticsBytesWrittenKey)));
                    if (num && CFNumberGetValue(num, kCFNumberSInt64Type, &value)) {
                        total_write_bytes += static_cast<uint64_t>(value);
                    }

                    num = static_cast<CFNumberRef>(CFDictionaryGetValue(stats,
                        CFSTR(kIOBlockStorageDriverStatisticsReadsKey)));
                    if (num && CFNumberGetValue(num, kCFNumberSInt64Type, &value)) {
                        total_read_ops += static_cast<uint64_t>(value);
                    }

                    num = static_cast<CFNumberRef>(CFDictionaryGetValue(stats,
                        CFSTR(kIOBlockStorageDriverStatisticsWritesKey)));
                    if (num && CFNumberGetValue(num, kCFNumberSInt64Type, &value)) {
                        total_write_ops += static_cast<uint64_t>(value);
                    }
                }
                CFRelease(props);
            }
            IOObjectRelease(disk);
        }
        IOObjectRelease(disk_iter);

        // Calculate rates
        if (last_disk_stats_.read_bytes > 0 && total_read_bytes >= last_disk_stats_.read_bytes) {
            resources.disk.io.read_bytes_per_sec = static_cast<size_t>(
                (total_read_bytes - last_disk_stats_.read_bytes) / seconds);
            resources.disk.io.write_bytes_per_sec = static_cast<size_t>(
                (total_write_bytes - last_disk_stats_.write_bytes) / seconds);
            resources.disk.io.read_ops_per_sec = static_cast<size_t>(
                (total_read_ops - last_disk_stats_.read_ops) / seconds);
            resources.disk.io.write_ops_per_sec = static_cast<size_t>(
                (total_write_ops - last_disk_stats_.write_ops) / seconds);
        }

        last_disk_stats_.read_bytes = total_read_bytes;
        last_disk_stats_.write_bytes = total_write_bytes;
        last_disk_stats_.read_ops = total_read_ops;
        last_disk_stats_.write_ops = total_write_ops;
    }
#elif __linux__
    // Read disk I/O stats from /proc/diskstats
    std::ifstream file("/proc/diskstats");
    std::string line;
    uint64_t total_read_sectors = 0;
    uint64_t total_write_sectors = 0;
    uint64_t total_read_ops = 0;
    uint64_t total_write_ops = 0;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        unsigned int major, minor;
        std::string dev_name;
        uint64_t reads, reads_merged, sectors_read, read_time;
        uint64_t writes, writes_merged, sectors_written, write_time;

        iss >> major >> minor >> dev_name
            >> reads >> reads_merged >> sectors_read >> read_time
            >> writes >> writes_merged >> sectors_written >> write_time;

        // Filter for actual disks (sd*, nvme*, vd*), skip partitions
        if ((dev_name.find("sd") == 0 || dev_name.find("nvme") == 0 || dev_name.find("vd") == 0) &&
            dev_name.find_first_of("0123456789") == dev_name.length() - 1) {
            // Skip partition entries (e.g., sda1, nvme0n1p1)
            continue;
        }
        if (dev_name.find("sd") == 0 || dev_name.find("nvme") == 0 || dev_name.find("vd") == 0) {
            total_read_sectors += sectors_read;
            total_write_sectors += sectors_written;
            total_read_ops += reads;
            total_write_ops += writes;
        }
    }

    // Convert sectors to bytes (512 bytes per sector)
    uint64_t total_read_bytes = total_read_sectors * 512;
    uint64_t total_write_bytes = total_write_sectors * 512;

    // Calculate rates
    if (last_disk_stats_.read_bytes > 0 && total_read_bytes >= last_disk_stats_.read_bytes) {
        resources.disk.io.read_bytes_per_sec = static_cast<size_t>(
            (total_read_bytes - last_disk_stats_.read_bytes) / seconds);
        resources.disk.io.write_bytes_per_sec = static_cast<size_t>(
            (total_write_bytes - last_disk_stats_.write_bytes) / seconds);
        resources.disk.io.read_ops_per_sec = static_cast<size_t>(
            (total_read_ops - last_disk_stats_.read_ops) / seconds);
        resources.disk.io.write_ops_per_sec = static_cast<size_t>(
            (total_write_ops - last_disk_stats_.write_ops) / seconds);
    }

    last_disk_stats_.read_bytes = total_read_bytes;
    last_disk_stats_.write_bytes = total_write_bytes;
    last_disk_stats_.read_ops = total_read_ops;
    last_disk_stats_.write_ops = total_write_ops;
#elif _WIN32
    // Get disk space usage for C: drive
    ULARGE_INTEGER free_bytes_available, total_bytes, total_free_bytes;
    if (GetDiskFreeSpaceExW(L"C:\\", &free_bytes_available, &total_bytes, &total_free_bytes)) {
        resources.disk.total_bytes = static_cast<size_t>(total_bytes.QuadPart);
        resources.disk.available_bytes = static_cast<size_t>(free_bytes_available.QuadPart);
        resources.disk.used_bytes = resources.disk.total_bytes - resources.disk.available_bytes;
        if (resources.disk.total_bytes > 0) {
            resources.disk.usage_percent = 100.0 * static_cast<double>(resources.disk.used_bytes) /
                                           static_cast<double>(resources.disk.total_bytes);
        }
    }

    // Note: Windows disk I/O requires PDH counters which need initialization
    // For now, disk I/O rates are not collected on Windows
    // Future improvement: Use PDH to collect \\PhysicalDisk\\Disk Read Bytes/sec etc.
#endif
}

void system_info_collector::collect_network_stats(system_resources& resources) {
#if defined(__APPLE__) || defined(__linux__)
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - last_collection_time_).count();
    double seconds = duration > 0 ? duration / 1000000.0 : 1.0;
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0) {
        uint64_t total_rx_bytes = 0;
        uint64_t total_tx_bytes = 0;
        uint64_t total_rx_packets = 0;
        uint64_t total_tx_packets = 0;
        uint64_t total_rx_errors = 0;
        uint64_t total_tx_errors = 0;
        uint64_t total_rx_dropped = 0;
        uint64_t total_tx_dropped = 0;

        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) continue;

#ifdef __APPLE__
            if (ifa->ifa_addr->sa_family == AF_LINK) {
                // Skip loopback interface
                if (ifa->ifa_flags & IFF_LOOPBACK) continue;

                struct if_data* if_data = static_cast<struct if_data*>(ifa->ifa_data);
                if (if_data) {
                    total_rx_bytes += if_data->ifi_ibytes;
                    total_tx_bytes += if_data->ifi_obytes;
                    total_rx_packets += if_data->ifi_ipackets;
                    total_tx_packets += if_data->ifi_opackets;
                    total_rx_errors += if_data->ifi_ierrors;
                    total_tx_errors += if_data->ifi_oerrors;
                    total_rx_dropped += if_data->ifi_iqdrops;
                    // macOS doesn't have separate tx drops in if_data
                }
            }
#elif __linux__
            if (ifa->ifa_addr->sa_family == AF_PACKET) {
                // Skip loopback interface
                if (ifa->ifa_flags & IFF_LOOPBACK) continue;

                // On Linux, we need to read from /proc/net/dev for detailed stats
            }
#endif
        }
        freeifaddrs(ifaddr);

#ifdef __linux__
        // Read detailed network stats from /proc/net/dev on Linux
        std::ifstream file("/proc/net/dev");
        std::string line;
        std::getline(file, line); // Skip header line 1
        std::getline(file, line); // Skip header line 2

        while (std::getline(file, line)) {
            std::istringstream iss(line);
            std::string iface;
            uint64_t rx_bytes, rx_packets, rx_errs, rx_drop, rx_fifo, rx_frame, rx_compressed, rx_multicast;
            uint64_t tx_bytes, tx_packets, tx_errs, tx_drop, tx_fifo, tx_colls, tx_carrier, tx_compressed;

            iss >> iface >> rx_bytes >> rx_packets >> rx_errs >> rx_drop >> rx_fifo >> rx_frame >> rx_compressed >> rx_multicast
                >> tx_bytes >> tx_packets >> tx_errs >> tx_drop >> tx_fifo >> tx_colls >> tx_carrier >> tx_compressed;

            // Remove colon from interface name
            if (!iface.empty() && iface.back() == ':') {
                iface.pop_back();
            }

            // Skip loopback
            if (iface == "lo") continue;

            total_rx_bytes += rx_bytes;
            total_tx_bytes += tx_bytes;
            total_rx_packets += rx_packets;
            total_tx_packets += tx_packets;
            total_rx_errors += rx_errs;
            total_tx_errors += tx_errs;
            total_rx_dropped += rx_drop;
            total_tx_dropped += tx_drop;
        }
#endif

        // Calculate rates
        if (last_network_stats_.rx_bytes > 0 && total_rx_bytes >= last_network_stats_.rx_bytes) {
            resources.network.rx_bytes_per_sec = static_cast<size_t>(
                (total_rx_bytes - last_network_stats_.rx_bytes) / seconds);
            resources.network.tx_bytes_per_sec = static_cast<size_t>(
                (total_tx_bytes - last_network_stats_.tx_bytes) / seconds);
            resources.network.rx_packets_per_sec = static_cast<size_t>(
                (total_rx_packets - last_network_stats_.rx_packets) / seconds);
            resources.network.tx_packets_per_sec = static_cast<size_t>(
                (total_tx_packets - last_network_stats_.tx_packets) / seconds);
        }

        resources.network.rx_errors = static_cast<size_t>(total_rx_errors);
        resources.network.tx_errors = static_cast<size_t>(total_tx_errors);
        resources.network.rx_dropped = static_cast<size_t>(total_rx_dropped);
        resources.network.tx_dropped = static_cast<size_t>(total_tx_dropped);

        last_network_stats_.rx_bytes = total_rx_bytes;
        last_network_stats_.tx_bytes = total_tx_bytes;
        last_network_stats_.rx_packets = total_rx_packets;
        last_network_stats_.tx_packets = total_tx_packets;
        last_network_stats_.rx_errors = total_rx_errors;
        last_network_stats_.tx_errors = total_tx_errors;
        last_network_stats_.rx_dropped = total_rx_dropped;
        last_network_stats_.tx_dropped = total_tx_dropped;
    }
#elif _WIN32
    // Windows network stats using GetIfTable
    MIB_IFTABLE* if_table = nullptr;
    ULONG size = 0;

    // First call to get required buffer size
    if (GetIfTable(nullptr, &size, FALSE) == ERROR_INSUFFICIENT_BUFFER) {
        if_table = reinterpret_cast<MIB_IFTABLE*>(malloc(size));
        if (if_table && GetIfTable(if_table, &size, FALSE) == NO_ERROR) {
            uint64_t total_rx_bytes = 0;
            uint64_t total_tx_bytes = 0;
            uint64_t total_rx_packets = 0;
            uint64_t total_tx_packets = 0;
            uint64_t total_rx_errors = 0;
            uint64_t total_tx_errors = 0;
            uint64_t total_rx_dropped = 0;
            uint64_t total_tx_dropped = 0;

            for (DWORD i = 0; i < if_table->dwNumEntries; ++i) {
                MIB_IFROW& row = if_table->table[i];

                // Skip loopback and non-operational interfaces
                if (row.dwType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
                if (row.dwOperStatus != IF_OPER_STATUS_OPERATIONAL) continue;

                total_rx_bytes += row.dwInOctets;
                total_tx_bytes += row.dwOutOctets;
                total_rx_packets += row.dwInUcastPkts + row.dwInNUcastPkts;
                total_tx_packets += row.dwOutUcastPkts + row.dwOutNUcastPkts;
                total_rx_errors += row.dwInErrors;
                total_tx_errors += row.dwOutErrors;
                total_rx_dropped += row.dwInDiscards;
                total_tx_dropped += row.dwOutDiscards;
            }

            // Calculate rates
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - last_collection_time_).count();
            double seconds = duration > 0 ? duration / 1000000.0 : 1.0;

            if (last_network_stats_.rx_bytes > 0 && total_rx_bytes >= last_network_stats_.rx_bytes) {
                resources.network.rx_bytes_per_sec = static_cast<size_t>(
                    (total_rx_bytes - last_network_stats_.rx_bytes) / seconds);
                resources.network.tx_bytes_per_sec = static_cast<size_t>(
                    (total_tx_bytes - last_network_stats_.tx_bytes) / seconds);
                resources.network.rx_packets_per_sec = static_cast<size_t>(
                    (total_rx_packets - last_network_stats_.rx_packets) / seconds);
                resources.network.tx_packets_per_sec = static_cast<size_t>(
                    (total_tx_packets - last_network_stats_.tx_packets) / seconds);
            }

            resources.network.rx_errors = static_cast<size_t>(total_rx_errors);
            resources.network.tx_errors = static_cast<size_t>(total_tx_errors);
            resources.network.rx_dropped = static_cast<size_t>(total_rx_dropped);
            resources.network.tx_dropped = static_cast<size_t>(total_tx_dropped);

            last_network_stats_.rx_bytes = total_rx_bytes;
            last_network_stats_.tx_bytes = total_tx_bytes;
            last_network_stats_.rx_packets = total_rx_packets;
            last_network_stats_.tx_packets = total_tx_packets;
            last_network_stats_.rx_errors = total_rx_errors;
            last_network_stats_.tx_errors = total_tx_errors;
            last_network_stats_.rx_dropped = total_rx_dropped;
            last_network_stats_.tx_dropped = total_tx_dropped;
        }
        free(if_table);
    }
#endif
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
void system_info_collector::collect_windows_cpu_stats(system_resources& resources) {
    FILETIME idle_time, kernel_time, user_time;
    if (!GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        return;
    }

    // Convert FILETIME to uint64_t (100-nanosecond intervals)
    auto filetime_to_uint64 = [](const FILETIME& ft) -> uint64_t {
        return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    };

    uint64_t idle = filetime_to_uint64(idle_time);
    uint64_t kernel = filetime_to_uint64(kernel_time);
    uint64_t user = filetime_to_uint64(user_time);

    // Note: kernel_time includes idle_time on Windows
    uint64_t system = kernel - idle;

    std::lock_guard<std::mutex> lock(stats_mutex_);

    // Calculate deltas from last collection
    uint64_t prev_idle = last_cpu_stats_.idle;
    uint64_t prev_system = last_cpu_stats_.system;
    uint64_t prev_user = last_cpu_stats_.user;

    uint64_t idle_delta = idle - prev_idle;
    uint64_t system_delta = system - prev_system;
    uint64_t user_delta = user - prev_user;
    uint64_t total_delta = idle_delta + system_delta + user_delta;

    if (total_delta > 0 && prev_idle > 0) {
        resources.cpu.idle_percent = 100.0 * static_cast<double>(idle_delta) / total_delta;
        resources.cpu.system_percent = 100.0 * static_cast<double>(system_delta) / total_delta;
        resources.cpu.user_percent = 100.0 * static_cast<double>(user_delta) / total_delta;
        resources.cpu.usage_percent = resources.cpu.system_percent + resources.cpu.user_percent;
    }

    // Store current values for next calculation
    last_cpu_stats_.idle = idle;
    last_cpu_stats_.system = system;
    last_cpu_stats_.user = user;

    // Get processor count
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);
    resources.cpu.count = sys_info.dwNumberOfProcessors;

    // Windows doesn't have load average like Unix systems
    // Leave load average at default (0.0)
}

void system_info_collector::collect_windows_memory_stats(system_resources& resources) {
    MEMORYSTATUSEX mem_status;
    mem_status.dwLength = sizeof(mem_status);

    if (!GlobalMemoryStatusEx(&mem_status)) {
        return;
    }

    resources.memory.total_bytes = static_cast<size_t>(mem_status.ullTotalPhys);
    resources.memory.available_bytes = static_cast<size_t>(mem_status.ullAvailPhys);
    resources.memory.used_bytes = resources.memory.total_bytes - resources.memory.available_bytes;
    resources.memory.usage_percent = static_cast<double>(mem_status.dwMemoryLoad);

    // Swap (Page File) information
    resources.memory.swap.total_bytes = static_cast<size_t>(mem_status.ullTotalPageFile - mem_status.ullTotalPhys);
    uint64_t swap_available = mem_status.ullAvailPageFile > mem_status.ullAvailPhys ?
                              mem_status.ullAvailPageFile - mem_status.ullAvailPhys : 0;
    resources.memory.swap.used_bytes = resources.memory.swap.total_bytes -
                                       static_cast<size_t>(swap_available);

    if (resources.memory.swap.total_bytes > 0) {
        resources.memory.swap.usage_percent = 100.0 * static_cast<double>(resources.memory.swap.used_bytes) /
                                              static_cast<double>(resources.memory.swap.total_bytes);
    }
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
    size_t max_samples = config_parser::get<size_t>(config, "load_history_max_samples", 1000);
    if (max_samples > 0) {
        load_history_ = std::make_unique<load_average_history>(max_samples);
    }

    enable_load_history_ = config_parser::get<bool>(config, "enable_load_history", false);

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
        // CPU metrics
        "cpu_usage_percent", "cpu_user_percent", "cpu_system_percent",
        "load_average_1min", "context_switches_total", "context_switches_per_sec",
        // Memory metrics
        "memory_usage_percent", "memory_used_bytes", "memory_available_bytes",
        // Disk metrics
        "disk_usage_percent", "disk_total_bytes", "disk_used_bytes", "disk_available_bytes",
        "disk_read_bytes_per_sec", "disk_write_bytes_per_sec",
        "disk_read_ops_per_sec", "disk_write_ops_per_sec",
        // Network metrics
        "network_rx_bytes_per_sec", "network_tx_bytes_per_sec",
        "network_rx_packets_per_sec", "network_tx_packets_per_sec",
        "network_rx_errors", "network_tx_errors", "network_rx_dropped", "network_tx_dropped",
        // Process metrics
        "process_count"
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
    // Disk usage
    metrics.push_back(create_metric("disk_usage_percent", resources.disk.usage_percent, "%"));
    metrics.push_back(create_metric("disk_total_bytes", static_cast<double>(resources.disk.total_bytes), "bytes"));
    metrics.push_back(create_metric("disk_used_bytes", static_cast<double>(resources.disk.used_bytes), "bytes"));
    metrics.push_back(create_metric("disk_available_bytes", static_cast<double>(resources.disk.available_bytes), "bytes"));

    // I/O throughput
    metrics.push_back(create_metric("disk_read_bytes_per_sec", static_cast<double>(resources.disk.io.read_bytes_per_sec), "bytes/s"));
    metrics.push_back(create_metric("disk_write_bytes_per_sec", static_cast<double>(resources.disk.io.write_bytes_per_sec), "bytes/s"));
    metrics.push_back(create_metric("disk_read_ops_per_sec", static_cast<double>(resources.disk.io.read_ops_per_sec), "ops/s"));
    metrics.push_back(create_metric("disk_write_ops_per_sec", static_cast<double>(resources.disk.io.write_ops_per_sec), "ops/s"));
}

void system_resource_collector::add_network_metrics(std::vector<metric>& metrics, const system_resources& resources) {
    // Bandwidth
    metrics.push_back(create_metric("network_rx_bytes_per_sec", static_cast<double>(resources.network.rx_bytes_per_sec), "bytes/s"));
    metrics.push_back(create_metric("network_tx_bytes_per_sec", static_cast<double>(resources.network.tx_bytes_per_sec), "bytes/s"));

    // Packets
    metrics.push_back(create_metric("network_rx_packets_per_sec", static_cast<double>(resources.network.rx_packets_per_sec), "pkts/s"));
    metrics.push_back(create_metric("network_tx_packets_per_sec", static_cast<double>(resources.network.tx_packets_per_sec), "pkts/s"));

    // Errors
    metrics.push_back(create_metric("network_rx_errors", static_cast<double>(resources.network.rx_errors)));
    metrics.push_back(create_metric("network_tx_errors", static_cast<double>(resources.network.tx_errors)));
    metrics.push_back(create_metric("network_rx_dropped", static_cast<double>(resources.network.rx_dropped)));
    metrics.push_back(create_metric("network_tx_dropped", static_cast<double>(resources.network.tx_dropped)));
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
