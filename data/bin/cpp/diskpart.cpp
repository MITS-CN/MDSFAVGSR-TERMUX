#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include </storage/emulated/0/MITS/TEMP/json.hpp>  // 如果头文件在子目录中，可能需要 "nlohmann/json.hpp" 或直接 "json.hpp"

using json = nlohmann::json;

// 简单去除字符串首尾空白
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

// 检查是否 root（有效用户 ID 为 0）
bool is_root() {
    return geteuid() == 0;
}

// 磁盘信息结构
struct DiskInfo {
    std::string name;        // 设备名，如 mmcblk0
    unsigned long long size; // 字节数
    bool removable;          // 是否可移除（如 USB/SD）
};

// 分区信息结构
struct PartInfo {
    std::string name;           // 分区设备名，如 mmcblk0p1
    unsigned long long start;   // 起始扇区
    unsigned long long size;    // 扇区数（512 字节/扇区）
    std::string fs;             // 文件系统类型
};

// 获取所有磁盘（修正版，适配 Android）
std::vector<DiskInfo> get_disks() {
    std::vector<DiskInfo> disks;
    std::ifstream proc("/proc/partitions");
    if (!proc.is_open()) return disks;

    std::string line;
    // 跳过前两行标题
    std::getline(proc, line);
    std::getline(proc, line);

    while (std::getline(proc, line)) {
        std::istringstream iss(line);
        int major, minor;
        unsigned long long blocks;
        std::string name;
        if (!(iss >> major >> minor >> blocks >> name)) continue;

        // 过滤虚拟设备
        if (name.find("loop") == 0 ||
            name.find("ram") == 0 ||
            name.find("zram") == 0 ||
            name.find("dm-") == 0) {
            continue;
        }

        // 判断是否为主磁盘设备：检查 /sys/block/ 下是否存在同名目录
        std::string sys_path = "/sys/block/" + name;
        struct stat st;
        if (stat(sys_path.c_str(), &st) != 0) {
            continue;  // 不存在说明是分区或其他非磁盘设备
        }

        DiskInfo disk;
        disk.name = name;
        disk.size = blocks * 1024ULL;  // /proc/partitions 的 blocks 单位是 1KiB

        // 读取 removable 标志
        disk.removable = false;
        std::string rem_path = "/sys/block/" + name + "/removable";
        std::ifstream rem_file(rem_path);
        if (rem_file.is_open()) {
            int rem;
            if (rem_file >> rem) disk.removable = (rem == 1);
        }

        disks.push_back(disk);
    }
    return disks;
}

// 获取指定磁盘的所有分区
std::vector<PartInfo> get_partitions(const std::string& disk_name) {
    std::vector<PartInfo> parts;
    std::ifstream proc("/proc/partitions");
    if (!proc.is_open()) return parts;

    std::string line;
    std::getline(proc, line);
    std::getline(proc, line);
    while (std::getline(proc, line)) {
        std::istringstream iss(line);
        int major, minor;
        unsigned long long blocks;
        std::string name;
        iss >> major >> minor >> blocks >> name;

        // 分区名以磁盘名开头且不是磁盘本身
        if (name.find(disk_name) == 0 && name != disk_name) {
            PartInfo part;
            part.name = name;
            // blocks 为 1KiB 块数，转换为 512 字节扇区数
            part.size = blocks * 2;

            // 读取起始扇区
            std::string start_path = "/sys/block/" + disk_name + "/" + name + "/start";
            std::ifstream start_file(start_path);
            if (start_file.is_open()) {
                start_file >> part.start;
            } else {
                part.start = 0;
            }
            parts.push_back(part);
        }
    }
    return parts;
}

// 获取分区文件系统类型（调用 blkid，若不存在则返回 unknown）
std::string get_fs_type(const std::string& part_dev) {
    std::string cmd = "blkid -o value -s TYPE /dev/block/" + part_dev + " 2>/dev/null";
    FILE* fp = popen(cmd.c_str(), "r");
    if (!fp) return "unknown";
    char buf[128];
    if (fgets(buf, sizeof(buf), fp) != nullptr) {
        pclose(fp);
        std::string fs = buf;
        fs.erase(fs.find_last_not_of("\n") + 1);
        return fs.empty() ? "unknown" : fs;
    }
    pclose(fp);
    return "unknown";
}

// 列出所有磁盘
void list_disks() {
    auto disks = get_disks();
    std::cout << "\n磁盘 ###  状态      大小    可用   动态  GPT\n";
    for (size_t i = 0; i < disks.size(); ++i) {
        const auto& d = disks[i];
        std::string size_str;
        if (d.size < 1024*1024) size_str = std::to_string(d.size/1024) + " KB";
        else if (d.size < 1024*1024*1024) size_str = std::to_string(d.size/(1024*1024)) + " MB";
        else size_str = std::to_string(d.size/(1024*1024*1024)) + " GB";

        std::string rem = d.removable ? "可移动" : "固定";
        std::cout << "磁盘 " << i << "     " << rem << "    " << size_str << "  0 B   *    \n";
    }
    std::cout << std::endl;
}

// 列出选中磁盘的分区
void list_partitions(const std::string& disk_name) {
    if (disk_name.empty()) {
        std::cout << "没有选中磁盘。请先使用 select disk <n>。\n";
        return;
    }
    auto parts = get_partitions(disk_name);
    if (parts.empty()) {
        std::cout << "该磁盘没有分区。\n";
        return;
    }
    std::cout << "\n分区 ###       类型             大小     偏移\n";
    for (size_t i = 0; i < parts.size(); ++i) {
        const auto& p = parts[i];
        std::string fs = get_fs_type(p.name);
        unsigned long long size_mb = (p.size * 512) / (1024*1024);
        unsigned long long offset_mb = (p.start * 512) / (1024*1024);
        std::cout << "分区 " << i+1 << "        " << fs << "            " << size_mb << " MB   " << offset_mb << " MB\n";
    }
    std::cout << std::endl;
}

// 创建分区（需 root）
bool create_partition(const std::string& disk_name, unsigned long long size_mb) {
    if (!is_root()) {
        std::cerr << "错误：创建分区需要 root 权限。\n";
        return false;
    }
    std::cout << "正在创建分区（此功能需要实际调用 parted，请根据您的设备调整命令）...\n";
    std::string cmd = "parted /dev/block/" + disk_name + " mkpart primary 0% " + std::to_string(size_mb) + "MB";
    std::cout << "执行: " << cmd << std::endl;
    int ret = system(cmd.c_str());
    if (ret == 0) {
        std::cout << "分区创建成功。请执行 partprobe 或重启设备以更新分区表。\n";
        return true;
    } else {
        std::cerr << "分区创建失败。\n";
        return false;
    }
}

// 格式化分区（需 root）
bool format_partition(const std::string& part_name, const std::string& fs_type, bool quick) {
    if (!is_root()) {
        std::cerr << "错误：格式化需要 root 权限。\n";
        return false;
    }
    std::string cmd;
    if (fs_type == "fat" || fs_type == "vfat") {
        cmd = "mkfs.vfat /dev/block/" + part_name;
    } else if (fs_type == "ext4") {
        cmd = "mkfs.ext4 /dev/block/" + part_name;
    } else if (fs_type == "ntfs") {
        cmd = "mkfs.ntfs /dev/block/" + part_name;
    } else {
        std::cerr << "不支持的文件系统类型：" << fs_type << "\n";
        return false;
    }
    if (!quick) {
        // 可添加慢速格式化选项（如 -c 检查坏块）
    }
    std::cout << "执行: " << cmd << std::endl;
    int ret = system(cmd.c_str());
    if (ret == 0) {
        std::cout << "格式化成功。\n";
        return true;
    } else {
        std::cerr << "格式化失败。\n";
        return false;
    }
}

int main() {
    // 读取配置文件（可选）
    std::ifstream file("/data/data/com.termux/files/usr/etc/MITS/config.json");
    std::string host_temp = "ANDROID";
    std::string copyright_temp = "(c) Microsoft Corporation. MITS Port";
    std::string version_temp = "Microsoft DiskPart 版本 10.0.17763.1 (MITS)";
    if (file.is_open()) {
        json data;
        file >> data;
        file.close();
        host_temp = data.value("MITS_Diskpart_host", host_temp);
        copyright_temp = data.value("MITS_Diskpart_copyright", copyright_temp);
        version_temp = data.value("MITS_Diskpart_version", version_temp);
    }

    std::cout << version_temp << "\n"
              << copyright_temp << "\n"
              << "在计算机上: " << host_temp << "\n\n";

    std::string current_disk;  // 当前选中的磁盘名

    while (true) {
        std::cout << "DISKPART> ";
        std::string line;
        std::getline(std::cin, line);
        line = trim(line);
        if (line.empty()) continue;

        auto tokens = split(line, ' ');
        std::string cmd = tokens[0];
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        if (cmd == "exit" || cmd == "quit") {
            break;
        } else if (cmd == "help" || cmd == "?") {
            std::cout << "支持的命令：\n"
                      << "  list disk                - 列出磁盘\n"
                      << "  select disk <索引>       - 选择磁盘（通过 list disk 显示的索引）\n"
                      << "  list partition           - 列出当前磁盘的分区\n"
                      << "  create partition primary size=<MB>  - 创建主分区（需 root）\n"
                      << "  format fs=<fstype> [quick] - 格式化当前分区（需 root，支持 vfat/ext4/ntfs）\n"
                      << "  exit                     - 退出\n";
        } else if (cmd == "list" && tokens.size() >= 2) {
            std::string sub = tokens[1];
            std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);
            if (sub == "disk") {
                list_disks();
            } else if (sub == "partition") {
                list_partitions(current_disk);
            } else {
                std::cout << "未知子命令。使用 'list disk' 或 'list partition'。\n";
            }
        } else if (cmd == "select" && tokens.size() >= 3 && tokens[1] == "disk") {
            int idx;
            try {
                idx = std::stoi(tokens[2]);
            } catch (...) {
                std::cout << "无效的磁盘索引。\n";
                continue;
            }
            auto disks = get_disks();
            if (idx >= 0 && idx < static_cast<int>(disks.size())) {
                current_disk = disks[idx].name;
                std::cout << "磁盘 " << idx << " 现在是所选磁盘。\n";
            } else {
                std::cout << "索引超出范围。\n";
            }
        } else if (cmd == "create" && tokens.size() >= 3 && tokens[1] == "partition" && tokens[2] == "primary") {
            if (current_disk.empty()) {
                std::cout << "没有选中磁盘。请先 select disk。\n";
                continue;
            }
            unsigned long long size_mb = 0;
            bool found = false;
            for (const auto& tok : tokens) {
                if (tok.find("size=") == 0) {
                    std::string val = tok.substr(5);
                    try {
                        size_mb = std::stoull(val);
                        found = true;
                    } catch (...) {}
                    break;
                }
            }
            if (!found) {
                std::cout << "必须指定 size=<MB>。\n";
                continue;
            }
            create_partition(current_disk, size_mb);
        } else if (cmd == "format") {
            // 简单提示，未实现分区选择
            std::cout << "请先使用 select partition 命令（本版本未实现）。直接使用示例：format fs=vfat quick /dev/block/mmcblk0p1\n";
            std::string fs_type = "vfat";
            bool quick = false;
            for (const auto& tok : tokens) {
                if (tok.find("fs=") == 0) {
                    fs_type = tok.substr(3);
                } else if (tok == "quick") {
                    quick = true;
                }
            }
            std::cout << "要格式化，请使用类似命令：mkfs." << fs_type << " /dev/block/" << current_disk << "p1\n";
        } else {
            std::cout << "未知命令。输入 help 查看帮助。\n";
        }
    }

    return 0;
}