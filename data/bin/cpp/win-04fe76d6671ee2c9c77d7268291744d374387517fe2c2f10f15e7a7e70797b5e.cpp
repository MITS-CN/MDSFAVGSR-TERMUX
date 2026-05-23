#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

// 递归删除目录内容，最后删除目录本身
bool remove_recursive(const fs::path& dir, bool verbose) {
    if (!fs::exists(dir)) {
        std::cerr << "错误: 路径不存在: " << dir << std::endl;
        return false;
    }
    if (!fs::is_directory(dir)) {
        std::cerr << "错误: 不是目录: " << dir << std::endl;
        return false;
    }

    bool success = true;

    // 先收集目录下的所有条目，避免迭代器失效
    std::vector<fs::path> entries;
    try {
        for (const auto& entry : fs::directory_iterator(dir)) {
            entries.push_back(entry.path());
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "无法读取目录 " << dir << ": " << e.what() << std::endl;
        return false;
    }

    // 遍历并删除每个条目
    for (const auto& path : entries) {
        try {
            if (fs::is_directory(path) && !fs::is_symlink(path)) {
                // 是目录（非符号链接）→ 递归删除其内容
                if (!remove_recursive(path, verbose)) {
                    success = false;
                }
                // 删除空目录本身
                if (fs::remove(path)) {
                    if (verbose) std::cout << "删除目录: " << path << std::endl;
                } else {
                    std::cerr << "删除目录失败: " << path << std::endl;
                    success = false;
                }
            } else {
                // 普通文件或符号链接 → 直接删除
                if (fs::remove(path)) {
                    if (verbose) std::cout << "删除文件: " << path << std::endl;
                } else {
                    std::cerr << "删除文件失败: " << path << std::endl;
                    success = false;
                }
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "操作失败: " << path << " - " << e.what() << std::endl;
            success = false;
        }
    }

    return success;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "用法: " << argv[0] << " <目录路径> [-v]" << std::endl;
        std::cerr << "  -v    显示详细删除信息" << std::endl;
        return 1;
    }

    fs::path target = argv[1];
    bool verbose = false;

    if (argc >= 3 && std::string(argv[2]) == "-v") {
        verbose = true;
    }

    if (!fs::exists(target)) {
        std::cerr << "错误: 路径不存在: " << target << std::endl;
        return 1;
    }

    if (!fs::is_directory(target)) {
        std::cerr << "错误: 指定路径不是目录: " << target << std::endl;
        return 1;
    }

    if (remove_recursive(target, verbose)) {
        // 最后删除顶层目录本身
        try {
            if (fs::remove(target)) {
                if (verbose) std::cout << "删除顶层目录: " << target << std::endl;
                std::cout << "删除完成。" << std::endl;
            } else {
                std::cerr << "删除顶层目录失败: " << target << std::endl;
                return 1;
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "删除顶层目录失败: " << e.what() << std::endl;
            return 1;
        }
        return 0;
    } else {
        std::cerr << "删除过程中发生错误，请检查权限或手动清理。" << std::endl;
        return 1;
    }
}