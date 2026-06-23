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
#include <sys/mount.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <json.hpp>

using json = nlohmann::json;

// ---------- 工具函数 ----------
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

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

bool is_root() {
    return geteuid() == 0;
}

// 执行命令并返回输出（用于获取信息）
std::string exec_cmd(const std::string& cmd) {
    std::array<char, 256> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
        result += buffer.data();
    pclose(pipe);
    return result;
}

// ---------- 数据结构 ----------
struct DiskInfo {
    std::string name;
    unsigned long long size;
    bool removable;
    std::string model;
    std::string serial;
    std::string pt_type;  // "gpt", "mbr", "unknown"
};

struct PartInfo {
    std::string name;
    unsigned long long start;
    unsigned long long size;  // 扇区数（512B）
    std::string fs;
    std::string uuid;
    std::string part_type;  // 分区类型 GUID 或 MBR 类型码
};

// ---------- 磁盘信息获取 ----------
std::vector<DiskInfo> get_disks() {
    std::vector<DiskInfo> disks;
    std::ifstream proc("/proc/partitions");
    if (!proc.is_open()) return disks;
    std::string line;
    std::getline(proc, line);
    std::getline(proc, line);

    while (std::getline(proc, line)) {
        std::istringstream iss(line);
        int major, minor;
        unsigned long long blocks;
        std::string name;
        if (!(iss >> major >> minor >> blocks >> name)) continue;
        if (name.find("loop") == 0 || name.find("ram") == 0 ||
            name.find("zram") == 0 || name.find("dm-") == 0)
            continue;

        std::string sys_path = "/sys/block/" + name;
        struct stat st;
        if (stat(sys_path.c_str(), &st) != 0) continue;

        DiskInfo disk;
        disk.name = name;
        disk.size = blocks * 1024ULL;

        // removable
        disk.removable = false;
        std::ifstream rem_file(sys_path + "/removable");
        if (rem_file) { int r; if (rem_file >> r) disk.removable = (r == 1); }

        // model
        std::ifstream model_file(sys_path + "/device/model");
        if (model_file) std::getline(model_file, disk.model);
        disk.model = trim(disk.model);
        if (disk.model.empty()) disk.model = "Unknown";

        // serial
        std::ifstream serial_file(sys_path + "/device/serial");
        if (serial_file) std::getline(serial_file, disk.serial);
        disk.serial = trim(disk.serial);
        if (disk.serial.empty()) disk.serial = "Unknown";

        // partition table type (use wipefs or blkid)
        disk.pt_type = "unknown";
        std::string cmd = "wipefs --noheadings /dev/block/" + name + " 2>/dev/null";
        std::string out = exec_cmd(cmd);
        if (out.find("gpt") != std::string::npos) disk.pt_type = "gpt";
        else if (out.find("dos") != std::string::npos) disk.pt_type = "mbr";

        disks.push_back(disk);
    }
    return disks;
}

std::vector<PartInfo> get_partitions(const std::string& disk_name) {
    std::vector<PartInfo> parts;
    std::ifstream proc("/proc/partitions");
    if (!proc.is_open()) return parts;
    std::string line;
    std::getline(proc, line); std::getline(proc, line);
    while (std::getline(proc, line)) {
        std::istringstream iss(line);
        int major, minor;
        unsigned long long blocks;
        std::string name;
        iss >> major >> minor >> blocks >> name;
        if (name.find(disk_name) == 0 && name != disk_name) {
            PartInfo part;
            part.name = name;
            part.size = blocks * 2;  // 扇区数

            std::string start_path = "/sys/block/" + disk_name + "/" + name + "/start";
            std::ifstream sf(start_path);
            if (sf) sf >> part.start; else part.start = 0;

            // 文件系统与 UUID
            std::string cmd = "blkid -o export /dev/block/" + name + " 2>/dev/null";
            std::string blk = exec_cmd(cmd);
            part.fs = "unknown";
            part.uuid = "";
            std::istringstream bss(blk);
            std::string bline;
            while (std::getline(bss, bline)) {
                if (bline.rfind("TYPE=", 0) == 0) part.fs = bline.substr(5);
                else if (bline.rfind("UUID=", 0) == 0) part.uuid = bline.substr(5);
                else if (bline.rfind("PART_ENTRY_TYPE=", 0) == 0) part.part_type = bline.substr(17);
            }
            if (part.fs.empty()) part.fs = "unknown";
            parts.push_back(part);
        }
    }
    return parts;
}

// ---------- 显示函数 ----------
void list_disks() {
    auto disks = get_disks();
    std::cout << "\n磁盘 ###  状态      大小    可用   动态  GPT\n";
    for (size_t i = 0; i < disks.size(); ++i) {
        const auto& d = disks[i];
        std::string size_str;
        if (d.size < 1024 * 1024) size_str = std::to_string(d.size / 1024) + " KB";
        else if (d.size < 1024LL * 1024 * 1024) size_str = std::to_string(d.size / (1024 * 1024)) + " MB";
        else size_str = std::to_string(d.size / (1024LL * 1024 * 1024)) + " GB";
        std::string rem = d.removable ? "可移动" : "固定";
        std::string gpt = (d.pt_type == "gpt") ? "*" : " ";
        std::cout << "磁盘 " << i << "     " << rem << "    " << size_str << "  0 B   " << gpt << "\n";
    }
    std::cout << std::endl;
}

void detail_disk(const std::string& disk_name) {
    auto disks = get_disks();
    const DiskInfo* d = nullptr;
    for (const auto& dk : disks) if (dk.name == disk_name) { d = &dk; break; }
    if (!d) {
        std::cout << "未找到磁盘信息。\n";
        return;
    }
    std::cout << "\n磁盘 " << disk_name << ":\n";
    std::cout << "  型号     : " << d->model << "\n";
    std::cout << "  序列号   : " << d->serial << "\n";
    std::cout << "  大小     : " << d->size << " 字节 (";
    if (d->size < 1024 * 1024) std::cout << d->size / 1024 << " KB";
    else if (d->size < 1024ULL * 1024 * 1024) std::cout << d->size / (1024 * 1024) << " MB";
    else std::cout << d->size / (1024ULL * 1024 * 1024) << " GB";
    std::cout << ")\n";
    std::cout << "  可移除   : " << (d->removable ? "是" : "否") << "\n";
    std::cout << "  分区表   : " << d->pt_type << "\n\n";
}

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
        unsigned long long size_mb = (p.size * 512) / (1024 * 1024);
        unsigned long long offset_mb = (p.start * 512) / (1024 * 1024);
        std::cout << "分区 " << i + 1 << "        " << p.fs << "            " << size_mb << " MB   " << offset_mb << " MB\n";
    }
    std::cout << std::endl;
}

void detail_partition(const PartInfo& p) {
    std::cout << "\n分区 " << p.name << ":\n";
    std::cout << "  文件系统 : " << p.fs << "\n";
    std::cout << "  大小     : " << (p.size * 512) / (1024 * 1024) << " MB\n";
    std::cout << "  起始偏移 : " << p.start << " 扇区 (" << (p.start * 512) / (1024 * 1024) << " MB)\n";
    std::cout << "  UUID     : " << (p.uuid.empty() ? "无" : p.uuid) << "\n";
    if (!p.part_type.empty())
        std::cout << "  分区类型 : " << p.part_type << "\n";
    std::cout << std::endl;
}

// ---------- 操作函数 ----------
bool create_partition(const std::string& disk_name, unsigned long long size_mb) {
    if (!is_root()) {
        std::cerr << "错误：创建分区需要 root 权限。\n";
        return false;
    }
    std::cout << "将使用 parted 创建分区（需要 parted 已安装）。\n";
    std::string cmd = "parted /dev/block/" + disk_name + " mkpart primary " + std::to_string(size_mb) + "MB";
    std::cout << "执行: " << cmd << std::endl;
    int ret = system(cmd.c_str());
    if (ret == 0) {
        std::cout << "分区创建成功，执行 partprobe 或重启使内核重读分区表。\n";
        return true;
    }
    else {
        std::cerr << "分区创建失败。\n";
        return false;
    }
}

bool delete_partition(const std::string& disk_name, const std::string& part_name) {
    if (!is_root()) {
        std::cerr << "错误：删除分区需要 root 权限。\n";
        return false;
    }
    // 提取分区号（假设形如 mmcblk0p1）
    std::string part_num;
    if (part_name.size() > disk_name.size() && part_name.substr(0, disk_name.size()) == disk_name) {
        part_num = part_name.substr(disk_name.size());
        // 去掉可能的前缀 'p'
        if (!part_num.empty() && part_num[0] == 'p') part_num.erase(0, 1);
    }
    if (part_num.empty()) {
        std::cerr << "无法解析分区号。\n";
        return false;
    }
    std::cout << "确认删除分区 /dev/block/" << part_name << " ? (输入 yes 继续): ";
    std::string confirm;
    std::cin >> confirm;
    std::cin.ignore();
    if (confirm != "yes") {
        std::cout << "已取消。\n";
        return false;
    }
    std::string cmd = "parted /dev/block/" + disk_name + " rm " + part_num;
    std::cout << "执行: " << cmd << std::endl;
    int ret = system(cmd.c_str());
    if (ret == 0) {
        std::cout << "分区已删除。\n";
        return true;
    }
    else {
        std::cerr << "删除失败。\n";
        return false;
    }
}

bool clean_disk(const std::string& disk_name) {
    if (!is_root()) {
        std::cerr << "错误：清除磁盘需要 root 权限。\n";
        return false;
    }
    std::cout << "这将清除磁盘 /dev/block/" << disk_name << " 上的所有分区表。\n";
    std::cout << "确认？(输入 yes 继续): ";
    std::string confirm;
    std::cin >> confirm;
    std::cin.ignore();
    if (confirm != "yes") {
        std::cout << "已取消。\n";
        return false;
    }
    std::string cmd = "wipefs --all /dev/block/" + disk_name;
    int ret = system(cmd.c_str());
    if (ret == 0) {
        std::cout << "分区表已清除。\n";
        return true;
    }
    else {
        std::cerr << "清除失败。\n";
        return false;
    }
}

bool format_partition(const std::string& part_dev, const std::string& fs_type, bool quick) {
    if (!is_root()) {
        std::cerr << "错误：格式化需要 root 权限。\n";
        return false;
    }
    std::string cmd;
    if (fs_type == "fat" || fs_type == "vfat")
        cmd = "mkfs.vfat " + part_dev;
    else if (fs_type == "ext4")
        cmd = "mkfs.ext4 " + part_dev;
    else if (fs_type == "ntfs")
        cmd = "mkfs.ntfs " + part_dev;
    else if (fs_type == "exfat")
        cmd = "mkfs.exfat " + part_dev;
    else if (fs_type == "f2fs")
        cmd = "mkfs.f2fs " + part_dev;
    else {
        std::cerr << "不支持的文件系统类型：" << fs_type << "\n";
        return false;
    }
    if (!quick) {
        // 某些文件系统可以加 -c 检查坏块（如 ext4）
        if (fs_type == "ext4") cmd += " -c";
    }
    std::cout << "执行: " << cmd << std::endl;
    int ret = system(cmd.c_str());
    if (ret == 0) {
        std::cout << "格式化成功。\n";
        return true;
    }
    else {
        std::cerr << "格式化失败。\n";
        return false;
    }
}

bool mount_partition(const std::string& part_dev, const std::string& mount_point) {
    if (!is_root()) {
        std::cerr << "错误：挂载需要 root 权限。\n";
        return false;
    }
    // 创建挂载点
    mkdir(mount_point.c_str(), 0755);
    std::string cmd = "mount " + part_dev + " " + mount_point;
    int ret = system(cmd.c_str());
    if (ret == 0) {
        std::cout << "分区已挂载至 " << mount_point << std::endl;
        return true;
    }
    else {
        std::cerr << "挂载失败。\n";
        return false;
    }
}

bool unmount_partition(const std::string& part_dev) {
    if (!is_root()) {
        std::cerr << "错误：卸载需要 root 权限。\n";
        return false;
    }
    std::string cmd = "umount " + part_dev;
    int ret = system(cmd.c_str());
    if (ret == 0) {
        std::cout << "分区已卸载。\n";
        return true;
    }
    else {
        std::cerr << "卸载失败。\n";
        return false;
    }
}

// ---------- 主程序 ----------
int main() {
    // 读取配置
    std::ifstream file("/data/data/com.termux/files/usr/etc/MITS/diskpart/config.json");
    std::string host_temp = "ANDROID";
    std::string copyright_temp = "(c) Microsoft Corporation. MITS Port";
    std::string version_temp = "Microsoft DiskPart 版本 10.0.17763.1 (MITS)";
    if (file) {
        json data;
        file >> data;
        host_temp = data.value("MITS_Diskpart_host", host_temp);
        copyright_temp = data.value("MITS_Diskpart_copyright", copyright_temp);
        version_temp = data.value("MITS_Diskpart_version", version_temp);
    }

    std::cout << version_temp << "\n"
        << copyright_temp << "\n"
        << "在计算机上: " << host_temp << "\n\n";

    std::string current_disk;
    std::string current_part;  // 当前选中的分区名
    int current_part_index = -1;

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
        }
        else if (cmd == "help" || cmd == "?") {
            std::cout << "支持的命令：\n"
                << "  list disk                     - 列出磁盘\n"
                << "  select disk <索引>            - 选择磁盘\n"
                << "  detail disk                   - 显示磁盘详细信息\n"
                << "  list partition                - 列出当前磁盘的分区\n"
                << "  select partition <索引>       - 选择分区\n"
                << "  detail partition              - 显示当前分区详细信息\n"
                << "  create partition primary size=<MB> - 创建主分区（需 root）\n"
                << "  delete partition              - 删除选中的分区（需 root）\n"
                << "  clean                         - 清除磁盘分区表（需 root）\n"
                << "  format fs=<fstype> [quick] [dev=<设备名>] - 格式化分区（需 root，支持 vfat/ext4/ntfs/exfat/f2fs）\n"
                << "  assign [mount=<路径>]         - 挂载当前分区（默认 /mnt/diskpart，需 root）\n"
                << "  remove                        - 卸载当前分区（需 root）\n"
                << "  exit                          - 退出\n";
        }
        else if (cmd == "list" && tokens.size() >= 2) {
            std::string sub = tokens[1];
            if (sub == "disk") {
                list_disks();
            }
            else if (sub == "partition") {
                list_partitions(current_disk);
            }
            else {
                std::cout << "未知子命令。\n";
            }
        }
        else if (cmd == "select" && tokens.size() >= 3) {
            if (tokens[1] == "disk") {
                int idx = std::stoi(tokens[2]);
                auto disks = get_disks();
                if (idx >= 0 && idx < (int)disks.size()) {
                    current_disk = disks[idx].name;
                    current_part.clear();
                    current_part_index = -1;
                    std::cout << "磁盘 " << idx << " 现在是所选磁盘。\n";
                }
                else {
                    std::cout << "索引超出范围。\n";
                }
            }
            else if (tokens[1] == "partition") {
                if (current_disk.empty()) {
                    std::cout << "请先选择磁盘。\n";
                    continue;
                }
                int idx = std::stoi(tokens[2]);
                auto parts = get_partitions(current_disk);
                if (idx >= 1 && idx <= (int)parts.size()) {
                    current_part = parts[idx - 1].name;
                    current_part_index = idx;
                    std::cout << "分区 " << idx << " 现在是所选分区。\n";
                }
                else {
                    std::cout << "分区索引超出范围。\n";
                }
            }
        }
        else if (cmd == "detail") {
            if (tokens.size() >= 2 && tokens[1] == "disk") {
                if (current_disk.empty()) {
                    std::cout << "没有选中磁盘。\n";
                }
                else {
                    detail_disk(current_disk);
                }
            }
            else if (tokens.size() >= 2 && tokens[1] == "partition") {
                if (current_part.empty()) {
                    std::cout << "没有选中分区。\n";
                }
                else {
                    auto parts = get_partitions(current_disk);
                    for (const auto& p : parts) {
                        if (p.name == current_part) {
                            detail_partition(p);
                            break;
                        }
                    }
                }
            }
            else {
                std::cout << "用法: detail disk 或 detail partition\n";
            }
        }
        else if (cmd == "create" && tokens.size() >= 3 && tokens[1] == "partition" && tokens[2] == "primary") {
            if (current_disk.empty()) {
                std::cout << "没有选中磁盘。\n";
                continue;
            }
            unsigned long long size_mb = 0;
            bool found = false;
            for (const auto& tok : tokens) {
                if (tok.find("size=") == 0) {
                    try { size_mb = std::stoull(tok.substr(5)); found = true; }
                    catch (...) {}
                }
            }
            if (!found) {
                std::cout << "必须指定 size=<MB>。\n";
                continue;
            }
            create_partition(current_disk, size_mb);
        }
        else if (cmd == "delete" && tokens.size() >= 2 && tokens[1] == "partition") {
            if (current_part.empty()) {
                std::cout << "没有选中分区。\n";
                continue;
            }
            delete_partition(current_disk, current_part);
            // 删除后重置选择
            current_part.clear();
            current_part_index = -1;
        }
        else if (cmd == "clean") {
            if (current_disk.empty()) {
                std::cout << "没有选中磁盘。\n";
                continue;
            }
            clean_disk(current_disk);
            current_part.clear();
        }
        else if (cmd == "format") {
            std::string fs_type = "vfat";
            bool quick = false;
            std::string target_dev;
            // 如果已选择分区且未指定 dev，默认使用当前分区
            if (!current_part.empty())
                target_dev = "/dev/block/" + current_part;
            for (const auto& tok : tokens) {
                if (tok.find("fs=") == 0) fs_type = tok.substr(3);
                else if (tok == "quick") quick = true;
                else if (tok.find("dev=") == 0) target_dev = tok.substr(4);
            }
            if (target_dev.empty()) {
                std::cout << "没有指定目标分区。请先用 select partition 或指定 dev=/dev/block/xxx\n";
                continue;
            }
            format_partition(target_dev, fs_type, quick);
        }
        else if (cmd == "assign") {
            if (current_part.empty()) {
                std::cout << "没有选中分区。\n";
                continue;
            }
            std::string mount_point = "/mnt/diskpart";
            for (const auto& tok : tokens) {
                if (tok.find("mount=") == 0) mount_point = tok.substr(6);
            }
            mount_partition("/dev/block/" + current_part, mount_point);
        }
        else if (cmd == "remove") {
            if (current_part.empty()) {
                std::cout << "没有选中分区。\n";
                continue;
            }
            unmount_partition("/dev/block/" + current_part);
        }
        else {
            std::cout << "未知命令。输入 help 查看帮助。\n";
        }
    }
    return 0;
}