// BSD 3-Clause License
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "macos_metrics_provider.h"

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <dirent.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/task_info.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_var.h>
#include <spawn.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/statvfs.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

extern "C" {
extern char** environ;
}

namespace kcenon {
namespace monitoring {
namespace platform {

namespace {

// =========================================================================
// Safe command execution using posix_spawn
// =========================================================================

std::string execute_command(const char* path, const std::vector<std::string>& args) {
    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) {
        return {};
    }

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);
    posix_spawn_file_actions_addclose(&actions, pipe_fds[1]);
    // Redirect stderr to /dev/null
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

    std::vector<const char*> argv;
    argv.push_back(path);
    for (const auto& arg : args) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);

    pid_t pid;
    int status = posix_spawn(&pid, path,
                             &actions, nullptr,
                             const_cast<char* const*>(argv.data()),
                             environ);

    posix_spawn_file_actions_destroy(&actions);
    close(pipe_fds[1]);

    std::string output;
    if (status == 0) {
        char buffer[4096];
        ssize_t n;
        while ((n = read(pipe_fds[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[n] = '\0';
            output += buffer;
        }
        waitpid(pid, &status, 0);
    }

    close(pipe_fds[0]);
    return output;
}

// =========================================================================
// SMC Structures and Constants
// =========================================================================

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

uint32_t str_to_key(const char* key_str) {
    return (static_cast<uint32_t>(key_str[0]) << 24) |
           (static_cast<uint32_t>(key_str[1]) << 16) |
           (static_cast<uint32_t>(key_str[2]) << 8) |
           static_cast<uint32_t>(key_str[3]);
}

// SMC connection wrapper
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
            connection_ = 0;
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

    double read_value(uint32_t key) {
        if (!is_valid()) {
            return 0.0;
        }

        smc_param input{};
        smc_param output{};

        input.key = key;
        input.selector = SMC_CMD_READ_KEYINFO;
        input.data32 = 0;

        size_t input_size = sizeof(smc_param);
        size_t output_size = sizeof(smc_param);

        kern_return_t result = IOConnectCallStructMethod(
            connection_, 2, &input, input_size, &output, &output_size);

        if (result != KERN_SUCCESS || output.result.result != 0) {
            return 0.0;
        }

        input.key = key;
        input.selector = SMC_CMD_READ_BYTES;
        input.keyInfo.keyInfo = output.keyInfo.keyInfo;
        input.val.dataSize = output.keyInfo.keyInfo;

        result = IOConnectCallStructMethod(
            connection_, 2, &input, input_size, &output, &output_size);

        if (result != KERN_SUCCESS || output.result.result != 0) {
            return 0.0;
        }

        if (output.result.val.dataSize >= 2) {
            int16_t raw_value = (static_cast<int16_t>(output.result.val.bytes[0]) << 8) |
                                static_cast<int16_t>(output.result.val.bytes[1]);
            return static_cast<double>(raw_value) / 256.0;
        }

        return 0.0;
    }

   private:
    io_connect_t connection_;
    io_service_t service_;
};

static std::unique_ptr<smc_connection> g_smc;
static std::mutex g_smc_mutex;

smc_connection* get_smc() {
    std::lock_guard<std::mutex> lock(g_smc_mutex);
    if (!g_smc) {
        g_smc = std::make_unique<smc_connection>();
    }
    return g_smc.get();
}

// =========================================================================
// Battery Helper Functions
// =========================================================================

struct iokit_battery_data {
    bool found{false};
    std::string name;
    std::string manufacturer;
    std::string device_name;
    std::string serial;
    bool is_present{false};
    bool is_charging{false};
    bool is_charged{false};
    bool is_ac_attached{false};
    int64_t current_capacity{0};
    int64_t max_capacity{0};
    int64_t design_capacity{0};
    int64_t voltage_mv{0};
    int64_t amperage_ma{0};
    int64_t instantaneous_amperage_ma{0};
    int64_t time_to_empty_minutes{-1};
    int64_t time_to_full_minutes{-1};
    int64_t cycle_count{-1};
    int64_t temperature_decikelvin{0};
};

iokit_battery_data get_iokit_battery_data() {
    iokit_battery_data data;

    io_service_t battery_service = IOServiceGetMatchingService(
        kIOMainPortDefault, IOServiceMatching("AppleSmartBattery"));

    if (!battery_service) {
        return data;
    }

    CFMutableDictionaryRef props = nullptr;
    if (IORegistryEntryCreateCFProperties(battery_service, &props,
                                          kCFAllocatorDefault, 0) != KERN_SUCCESS) {
        IOObjectRelease(battery_service);
        return data;
    }

    data.found = true;
    data.is_present = true;

    auto get_string = [&props](const char* key) -> std::string {
        CFStringRef str = static_cast<CFStringRef>(
            CFDictionaryGetValue(props, CFStringCreateWithCString(
                kCFAllocatorDefault, key, kCFStringEncodingUTF8)));
        if (str && CFGetTypeID(str) == CFStringGetTypeID()) {
            char buffer[256];
            if (CFStringGetCString(str, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
                return buffer;
            }
        }
        return "";
    };

    auto get_int64 = [&props](const char* key) -> int64_t {
        CFNumberRef num = static_cast<CFNumberRef>(
            CFDictionaryGetValue(props, CFStringCreateWithCString(
                kCFAllocatorDefault, key, kCFStringEncodingUTF8)));
        if (num && CFGetTypeID(num) == CFNumberGetTypeID()) {
            int64_t value = 0;
            CFNumberGetValue(num, kCFNumberSInt64Type, &value);
            return value;
        }
        return 0;
    };

    auto get_bool = [&props](const char* key) -> bool {
        CFBooleanRef b = static_cast<CFBooleanRef>(
            CFDictionaryGetValue(props, CFStringCreateWithCString(
                kCFAllocatorDefault, key, kCFStringEncodingUTF8)));
        if (b && CFGetTypeID(b) == CFBooleanGetTypeID()) {
            return CFBooleanGetValue(b);
        }
        return false;
    };

    data.manufacturer = get_string("Manufacturer");
    data.device_name = get_string("DeviceName");
    data.name = data.device_name;
    data.serial = get_string("BatterySerialNumber");
    data.is_charging = get_bool("IsCharging");
    data.is_charged = get_bool("FullyCharged");
    data.is_ac_attached = get_bool("ExternalConnected");
    data.current_capacity = get_int64("CurrentCapacity");
    data.max_capacity = get_int64("MaxCapacity");
    data.design_capacity = get_int64("DesignCapacity");
    data.voltage_mv = get_int64("Voltage");
    data.amperage_ma = get_int64("Amperage");
    data.instantaneous_amperage_ma = get_int64("InstantAmperage");
    data.cycle_count = get_int64("CycleCount");
    data.temperature_decikelvin = get_int64("Temperature");

    int64_t time_remaining = get_int64("TimeRemaining");
    if (data.is_charging) {
        data.time_to_full_minutes = time_remaining;
    } else {
        data.time_to_empty_minutes = time_remaining;
    }

    CFRelease(props);
    IOObjectRelease(battery_service);

    return data;
}

// =========================================================================
// Temperature Sensor Keys
// =========================================================================

const std::array<std::pair<std::string, std::pair<const char*, sensor_type>>, 10> SMC_TEMP_KEYS = {{
    {"TC0P", {"CPU Proximity", sensor_type::cpu}},
    {"TC0D", {"CPU Die", sensor_type::cpu}},
    {"TC0H", {"CPU Heatsink", sensor_type::cpu}},
    {"TCXC", {"CPU Core", sensor_type::cpu}},
    {"TCSA", {"CPU System Agent", sensor_type::cpu}},
    {"TG0P", {"GPU Proximity", sensor_type::gpu}},
    {"TG0D", {"GPU Die", sensor_type::gpu}},
    {"TA0P", {"Ambient", sensor_type::ambient}},
    {"TM0P", {"Memory Proximity", sensor_type::motherboard}},
    {"TPCD", {"Platform Controller Hub", sensor_type::motherboard}},
}};

// =========================================================================
// Inode Helper Functions
// =========================================================================

const std::unordered_set<std::string> PSEUDO_FILESYSTEMS = {
    "devfs", "autofs", "volfs", "fdesc", "nullfs", "unionfs", "lifs"};

bool should_skip_filesystem(const std::string& fs_type) {
    return PSEUDO_FILESYSTEMS.find(fs_type) != PSEUDO_FILESYSTEMS.end();
}

// =========================================================================
// GPU Helper Functions
// =========================================================================

constexpr uint16_t VENDOR_NVIDIA = 0x10de;
constexpr uint16_t VENDOR_AMD = 0x1002;
constexpr uint16_t VENDOR_INTEL = 0x8086;
constexpr uint16_t VENDOR_APPLE = 0x106b;

}  // anonymous namespace

// =========================================================================
// macos_metrics_provider Implementation
// =========================================================================

macos_metrics_provider::macos_metrics_provider() = default;

// =========================================================================
// Battery
// =========================================================================

bool macos_metrics_provider::is_battery_available() const {
    if (!battery_checked_) {
        auto data = get_iokit_battery_data();
        battery_available_ = data.found;
        battery_checked_ = true;
    }
    return battery_available_;
}

std::vector<battery_reading> macos_metrics_provider::get_battery_readings() {
    std::vector<battery_reading> readings;

    auto iokit_data = get_iokit_battery_data();
    if (!iokit_data.found) {
        return readings;
    }

    battery_reading reading;
    reading.timestamp = std::chrono::system_clock::now();
    reading.battery_present = iokit_data.is_present;
    reading.metrics_available = true;

    reading.info.id = "InternalBattery-0";
    reading.info.name = iokit_data.name.empty() ? "Internal Battery" : iokit_data.name;
    reading.info.path = "iokit:AppleSmartBattery";
    reading.info.manufacturer = iokit_data.manufacturer;
    reading.info.model = iokit_data.device_name;
    reading.info.serial = iokit_data.serial;
    reading.info.technology = "Li-ion";

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

    if (iokit_data.max_capacity > 0) {
        reading.level_percent = (static_cast<double>(iokit_data.current_capacity) /
                                 iokit_data.max_capacity) * 100.0;
    }

    if (iokit_data.voltage_mv > 0) {
        reading.voltage_volts = static_cast<double>(iokit_data.voltage_mv) / 1000.0;
    }

    int64_t amperage = iokit_data.instantaneous_amperage_ma != 0
                           ? iokit_data.instantaneous_amperage_ma
                           : iokit_data.amperage_ma;
    if (amperage != 0) {
        reading.current_amps = static_cast<double>(amperage) / 1000.0;
    }

    if (reading.voltage_volts > 0.0 && reading.current_amps != 0.0) {
        reading.power_watts = reading.voltage_volts * std::abs(reading.current_amps);
    }

    double nominal_voltage = reading.voltage_volts > 0 ? reading.voltage_volts : 11.4;
    if (iokit_data.current_capacity > 0) {
        reading.current_capacity_wh =
            (static_cast<double>(iokit_data.current_capacity) / 1000.0) * nominal_voltage;
    }
    if (iokit_data.max_capacity > 0) {
        reading.full_charge_capacity_wh =
            (static_cast<double>(iokit_data.max_capacity) / 1000.0) * nominal_voltage;
    }
    if (iokit_data.design_capacity > 0) {
        reading.design_capacity_wh =
            (static_cast<double>(iokit_data.design_capacity) / 1000.0) * nominal_voltage;
    }

    if (reading.design_capacity_wh > 0.0) {
        reading.health_percent =
            (reading.full_charge_capacity_wh / reading.design_capacity_wh) * 100.0;
    }

    if (iokit_data.time_to_empty_minutes > 0) {
        reading.time_to_empty_seconds = iokit_data.time_to_empty_minutes * 60;
    }
    if (iokit_data.time_to_full_minutes > 0) {
        reading.time_to_full_seconds = iokit_data.time_to_full_minutes * 60;
    }

    reading.cycle_count = iokit_data.cycle_count;

    if (iokit_data.temperature_decikelvin > 2500) {
        double kelvin = static_cast<double>(iokit_data.temperature_decikelvin) / 10.0;
        double celsius = kelvin - 273.15;
        if (celsius > -40.0 && celsius < 100.0) {
            reading.temperature_celsius = celsius;
            reading.temperature_available = true;
        }
    }

    readings.push_back(reading);
    return readings;
}

// =========================================================================
// Temperature
// =========================================================================

bool macos_metrics_provider::is_temperature_available() const {
    if (!temperature_checked_) {
        auto* smc = get_smc();
        temperature_available_ = (smc != nullptr && smc->is_valid());
        temperature_checked_ = true;
    }
    return temperature_available_;
}

std::vector<temperature_reading> macos_metrics_provider::get_temperature_readings() {
    std::vector<temperature_reading> readings;

    auto* smc = get_smc();
    if (!smc || !smc->is_valid()) {
        return readings;
    }

    for (const auto& [key_str, info] : SMC_TEMP_KEYS) {
        uint32_t key = str_to_key(key_str.c_str());
        double temp = smc->read_value(key);

        if (temp > 0.0 && temp < 200.0) {
            temperature_reading reading;
            reading.timestamp = std::chrono::system_clock::now();
            reading.sensor.id = key_str;
            reading.sensor.name = info.first;
            reading.sensor.zone_path = key_str;
            reading.sensor.type = info.second;
            reading.temperature_celsius = temp;

            if (reading.sensor.type == sensor_type::cpu) {
                reading.thresholds_available = true;
                reading.critical_threshold_celsius = 105.0;
                reading.warning_threshold_celsius = 90.0;
            } else if (reading.sensor.type == sensor_type::gpu) {
                reading.thresholds_available = true;
                reading.critical_threshold_celsius = 95.0;
                reading.warning_threshold_celsius = 85.0;
            }

            if (reading.thresholds_available) {
                if (reading.critical_threshold_celsius > 0.0 &&
                    reading.temperature_celsius >= reading.critical_threshold_celsius) {
                    reading.is_critical = true;
                }
                if (reading.warning_threshold_celsius > 0.0 &&
                    reading.temperature_celsius >= reading.warning_threshold_celsius) {
                    reading.is_warning = true;
                }
            }

            readings.push_back(reading);
        }
    }

    return readings;
}

// =========================================================================
// Uptime
// =========================================================================

uptime_info macos_metrics_provider::get_uptime() {
    uptime_info info;

    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};

    if (sysctl(mib, 2, &boottime, &len, nullptr, 0) == 0) {
        auto boot_time = std::chrono::system_clock::from_time_t(boottime.tv_sec);
        auto now = std::chrono::system_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - boot_time);

        info.uptime_seconds = uptime.count();
        info.boot_time = boot_time;
        info.available = true;
    }

    return info;
}

// =========================================================================
// Context Switches
// =========================================================================

context_switch_info macos_metrics_provider::get_context_switches() {
    context_switch_info info;
    info.timestamp = std::chrono::system_clock::now();

    struct task_events_info events_info;
    mach_msg_type_number_t count = TASK_EVENTS_INFO_COUNT;

    kern_return_t kr = task_info(mach_task_self(), TASK_EVENTS_INFO,
                                 reinterpret_cast<task_info_t>(&events_info), &count);

    if (kr == KERN_SUCCESS) {
        info.total_switches = events_info.csw;
        info.voluntary_switches = events_info.csw;
        info.involuntary_switches = 0;
        info.available = true;
    }

    return info;
}

// =========================================================================
// File Descriptors
// =========================================================================

fd_info macos_metrics_provider::get_fd_stats() {
    fd_info info;

    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        info.max_fds = static_cast<uint64_t>(rl.rlim_cur);
    }

    DIR* dir = opendir("/dev/fd");
    if (dir != nullptr) {
        uint64_t count = 0;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] != '.') {
                ++count;
            }
        }
        closedir(dir);
        info.open_fds = count > 0 ? count - 1 : 0;
    }

    if (info.max_fds > 0) {
        info.usage_percent = 100.0 * static_cast<double>(info.open_fds) /
                             static_cast<double>(info.max_fds);
    }
    info.available = true;

    return info;
}

// =========================================================================
// Inodes
// =========================================================================

std::vector<inode_info> macos_metrics_provider::get_inode_stats() {
    std::vector<inode_info> result;

    struct statfs* mounts = nullptr;
    int num_mounts = getmntinfo(&mounts, MNT_NOWAIT);

    if (num_mounts <= 0 || mounts == nullptr) {
        return result;
    }

    for (int i = 0; i < num_mounts; ++i) {
        const struct statfs& mount = mounts[i];

        if (should_skip_filesystem(mount.f_fstypename)) {
            continue;
        }

        struct statvfs stat;
        if (statvfs(mount.f_mntonname, &stat) != 0) {
            continue;
        }

        if (stat.f_files == 0) {
            continue;
        }

        inode_info info;
        info.filesystem = mount.f_mntonname;
        info.total_inodes = stat.f_files;
        info.free_inodes = stat.f_ffree;
        info.used_inodes = info.total_inodes - info.free_inodes;

        if (info.total_inodes > 0) {
            info.usage_percent = 100.0 * static_cast<double>(info.used_inodes) /
                                 static_cast<double>(info.total_inodes);
        }
        info.available = true;

        result.push_back(info);
    }

    return result;
}

// =========================================================================
// TCP States
// =========================================================================

tcp_state_info macos_metrics_provider::get_tcp_states() {
    tcp_state_info info;
    info.available = true;

    // Use sysctl to read TCP connection states directly instead of popen("netstat")
    int mib[] = {CTL_NET, PF_INET, IPPROTO_TCP, TCPCTL_PCBLIST};
    size_t len = 0;

    if (sysctl(mib, 4, nullptr, &len, nullptr, 0) < 0) {
        return info;
    }

    std::vector<char> buf(len);
    if (sysctl(mib, 4, buf.data(), &len, nullptr, 0) < 0) {
        return info;
    }

    auto* xig = reinterpret_cast<struct xinpgen*>(buf.data());
    auto* end = reinterpret_cast<struct xinpgen*>(buf.data() + len);

    // Skip the first xinpgen header
    xig = reinterpret_cast<struct xinpgen*>(reinterpret_cast<char*>(xig) + xig->xig_len);

    while (xig < end && xig->xig_len > sizeof(struct xinpgen)) {
        auto* tp = reinterpret_cast<struct xtcpcb*>(xig);
        int state = tp->xt_tp.t_state;

        switch (state) {
            case TCPS_ESTABLISHED: info.established++; break;
            case TCPS_SYN_SENT:    info.syn_sent++; break;
            case TCPS_SYN_RECEIVED: info.syn_recv++; break;
            case TCPS_FIN_WAIT_1:  info.fin_wait1++; break;
            case TCPS_FIN_WAIT_2:  info.fin_wait2++; break;
            case TCPS_TIME_WAIT:   info.time_wait++; break;
            case TCPS_CLOSE_WAIT:  info.close_wait++; break;
            case TCPS_LAST_ACK:    info.last_ack++; break;
            case TCPS_LISTEN:      info.listen++; break;
            case TCPS_CLOSING:     info.closing++; break;
            default: break;
        }

        xig = reinterpret_cast<struct xinpgen*>(reinterpret_cast<char*>(xig) + xig->xig_len);
    }

    info.total = info.established + info.syn_sent + info.syn_recv +
                 info.fin_wait1 + info.fin_wait2 + info.time_wait +
                 info.close_wait + info.last_ack + info.listen + info.closing;

    return info;
}

// =========================================================================
// Socket Buffers
// =========================================================================

socket_buffer_info macos_metrics_provider::get_socket_buffer_stats() {
    socket_buffer_info info;
    info.available = true;

    // Use sysctl to read TCP socket buffer stats directly instead of popen("netstat")
    int mib[] = {CTL_NET, PF_INET, IPPROTO_TCP, TCPCTL_PCBLIST};
    size_t len = 0;

    if (sysctl(mib, 4, nullptr, &len, nullptr, 0) < 0) {
        return info;
    }

    std::vector<char> buf(len);
    if (sysctl(mib, 4, buf.data(), &len, nullptr, 0) < 0) {
        return info;
    }

    auto* xig = reinterpret_cast<struct xinpgen*>(buf.data());
    auto* end = reinterpret_cast<struct xinpgen*>(buf.data() + len);

    // Skip the first xinpgen header
    xig = reinterpret_cast<struct xinpgen*>(reinterpret_cast<char*>(xig) + xig->xig_len);

    while (xig < end && xig->xig_len > sizeof(struct xinpgen)) {
        auto* tp = reinterpret_cast<struct xtcpcb*>(xig);
        auto& so = tp->xt_socket;

        info.rx_buffer_used += so.so_rcv.sb_cc;
        info.tx_buffer_used += so.so_snd.sb_cc;

        xig = reinterpret_cast<struct xinpgen*>(reinterpret_cast<char*>(xig) + xig->xig_len);
    }

    return info;
}

// =========================================================================
// Interrupts
// =========================================================================

std::vector<interrupt_info> macos_metrics_provider::get_interrupt_stats() {
    std::vector<interrupt_info> result;

    mach_port_t host = mach_host_self();
    vm_statistics64_data_t vm_stats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

    if (host_statistics64(host, HOST_VM_INFO64,
                          reinterpret_cast<host_info64_t>(&vm_stats),
                          &count) == KERN_SUCCESS) {
        interrupt_info info;
        info.name = "page_operations";
        info.count = static_cast<uint64_t>(vm_stats.pageins + vm_stats.pageouts);
        info.available = true;
        result.push_back(info);
    }

    return result;
}

// =========================================================================
// Power
// =========================================================================

bool macos_metrics_provider::is_power_available() const {
    if (!power_checked_) {
        auto* smc = get_smc();
        if (smc && smc->is_valid()) {
            power_available_ = true;
        } else {
            auto data = get_iokit_battery_data();
            power_available_ = data.found;
        }
        power_checked_ = true;
    }
    return power_available_;
}

power_info macos_metrics_provider::get_power_info() {
    power_info info;

    auto battery_data = get_iokit_battery_data();
    if (battery_data.found) {
        if (battery_data.is_ac_attached) {
            info.source = "ac";
        } else {
            info.source = "battery";
        }

        if (battery_data.voltage_mv > 0 && battery_data.amperage_ma != 0) {
            double voltage = static_cast<double>(battery_data.voltage_mv) / 1000.0;
            double current = static_cast<double>(std::abs(battery_data.amperage_ma)) / 1000.0;
            info.power_watts = voltage * current;
            info.voltage_volts = voltage;
            info.current_amps = static_cast<double>(battery_data.amperage_ma) / 1000.0;
        }

        info.available = true;
    }

    return info;
}

// =========================================================================
// GPU
// =========================================================================

bool macos_metrics_provider::is_gpu_available() const {
    if (!gpu_checked_) {
        CFMutableDictionaryRef matching = IOServiceMatching(kIOAcceleratorClassName);
        if (matching) {
            io_iterator_t iterator;
            kern_return_t result =
                IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator);
            if (result == KERN_SUCCESS) {
                io_object_t service = IOIteratorNext(iterator);
                gpu_available_ = (service != 0);
                if (service) {
                    IOObjectRelease(service);
                }
                IOObjectRelease(iterator);
            }
        }
        gpu_checked_ = true;
    }
    return gpu_available_;
}

std::vector<gpu_info> macos_metrics_provider::get_gpu_info() {
    std::vector<gpu_info> result;

    CFMutableDictionaryRef matching = IOServiceMatching(kIOAcceleratorClassName);
    if (!matching) {
        return result;
    }

    io_iterator_t iterator;
    kern_return_t kr =
        IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator);
    if (kr != KERN_SUCCESS) {
        return result;
    }

    uint32_t device_index = 0;
    io_object_t service;
    while ((service = IOIteratorNext(iterator)) != 0) {
        CFMutableDictionaryRef properties = nullptr;
        if (IORegistryEntryCreateCFProperties(service, &properties,
                                              kCFAllocatorDefault, 0) == KERN_SUCCESS) {
            gpu_info info;
            info.name = "gpu" + std::to_string(device_index);
            info.available = true;

            CFTypeRef vendor_data = CFDictionaryGetValue(
                properties, CFStringCreateWithCString(
                    kCFAllocatorDefault, "vendor-id", kCFStringEncodingUTF8));

            if (vendor_data && CFGetTypeID(vendor_data) == CFDataGetTypeID()) {
                CFDataRef data = static_cast<CFDataRef>(vendor_data);
                if (CFDataGetLength(data) >= 2) {
                    const uint8_t* bytes = CFDataGetBytePtr(data);
                    uint16_t vendor_id = bytes[0] | (bytes[1] << 8);

                    switch (vendor_id) {
                        case VENDOR_NVIDIA:
                            info.vendor = "NVIDIA";
                            info.name = "NVIDIA GPU";
                            break;
                        case VENDOR_AMD:
                            info.vendor = "AMD";
                            info.name = "AMD GPU";
                            break;
                        case VENDOR_INTEL:
                            info.vendor = "Intel";
                            info.name = "Intel GPU";
                            break;
                        case VENDOR_APPLE:
                            info.vendor = "Apple";
                            info.name = "Apple GPU";
                            break;
                        default:
                            info.vendor = "Unknown";
                            break;
                    }
                }
            }

            CFTypeRef model_value = CFDictionaryGetValue(
                properties, CFStringCreateWithCString(
                    kCFAllocatorDefault, "model", kCFStringEncodingUTF8));

            if (model_value && CFGetTypeID(model_value) == CFStringGetTypeID()) {
                char buffer[256];
                if (CFStringGetCString(static_cast<CFStringRef>(model_value),
                                       buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
                    info.name = buffer;
                }
            }

            auto* smc = get_smc();
            if (smc && smc->is_valid()) {
                double temp = smc->read_value(str_to_key("TG0D"));
                if (temp > 0.0 && temp < 150.0) {
                    info.temperature_celsius = temp;
                }
            }

            result.push_back(info);
            ++device_index;

            CFRelease(properties);
        }
        IOObjectRelease(service);
    }
    IOObjectRelease(iterator);

    return result;
}

// =========================================================================
// Security
// =========================================================================

security_info macos_metrics_provider::get_security_info() {
    security_info info;
    info.available = true;

    // Check firewall state using posix_spawn instead of popen
    std::string fw_output = execute_command(
        "/usr/libexec/ApplicationFirewall/socketfilterfw",
        {"--getglobalstate"});
    if (!fw_output.empty()) {
        info.firewall_enabled = (fw_output.find("enabled") != std::string::npos);
    }

    // Get active session count using posix_spawn instead of popen
    std::string who_output = execute_command("/usr/bin/who", {});
    if (!who_output.empty()) {
        // Count lines in who output
        size_t count = 0;
        for (char c : who_output) {
            if (c == '\n') {
                count++;
            }
        }
        info.active_sessions = count;
    }

    return info;
}

}  // namespace platform
}  // namespace monitoring
}  // namespace kcenon

#endif  // __APPLE__
