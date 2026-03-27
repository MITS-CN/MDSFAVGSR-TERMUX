#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>

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
    std::string name;       // 设备名，如 mmcblk0
    unsigned long long size; // 字节数
    bool removable;          // 是否可移除（如 USB/SD）
};

// 分区信息结构
struct PartInfo {
    std::string name;       // 分区设备名，如 mmcblk0p1
    unsigned long long start; // 起始扇区（从 /proc/partitions 获取）
    unsigned long long size;  // 扇区数（通常 512 字节/扇区）
    std::string fs;           // 文件系统类型（通过 blkid 检测，可选）
};

// 获取所有磁盘（通过 /sys/block）
std::vector<DiskInfo> get_disks() {
    std::vector<DiskInfo> disks;
    DIR* dir = opendir("/sys/block");
    if (!dir) return disks;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR || entry->d_type == DT_LNK) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;

            // 检查是否真的是块设备（存在 /dev/ 下对应文件）
            std::string dev_path = "/dev/" + name;
            struct stat st;
            if (stat(dev_path.c_str(), &st) != 0 || !S_ISBLK(st.st_mode))
                continue;

            DiskInfo disk;
            disk.name = name;

            // 读取设备大小（字节）
            std::string size_path = "/sys/block/" + name + "/size";
            std::ifstream size_file(size_path);
            if (size_file.is_open()) {
                unsigned long long sectors;
                size_file >> sectors;
                disk.size = sectors * 512; // 假设扇区大小 512 字节
                size_file.close();
            } else {
                disk.size = 0;
            }

            // 读取 removable 标志
            std::string rem_path = "/sys/block/" + name + "/removable";
            std::ifstream rem_file(rem_path);
            if (rem_file.is_open()) {
                int rem;
                rem_file >> rem;
                disk.removable = (rem == 1);
                rem_file.close();
            } else {
                disk.removable = false;
            }

            disks.push_back(disk);
        }
    }
    closedir(dir);
    return disks;
}

// 获取指定磁盘的所有分区（通过 /proc/partitions）
std::vector<PartInfo> get_partitions(const std::string& disk_name) {
    std::vector<PartInfo> parts;
    std::ifstream proc("/proc/partitions");
    if (!proc.is_open()) return parts;

    std::string line;
    // 跳过标题行
    std::getline(proc, line);
    while (std::getline(proc, line)) {
        std::istringstream iss(line);
        int major, minor;
        unsigned long long blocks;
        std::string name;
        iss >> major >> minor >> blocks >> name;

        // 分区名应以磁盘名开头，例如 mmcblk0p1 或 mmcblk0 后跟数字
        if (name.find(disk_name) == 0 && name != disk_name) {
            PartInfo part;
            part.name = name;
            part.size = blocks; // blocks 是 1024 字节块？不，/proc/partitions 中单位是 1K？实际上通常是 1024 字节块
            // 但很多系统上 blocks 是 1024 字节块，而 /sys/block/.../size 是 512 字节扇区
            // 为了统一，这里我们使用 /sys/block/.../ 中的信息更准确
            // 我们可以通过 /sys/block/disk_name/part_name/size 获取扇区数，但简单起见，保留 blocks*2 作为 512 字节扇区
            part.size = blocks * 2; // 转换成 512 字节扇区数

            // 尝试获取起始扇区（需要读 /sys/block/.../start）
            std::string start_path = "/sys/block/" + disk_name + "/" + name + "/start";
            std::ifstream start_file(start_path);
            if (start_file.is_open()) {
                start_file >> part.start;
                start_file.close();
            } else {
                part.start = 0;
            }
            parts.push_back(part);
        }
    }
    proc.close();
    return parts;
}

// 获取分区文件系统类型（需要 blkid 命令）
std::string get_fs_type(const std::string& part_dev) {
    std::string cmd = "blkid -o value -s TYPE /dev/" + part_dev + " 2>/dev/null";
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
    std::cout << "\n磁盘 ###  状态      大小    可用   动态  GPT\n"; // 简单模拟
    for (size_t i = 0; i < disks.size(); ++i) {
        const auto& d = disks[i];
        std::string size_str;
        if (d.size < 1024*1024) size_str = std::to_string(d.size/1024) + " KB";
        else if (d.size < 1024*1024*1024) size_str = std::to_string(d.size/(1024*1024)) + " MB";
        else size_str = std::to_string(d.size/(1024*1024*1024)) + " GB";

        std::string rem = d.removable ? "可移动" : "固定";
        std::cout << "磁盘 " << i << "     " << rem << "    " << size_str << "  0 B   *    \n";
        // 这里可用、动态、GPT 列我们简化为固定值
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
        // 尝试获取文件系统类型
        std::string fs = get_fs_type(p.name);
        // 大小（MB）
        unsigned long long size_mb = (p.size * 512) / (1024*1024);
        unsigned long long offset_mb = (p.start * 512) / (1024*1024);
        std::cout << "分区 " << i+1 << "        " << fs << "            " << size_mb << " MB   " << offset_mb << " MB\n";
    }
    std::cout << std::endl;
}

// 创建分区（需 root，调用 parted 或 fdisk？这里简单使用 dd 和 losetup 不实际，我们仅演示调用 parted）
bool create_partition(const std::string& disk_name, unsigned long long size_mb) {
    if (!is_root()) {
        std::cerr << "错误：创建分区需要 root 权限。\n";
        return false;
    }
    // 这里应使用 parted 或 fdisk 命令自动执行，但为简化，我们提示用户并退出
    std::cout << "正在创建分区（此功能需要实际调用 parted，请根据您的设备调整命令）...\n";
    std::string cmd = "parted /dev/" + disk_name + " mkpart primary 0% " + std::to_string(size_mb) + "MB";
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

// 格式化分区（需 root，调用 mkfs）
bool format_partition(const std::string& part_name, const std::string& fs_type, bool quick) {
    if (!is_root()) {
        std::cerr << "错误：格式化需要 root 权限。\n";
        return false;
    }
    std::string cmd;
    if (fs_type == "fat" || fs_type == "vfat") {
        cmd = "mkfs.vfat /dev/" + part_name;
    } else if (fs_type == "ext4") {
        cmd = "mkfs.ext4 /dev/" + part_name;
    } else if (fs_type == "ntfs") {
        cmd = "mkfs.ntfs /dev/" + part_name;
    } else {
        std::cerr << "不支持的文件系统类型：" << fs_type << "\n";
        return false;
    }
    if (!quick) {
        // 对于某些 mkfs，可以添加 -c 检查坏块，但通常不需要
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
    std::cout << "Microsoft DiskPart 版本 10.0 (简化版移植)\n"
              << "版权所有 (C) Microsoft Corporation.\n"
              << "在计算机上: ANDROID\n\n";

    std::string current_disk; // 当前选中的磁盘名，如 mmcblk0
    std::string current_part; // 当前选中的分区名（可选）

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
            // 解析 size=xxx
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
            // 需要先选中分区（简单起见，我们使用 list partition 显示的分区索引，但这里未实现 select partition）
            // 我们简化：format 针对当前磁盘的第一个分区？不安全。
            // 这里我们要求用户输入分区名，但命令解析复杂，我们改为提示用户需要指定分区。
            std::cout << "请先使用 select partition 命令（本版本未实现）。直接使用示例：format fs=vfat quick /dev/block/mmcblk0p1\n";
            // 为演示，我们仅解析 fs=xxx
            std::string fs_type = "vfat";
            bool quick = false;
            for (const auto& tok : tokens) {
                if (tok.find("fs=") == 0) {
                    fs_type = tok.substr(3);
                } else if (tok == "quick") {
                    quick = true;
                }
            }
            // 假设用户知道分区名
            // 这里仅输出提示
            std::cout << "要格式化，请使用类似命令：mkfs." << fs_type << " /dev/block/" << current_disk << "p1\n";
        } else {
            std::cout << "未知命令。输入 help 查看帮助。\n";
        }
    }

    return 0;
}