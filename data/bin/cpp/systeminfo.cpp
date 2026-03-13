#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/utsname.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/wireless.h>  // 用于获取 Wi-Fi 信号（可选）

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
    // 去除末尾换行
    if (!result.empty() && result.back() == '\n')
        result.pop_back();
    return result;
}

// 读取文件第一行（去除末尾换行）
std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "N/A";
    std::string line;
    std::getline(f, line);
    return line;
}

// 获取 Android 系统属性（通过 getprop 命令）
std::string getprop(const std::string& key) {
    std::string cmd = "getprop " + key;
    return exec_cmd(cmd.c_str());
}

// 获取主机名
std::string get_hostname() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
        return hostname;
    return read_file("/proc/sys/kernel/hostname");
}

// 获取操作系统名称和版本
std::string get_os_info() {
    std::string release = getprop("ro.build.version.release");
    std::string sdk = getprop("ro.build.version.sdk");
    std::string codename = getprop("ro.build.version.codename");
    if (release.empty()) release = "未知";
    if (sdk.empty()) sdk = "未知";
    return "Android " + release + " (API " + sdk + ")";
}

// 获取内核版本
std::string get_kernel_version() {
    struct utsname buf;
    if (uname(&buf) == 0)
        return buf.release;
    return "N/A";
}

// 获取制造商和型号
std::string get_device_model() {
    std::string manufacturer = getprop("ro.product.manufacturer");
    std::string model = getprop("ro.product.model");
    if (manufacturer.empty()) manufacturer = "未知";
    if (model.empty()) model = "未知";
    return manufacturer + " " + model;
}

// 获取处理器信息
std::string get_cpu_info() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (!cpuinfo.is_open()) return "N/A";
    std::string line;
    std::string model;
    int cores = 0;
    std::string max_freq;
    while (std::getline(cpuinfo, line)) {
        if (line.find("model name") != std::string::npos) {
            size_t pos = line.find(':');
            if (pos != std::string::npos) {
                model = line.substr(pos + 2);
            }
        } else if (line.find("processor") != std::string::npos) {
            cores++;
        }
    }
    // 尝试读取最大频率（可能存在多个 policy 文件）
    for (int cpu = 0; cpu < cores; ++cpu) {
        std::string path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/cpufreq/cpuinfo_max_freq";
        std::string freq = read_file(path);
        if (freq != "N/A") {
            int khz = std::stoi(freq);
            if (khz >= 1000000)
                max_freq = std::to_string(khz / 1000) + " GHz";
            else
                max_freq = std::to_string(khz / 1000) + " MHz";
            break;
        }
    }
    std::string result = model;
    if (cores > 0) result += " (" + std::to_string(cores) + " 核心)";
    if (!max_freq.empty()) result += " 最大频率 " + max_freq;
    return result;
}

// 获取内存信息（单位自动转换）
std::string format_memory(unsigned long long kb) {
    if (kb < 1024) return std::to_string(kb) + " KB";
    else if (kb < 1024 * 1024) return std::to_string(kb / 1024.0) + " MB";
    else return std::to_string(kb / (1024.0 * 1024.0)) + " GB";
}

std::pair<std::string, std::string> get_memory_info() {
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) return {"N/A", "N/A"};
    std::string line;
    unsigned long long total = 0, available = 0;
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") == 0) {
            sscanf(line.c_str(), "MemTotal: %llu kB", &total);
        } else if (line.find("MemAvailable:") == 0) {
            sscanf(line.c_str(), "MemAvailable: %llu kB", &available);
        }
    }
    return {format_memory(total), format_memory(available)};
}

// 获取引导加载程序版本（类似 BIOS 版本）
std::string get_bootloader_version() {
    std::string bl = getprop("ro.bootloader");
    if (bl.empty()) bl = getprop("ro.boot.bootloader");
    if (bl.empty()) bl = "N/A";
    return bl;
}

// 获取网络接口信息（名称、IP、MAC）
struct NetInterface {
    std::string name;
    std::string ipv4;
    std::string ipv6;
    std::string mac;
};

std::vector<NetInterface> get_network_interfaces() {
    std::vector<NetInterface> ifs;
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return ifs;

    // 临时存储 MAC 地址（需要 ioctl）
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        freeifaddrs(ifaddr);
        return ifs;
    }

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;

        std::string name = ifa->ifa_name;
        // 跳过回环接口
        if (name == "lo") continue;

        // 查找或创建接口条目
        auto it = std::find_if(ifs.begin(), ifs.end(), [&](const NetInterface& ni) { return ni.name == name; });
        if (it == ifs.end()) {
            ifs.push_back({name, "", "", ""});
            it = ifs.end() - 1;
        }

        // IP 地址
        if (ifa->ifa_addr->sa_family == AF_INET) {
            char ip[INET_ADDRSTRLEN];
            struct sockaddr_in* sa = (struct sockaddr_in*)ifa->ifa_addr;
            inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
            it->ipv4 = ip;
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            char ip6[INET6_ADDRSTRLEN];
            struct sockaddr_in6* sa6 = (struct sockaddr_in6*)ifa->ifa_addr;
            inet_ntop(AF_INET6, &sa6->sin6_addr, ip6, sizeof(ip6));
            it->ipv6 = ip6;
        }
    }

    // 获取 MAC 地址（通过 ioctl SIOCGIFHWADDR）
    struct ifreq ifr;
    for (auto& ni : ifs) {
        std::strncpy(ifr.ifr_name, ni.name.c_str(), IFNAMSIZ-1);
        if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
            unsigned char* mac = (unsigned char*)ifr.ifr_hwaddr.sa_data;
            char mac_str[18];
            snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            ni.mac = mac_str;
        }
    }

    close(sock);
    freeifaddrs(ifaddr);
    return ifs;
}

// 获取系统运行时间
std::string get_uptime() {
    std::ifstream uptime("/proc/uptime");
    if (!uptime.is_open()) return "N/A";
    double up_secs;
    uptime >> up_secs;
    int days = int(up_secs) / 86400;
    int hours = (int(up_secs) % 86400) / 3600;
    int minutes = (int(up_secs) % 3600) / 60;
    int seconds = int(up_secs) % 60;
    char buf[128];
    snprintf(buf, sizeof(buf), "%d 天 %d 小时 %d 分钟 %d 秒", days, hours, minutes, seconds);
    return buf;
}

int main() {
    std::cout << "系统信息 (System Information for Android)\n";
    std::cout << "------------------------------------------\n";

    // 主机名
    std::cout << "主机名:                 " << get_hostname() << std::endl;

    // OS 信息
    std::cout << "OS 名称:                " << get_os_info() << std::endl;
    std::cout << "OS 版本:                内核 " << get_kernel_version() << std::endl;

    // 制造商和型号
    std::cout << "制造商:                 " << getprop("ro.product.manufacturer") << std::endl;
    std::cout << "系统型号:               " << getprop("ro.product.model") << std::endl;

    // 处理器
    std::cout << "处理器:                 " << get_cpu_info() << std::endl;

    // BIOS/引导加载程序版本
    std::cout << "BIOS 版本:               " << get_bootloader_version() << std::endl;

    // 内存
    auto mem = get_memory_info();
    std::cout << "物理内存总量:           " << mem.first << std::endl;
    std::cout << "可用物理内存:           " << mem.second << std::endl;

    // 运行时间
    std::cout << "系统运行时间:           " << get_uptime() << std::endl;

    // 网络接口
    std::cout << "\n网络接口信息:\n";
    auto nets = get_network_interfaces();
    if (nets.empty()) {
        std::cout << "   无活动接口\n";
    } else {
        for (const auto& ni : nets) {
            std::cout << "   " << ni.name << ":\n";
            if (!ni.ipv4.empty()) std::cout << "       IPv4: " << ni.ipv4 << "\n";
            if (!ni.ipv6.empty()) std::cout << "       IPv6: " << ni.ipv6 << "\n";
            if (!ni.mac.empty()) std::cout << "       MAC:  " << ni.mac << "\n";
        }
    }

    // 可以添加更多信息如当前用户名、时区等（可选）
    std::cout << "\n------------------------------------------\n";
    return 0;
}