#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>

// 工具函数：去除字符串首尾空白
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// 分割字符串
std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        if (!item.empty())
            tokens.push_back(item);
    }
    return tokens;
}

// 执行命令并返回输出
std::string exec_cmd(const char* cmd) {
    char buffer[128];
    std::string result;
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "";
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
    if (!f.is_open()) return "";
    std::string line;
    std::getline(f, line);
    return line;
}

// 检查是否 root
bool is_root() {
    return geteuid() == 0;
}

// ========== 网络接口相关 ==========

struct InterfaceInfo {
    std::string name;
    std::string ipv4;
    std::string ipv6;
    std::string mac;
    std::string status;  // up/down
    std::string gateway;
    std::string dns;
};

// 获取所有网络接口
std::vector<InterfaceInfo> get_interfaces() {
    std::vector<InterfaceInfo> ifs;
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return ifs;

    // 收集接口名称及IP
    std::map<std::string, InterfaceInfo> if_map;
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;
        std::string name = ifa->ifa_name;
        if (name == "lo") continue;  // 忽略回环

        auto it = if_map.find(name);
        if (it == if_map.end()) {
            InterfaceInfo info;
            info.name = name;
            info.status = (ifa->ifa_flags & IFF_UP) ? "已连接" : "已断开";
            if_map[name] = info;
            it = if_map.find(name);
        }

        if (ifa->ifa_addr->sa_family == AF_INET) {
            char ip[INET_ADDRSTRLEN];
            struct sockaddr_in* sa = (struct sockaddr_in*)ifa->ifa_addr;
            inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
            it->second.ipv4 = ip;
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            char ip6[INET6_ADDRSTRLEN];
            struct sockaddr_in6* sa6 = (struct sockaddr_in6*)ifa->ifa_addr;
            inet_ntop(AF_INET6, &sa6->sin6_addr, ip6, sizeof(ip6));
            it->second.ipv6 = ip6;
        }
    }
    freeifaddrs(ifaddr);

    // 获取MAC地址（通过ioctl）
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        struct ifreq ifr;
        for (auto& pair : if_map) {
            std::strncpy(ifr.ifr_name, pair.first.c_str(), IFNAMSIZ-1);
            if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
                unsigned char* mac = (unsigned char*)ifr.ifr_hwaddr.sa_data;
                char mac_str[18];
                snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                pair.second.mac = mac_str;
            }
        }
        close(sock);
    }

    // 获取网关（从 /proc/net/route）
    std::ifstream route("/proc/net/route");
    if (route.is_open()) {
        std::string line;
        std::getline(route, line);  // 跳过标题
        while (std::getline(route, line)) {
            std::istringstream iss(line);
            std::string iface, dest, gw, flags;
            iss >> iface >> dest >> gw;
            if (dest == "00000000" && gw != "00000000") {  // 默认路由
                struct in_addr addr;
                addr.s_addr = std::stoul(gw, nullptr, 16);
                char* gw_str = inet_ntoa(addr);
                if (if_map.find(iface) != if_map.end()) {
                    if_map[iface].gateway = gw_str;
                }
            }
        }
        route.close();
    }

    // 获取DNS（通过getprop）
    std::string dns1 = exec_cmd("getprop net.dns1");
    std::string dns2 = exec_cmd("getprop net.dns2");
    std::string dns = dns1;
    if (!dns2.empty()) dns += ", " + dns2;

    for (auto& pair : if_map) {
        pair.second.dns = dns;  // 目前所有接口共享DNS（简化）
    }

    for (const auto& pair : if_map) {
        ifs.push_back(pair.second);
    }
    return ifs;
}

// 显示接口配置
void show_interface_config() {
    auto ifs = get_interfaces();
    if (ifs.empty()) {
        std::cout << "没有找到网络接口。\n";
        return;
    }
    std::cout << "\n接口                IP 地址                                 状态\n";
    std::cout << "--------------------------------------------------------------------\n";
    for (const auto& iface : ifs) {
        std::string ip = iface.ipv4;
        if (ip.empty()) ip = iface.ipv6;
        if (ip.empty()) ip = "未分配";
        printf("%-18s %-38s %s\n", iface.name.c_str(), ip.c_str(), iface.status.c_str());
        if (!iface.mac.empty()) {
            std::cout << "   MAC 地址: " << iface.mac << "\n";
        }
        if (!iface.gateway.empty()) {
            std::cout << "   默认网关: " << iface.gateway << "\n";
        }
        if (!iface.dns.empty()) {
            std::cout << "   DNS 服务器: " << iface.dns << "\n";
        }
        std::cout << "\n";
    }
}

// 设置静态IP（需要root）
bool set_static_ip(const std::string& iface, const std::string& ip, const std::string& mask, const std::string& gateway) {
    if (!is_root()) {
        std::cerr << "错误：设置静态IP需要 root 权限。\n";
        return false;
    }
    // 使用 ip 命令配置
    std::string cmd = "ip addr add " + ip + "/" + mask + " dev " + iface + " 2>/dev/null";
    if (system(cmd.c_str()) != 0) {
        std::cerr << "添加IP地址失败。\n";
        return false;
    }
    cmd = "ip route add default via " + gateway + " dev " + iface + " 2>/dev/null";
    system(cmd.c_str());  // 忽略错误（可能已存在）
    std::cout << "已配置 " << iface << " 为静态 IP: " << ip << "\n";
    return true;
}

// 启用DHCP（需要root）
bool set_dhcp(const std::string& iface) {
    if (!is_root()) {
        std::cerr << "错误：启用 DHCP 需要 root 权限。\n";
        return false;
    }
    // 简单做法：刷新IP并启动dhcpcd（如果已安装）
    std::string cmd = "dhcpcd -n " + iface + " 2>/dev/null";
    int ret = system(cmd.c_str());
    if (ret == 0) {
        std::cout << "已为 " << iface << " 启用 DHCP。\n";
        return true;
    } else {
        std::cerr << "DHCP 客户端启动失败。可能需要安装 dhcpcd 或使用其他工具。\n";
        return false;
    }
}

// 显示DNS
void show_dns() {
    std::string dns = exec_cmd("getprop net.dns1");
    if (dns.empty()) {
        std::cout << "未配置 DNS 服务器。\n";
    } else {
        std::cout << "DNS 服务器: " << dns << "\n";
        std::string dns2 = exec_cmd("getprop net.dns2");
        if (!dns2.empty()) std::cout << "            " << dns2 << "\n";
    }
}

// 设置静态DNS（通过setprop，需要root？实测非root也可设置net.dns1，但可能被系统覆盖）
bool set_static_dns(const std::string& dns) {
    std::string cmd = "setprop net.dns1 " + dns;
    int ret = system(cmd.c_str());
    if (ret == 0) {
        std::cout << "DNS 已设置为 " << dns << "\n";
        return true;
    } else {
        std::cerr << "设置 DNS 失败。可能需要 root 权限。\n";
        return false;
    }
}

// 恢复DHCP DNS（删除自定义DNS）
bool set_dhcp_dns() {
    // 清空自定义DNS，让系统从DHCP获取
    system("setprop net.dns1 ''");
    system("setprop net.dns2 ''");
    std::cout << "已恢复 DHCP DNS。\n";
    return true;
}

// ========== WiFi 相关（使用 Android 系统工具）=========

// 扫描WiFi网络
void scan_wifi() {
    std::cout << "正在扫描 WiFi 网络...\n";
    // 触发扫描（可能需要权限）
    system("cmd wifi start-scan >/dev/null 2>&1");
    sleep(2);  // 等待扫描完成

    // 获取扫描结果
    std::string result = exec_cmd("cmd wifi list-scan-results");
    if (result.empty()) {
        std::cout << "未找到 WiFi 网络或扫描失败。\n";
        return;
    }
    std::cout << "\n可用的 WiFi 网络：\n";
    std::cout << result << std::endl;
}

// 连接WiFi（使用Android的cmd wifi命令）
bool connect_wifi(const std::string& ssid, const std::string& password) {
    std::string cmd;
    if (password.empty()) {
        // 开放网络
        cmd = "cmd wifi connect-network \"" + ssid + "\" open";
    } else {
        // WPA2网络
        cmd = "cmd wifi connect-network \"" + ssid + "\" wpa2 \"" + password + "\"";
    }
    std::cout << "正在连接 " << ssid << "...\n";
    std::string result = exec_cmd(cmd.c_str());
    if (result.find("failed") == std::string::npos) {
        std::cout << "连接成功。\n";
        return true;
    } else {
        std::cerr << "连接失败: " << result << "\n";
        return false;
    }
}

// 断开当前WiFi
void disconnect_wifi() {
    system("cmd wifi disconnect");
    std::cout << "已断开 WiFi。\n";
}

// ========== 主程序与命令分发 ==========

// 当前上下文
std::string current_context = ">";  // 例如 "interface ip>", "wlan>"

void print_prompt() {
    std::cout << "netsh" << current_context << " ";
}

bool handle_command(const std::vector<std::string>& tokens) {
    if (tokens.empty()) return true;

    std::string cmd = tokens[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    // 全局命令：exit, help
    if (cmd == "exit" || cmd == "quit") {
        return false;
    }
    if (cmd == "help" || cmd == "?") {
        std::cout << "支持的命令：\n"
                  << "  interface ip show config          - 显示接口配置\n"
                  << "  interface ip set address <name> static <ip> <mask> <gateway>  - 设置静态IP（需root）\n"
                  << "  interface ip set address <name> dhcp                         - 启用DHCP（需root）\n"
                  << "  interface ip show dns              - 显示DNS服务器\n"
                  << "  interface ip set dns <name> static <dns>  - 设置静态DNS\n"
                  << "  interface ip set dns <name> dhcp          - 恢复DHCP DNS\n"
                  << "  wlan show networks                  - 扫描WiFi网络\n"
                  << "  wlan connect <ssid> [password]      - 连接WiFi（密码可选，开放网络不填）\n"
                  << "  wlan disconnect                      - 断开WiFi\n"
                  << "  exit                                 - 退出\n"
                  << "可以使用上下文，例如输入 'interface ip' 进入接口IP配置上下文。\n";
        return true;
    }

    // 处理上下文切换
    if (cmd == "interface" && tokens.size() >= 2 && tokens[1] == "ip") {
        current_context = " interface ip>";
        std::cout << "当前上下文: interface ip\n";
        return true;
    }
    if (cmd == "wlan") {
        current_context = " wlan>";
        std::cout << "当前上下文: wlan\n";
        return true;
    }
    if (cmd == ".." || cmd == "back") {
        current_context = ">";
        std::cout << "返回根上下文。\n";
        return true;
    }

    // 根上下文命令（如果当前不在根，尝试解析）
    if (current_context == ">") {
        std::cerr << "未知命令。输入 help 查看帮助。\n";
        return true;
    }

    // 根据当前上下文处理命令
    if (current_context == " interface ip>") {
        // 解析 interface ip 子命令
        if (tokens.size() >= 2 && tokens[0] == "show" && tokens[1] == "config") {
            show_interface_config();
        } else if (tokens.size() >= 2 && tokens[0] == "show" && tokens[1] == "dns") {
            show_dns();
        } else if (tokens.size() >= 8 && tokens[0] == "set" && tokens[1] == "address" && tokens[3] == "static") {
            // interface ip set address <name> static <ip> <mask> <gateway>
            std::string name = tokens[2];
            std::string ip = tokens[4];
            std::string mask = tokens[5];
            std::string gateway = tokens[6];
            set_static_ip(name, ip, mask, gateway);
        } else if (tokens.size() >= 5 && tokens[0] == "set" && tokens[1] == "address" && tokens[3] == "dhcp") {
            std::string name = tokens[2];
            set_dhcp(name);
        } else if (tokens.size() >= 6 && tokens[0] == "set" && tokens[1] == "dns" && tokens[3] == "static") {
            // interface ip set dns <name> static <dns>
            std::string dns = tokens[4];
            set_static_dns(dns);
        } else if (tokens.size() >= 5 && tokens[0] == "set" && tokens[1] == "dns" && tokens[3] == "dhcp") {
            set_dhcp_dns();
        } else {
            std::cerr << "未知的 interface ip 命令。输入 help 查看帮助。\n";
        }
    } else if (current_context == " wlan>") {
        // 解析 wlan 子命令
        if (tokens.size() >= 2 && tokens[0] == "show" && tokens[1] == "networks") {
            scan_wifi();
        } else if (tokens.size() >= 2 && tokens[0] == "connect") {
            std::string ssid = tokens[1];
            std::string password = (tokens.size() >= 3) ? tokens[2] : "";
            connect_wifi(ssid, password);
        } else if (tokens.size() == 1 && tokens[0] == "disconnect") {
            disconnect_wifi();
        } else {
            std::cerr << "未知的 wlan 命令。输入 help 查看帮助。\n";
        }
    } else {
        std::cerr << "未知上下文。\n";
    }
    return true;
}

int main() {
    std::cout << "Netsh 简化版 for Termux (Android 网络配置工具)\n"
              << "输入 help 查看命令。\n\n";

    std::string line;
    while (true) {
        print_prompt();
        std::getline(std::cin, line);
        line = trim(line);
        if (line.empty()) continue;

        auto tokens = split(line, ' ');
        if (!handle_command(tokens))
            break;
    }
    return 0;
}