#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <iomanip>
#include <cmath>
#include <unistd.h>
#include <sys/utsname.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/wireless.h>
#include <ctime>

// 执行命令并返回输出（去除末尾换行）
std::string exec_cmd(const char* cmd) {
    char buffer[128];
    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "N/A";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    if (!result.empty() && result.back() == '\n')
        result.pop_back();
    return result;
}

// 读取文件第一行
std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "N/A";
    std::string line;
    std::getline(f, line);
    return line;
}

// 读取文件所有内容
std::string read_all(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "N/A";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// getprop 封装
std::string getprop(const std::string& key) {
    return exec_cmd(("getprop " + key).c_str());
}

// 格式化字节数（自动选择单位）
std::string format_size_kb(unsigned long long kb) {
    if (kb < 1024) return std::to_string(kb) + " KB";
    else if (kb < 1024 * 1024) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << (kb / 1024.0) << " MB";
        return ss.str();
    }
    else {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << (kb / (1024.0 * 1024.0)) << " GB";
        return ss.str();
    }
}

std::string format_size_bytes(unsigned long long bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    else if (bytes < 1024 * 1024) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << " KB";
        return ss.str();
    }
    else if (bytes < 1024ULL * 1024 * 1024) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << (bytes / (1024.0 * 1024)) << " MB";
        return ss.str();
    }
    else {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024 * 1024)) << " GB";
        return ss.str();
    }
}

// 获取主机名
std::string get_hostname() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
        return hostname;
    return read_file("/proc/sys/kernel/hostname");
}

// OS 信息
std::string get_os_info() {
    std::string release = getprop("ro.build.version.release");
    std::string sdk = getprop("ro.build.version.sdk");
    if (release.empty()) release = "未知";
    if (sdk.empty()) sdk = "未知";
    return "Android " + release + " (API " + sdk + ")";
}

// 内核版本
std::string get_kernel_version() {
    struct utsname buf;
    if (uname(&buf) == 0)
        return buf.release;
    return "N/A";
}

// 设备制造商和型号
std::string get_device_model() {
    std::string man = getprop("ro.product.manufacturer");
    std::string mod = getprop("ro.product.model");
    if (man.empty()) man = "未知";
    if (mod.empty()) mod = "未知";
    return man + " " + mod;
}

// 处理器信息（模型、核心数、最大频率、架构）
std::string get_cpu_info() {
    std::string model, architecture;
    int cores = 0;
    std::string max_freq;

    // 从 /proc/cpuinfo 读取
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos)
                    model = line.substr(pos + 2);
            }
            else if (line.find("processor") != std::string::npos) {
                cores++;
            }
            else if (line.find("Hardware") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos && model.empty())
                    model = line.substr(pos + 2); // 对于 ARM 设备，Hardware 字段常用
            }
        }
    }
    if (model.empty()) model = getprop("ro.product.board");

    // 尝试读取最大频率
    for (int cpu = 0; cpu < cores; ++cpu) {
        std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/cpufreq/cpuinfo_max_freq";
        std::string freq = read_file(path);
        if (!freq.empty() && freq != "N/A") {
            try {
                int khz = std::stoi(freq);
                if (khz >= 1000000) {
                    std::stringstream ss;
                    ss << std::fixed << std::setprecision(2) << (khz / 1000000.0) << " GHz";
                    max_freq = ss.str();
                }
                else {
                    max_freq = std::to_string(khz / 1000) + " MHz";
                }
            }
            catch (...) {}
            break;
        }
    }

    // 架构
    struct utsname buf;
    if (uname(&buf) == 0)
        architecture = buf.machine;

    std::string result = model;
    if (cores > 0) result += " (" + std::to_string(cores) + " 核心)";
    if (!max_freq.empty()) result += " @ " + max_freq;
    if (!architecture.empty()) result += " [" + architecture + "]";
    return result;
}

// 引导加载程序版本
std::string get_bootloader_version() {
    std::string bl = getprop("ro.bootloader");
    if (bl.empty()) bl = getprop("ro.boot.bootloader");
    if (bl.empty()) bl = "N/A";
    return bl;
}

// 内存信息
std::pair<std::string, std::string> get_memory_info() {
    std::ifstream meminfo("/proc/meminfo");
    unsigned long long total = 0, available = 0;
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") == 0)
            sscanf(line.c_str(), "MemTotal: %llu kB", &total);
        else if (line.find("MemAvailable:") == 0)
            sscanf(line.c_str(), "MemAvailable: %llu kB", &available);
    }
    return { format_size_kb(total), format_size_kb(available) };
}

// 虚拟内存（交换分区）
std::pair<std::string, std::string> get_swap_info() {
    std::ifstream meminfo("/proc/meminfo");
    unsigned long long total = 0, free = 0;
    std::string line;
    while (std::getline(meminfo, line)) {
        if (line.find("SwapTotal:") == 0)
            sscanf(line.c_str(), "SwapTotal: %llu kB", &total);
        else if (line.find("SwapFree:") == 0)
            sscanf(line.c_str(), "SwapFree: %llu kB", &free);
    }
    return { format_size_kb(total), format_size_kb(free) };
}

// 存储信息（挂载点路径 -> 总大小、可用大小）
std::pair<unsigned long long, unsigned long long> get_storage(const std::string& mount_point) {
    std::string cmd = "df -k " + mount_point + " 2>/dev/null | tail -1";
    std::string out = exec_cmd(cmd.c_str());
    if (out.empty()) return { 0, 0 };
    std::istringstream iss(out);
    std::string fs, blks, used, avail, pct, mp;
    iss >> fs >> blks >> used >> avail >> pct >> mp;
    try {
        return { std::stoull(blks), std::stoull(avail) };
    }
    catch (...) {
        return { 0, 0 };
    }
}

// 电池信息
std::string get_battery_info() {
    std::string cap_file = "/sys/class/power_supply/battery/capacity";
    std::string status_file = "/sys/class/power_supply/battery/status";
    std::string health_file = "/sys/class/power_supply/battery/health";

    // 尝试备用路径（ACPI 或 USB）
    if (access(cap_file.c_str(), F_OK) != 0) {
        cap_file = "/sys/class/power_supply/BAT0/capacity";
        status_file = "/sys/class/power_supply/BAT0/status";
        health_file = "/sys/class/power_supply/BAT0/health";
    }

    std::string cap = read_file(cap_file);
    std::string status = read_file(status_file);
    std::string health = read_file(health_file);

    if (cap == "N/A") return "无电池信息（可能是台式设备）";

    std::string result = cap + "%";
    if (!status.empty() && status != "N/A") result += " (" + status + ")";
    if (!health.empty() && health != "N/A") result += " 健康: " + health;
    return result;
}

// 屏幕分辨率与 DPI
std::string get_display_info() {
    std::string size = exec_cmd("wm size 2>/dev/null");
    std::string density = exec_cmd("wm density 2>/dev/null");

    // 提取值
    if (size.find("Physical size:") == 0)
        size = size.substr(14);
    else if (size.find("Override size:") == 0)
        size = size.substr(14);
    else
        size = "未知";

    if (density.find("Physical density:") == 0)
        density = density.substr(17);
    else if (density.find("Override density:") == 0)
        density = density.substr(17);
    else
        density = "未知";

    return size + " (" + density + " dpi)";
}

// GPU 渲染器
std::string get_gpu_info() {
    // 尝试从 EGL 或直接读取属性
    std::string renderer = getprop("ro.hardware.egl");
    if (renderer.empty()) renderer = getprop("ro.board.platform");
    if (renderer.empty()) {
        // 尝试读取 /sys 下 GPU 相关信息
        std::string gpu_model = read_file("/sys/class/kgsl/kgsl-3d0/gpu_model");
        if (gpu_model != "N/A") renderer = gpu_model;
    }
    if (renderer.empty()) renderer = "未知";
    return renderer;
}

// 网络接口增强（信号强度、默认网关）
std::string get_wifi_signal() {
    // 尝试通过 iw 或 iwconfig 获取信号
    std::string iw = exec_cmd("iw dev wlan0 link 2>/dev/null | grep signal");
    if (!iw.empty()) {
        size_t pos = iw.find("signal:");
        if (pos != std::string::npos) return iw.substr(pos + 7);
    }
    // 备用：通过 iwconfig
    std::string iwconfig = exec_cmd("iwconfig wlan0 2>/dev/null | grep Signal");
    if (!iwconfig.empty()) {
        size_t pos = iwconfig.find("Signal level=");
        if (pos != std::string::npos) return iwconfig.substr(pos + 13);
    }
    return "N/A";
}

std::string get_default_gateway() {
    std::string route = exec_cmd("ip route show default 2>/dev/null | awk '{print $3}'");
    if (route.empty()) route = "N/A";
    return route;
}

// 启动时间（具体日期时间）
std::string get_boot_time() {
    std::ifstream uptime("/proc/uptime");
    double up_secs = 0;
    uptime >> up_secs;
    time_t now = time(nullptr);
    time_t boot = now - (time_t)up_secs;
    struct tm* boot_tm = localtime(&boot);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", boot_tm);
    return std::string(buf);
}

// 系统运行时长
std::string get_uptime() {
    std::ifstream uptime("/proc/uptime");
    double up_secs = 0;
    uptime >> up_secs;
    int days = (int)up_secs / 86400;
    int hours = ((int)up_secs % 86400) / 3600;
    int mins = ((int)up_secs % 3600) / 60;
    int secs = (int)up_secs % 60;
    std::stringstream ss;
    ss << days << " 天 " << hours << " 小时 " << mins << " 分钟 " << secs << " 秒";
    return ss.str();
}

int main() {
    std::cout << "\n系统信息 (System Information for Android)\n";
    std::cout << "==========================================\n\n";

    std::cout << std::left << std::setw(24) << "主机名:" << get_hostname() << std::endl;
    std::cout << std::left << std::setw(24) << "当前用户:" << getenv("USER") << std::endl;
    std::cout << std::left << std::setw(24) << "主目录:" << getenv("HOME") << std::endl;
    std::cout << std::left << std::setw(24) << "OS 名称:" << get_os_info() << std::endl;
    std::cout << std::left << std::setw(24) << "内核版本:" << get_kernel_version() << std::endl;
    std::cout << std::left << std::setw(24) << "设备型号:" << get_device_model() << std::endl;
    std::cout << std::left << std::setw(24) << "处理器:" << get_cpu_info() << std::endl;
    std::cout << std::left << std::setw(24) << "GPU 渲染器:" << get_gpu_info() << std::endl;
    std::cout << std::left << std::setw(24) << "引导加载程序版本:" << get_bootloader_version() << std::endl;
    std::cout << std::left << std::setw(24) << "BIOS 版本:" << get_bootloader_version() << std::endl; // 等同于 BL
    std::cout << std::left << std::setw(24) << "屏幕分辨率:" << get_display_info() << std::endl;
    std::cout << std::left << std::setw(24) << "时区:" << read_file("/etc/timezone") << std::endl;
    std::cout << std::left << std::setw(24) << "语言/区域:" << getprop("persist.sys.locale") << std::endl;

    auto mem = get_memory_info();
    auto swap = get_swap_info();
    std::cout << std::left << std::setw(24) << "物理内存总量:" << mem.first << std::endl;
    std::cout << std::left << std::setw(24) << "可用物理内存:" << mem.second << std::endl;
    std::cout << std::left << std::setw(24) << "虚拟内存: 最大值:" << swap.first << std::endl;
    std::cout << std::left << std::setw(24) << "虚拟内存: 可用:" << swap.second << std::endl;

    // 存储信息
    auto storage_internal = get_storage("/data");
    auto storage_sdcard = get_storage("/sdcard");
    std::cout << std::left << std::setw(24) << "内部存储 (/data):"
        << "总量 " << format_size_kb(storage_internal.first)
        << "，可用 " << format_size_kb(storage_internal.second) << std::endl;
    if (storage_sdcard.first > 0) {
        std::cout << std::left << std::setw(24) << "外部存储 (/sdcard):"
            << "总量 " << format_size_kb(storage_sdcard.first)
            << "，可用 " << format_size_kb(storage_sdcard.second) << std::endl;
    }

    std::cout << std::left << std::setw(24) << "电池状态:" << get_battery_info() << std::endl;

    std::cout << std::left << std::setw(24) << "系统启动时间:" << get_boot_time() << std::endl;
    std::cout << std::left << std::setw(24) << "系统运行时间:" << get_uptime() << std::endl;

    // 网络信息
    std::cout << "\n网络接口:\n";
    struct ifaddrs* ifaddr, * ifa;
    if (getifaddrs(&ifaddr) == 0) {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) continue;
            std::string name = ifa->ifa_name;
            if (name == "lo") continue;
            std::cout << "   接口: " << name << std::endl;

            if (ifa->ifa_addr->sa_family == AF_INET) {
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &((sockaddr_in*)ifa->ifa_addr)->sin_addr, ip, sizeof(ip));
                std::cout << "      IPv4: " << ip << std::endl;
            }
            else if (ifa->ifa_addr->sa_family == AF_INET6) {
                char ip6[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &((sockaddr_in6*)ifa->ifa_addr)->sin6_addr, ip6, sizeof(ip6));
                std::cout << "      IPv6: " << ip6 << std::endl;
            }

            // MAC 地址
            struct ifreq ifr;
            strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
            if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
                unsigned char* mac = (unsigned char*)ifr.ifr_hwaddr.sa_data;
                printf("      MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            }
        }
        close(sock);
        freeifaddrs(ifaddr);
    }

    // Wi-Fi 信号
    std::string wifi_sig = get_wifi_signal();
    if (wifi_sig != "N/A")
        std::cout << "   Wi-Fi 信号: " << wifi_sig << std::endl;

    std::cout << "   默认网关: " << get_default_gateway() << std::endl;

    std::cout << "\n==========================================\n";
    return 0;
}