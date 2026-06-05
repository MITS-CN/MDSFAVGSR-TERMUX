#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <climits>
#include <algorithm>

#ifdef _WIN32
#else
#include <strings.h>
#define strcasecmp strcasecmp
#endif

// ---------- 通配符匹配 (不区分大小写) ----------
static bool wildmatch(const char *pattern, const char *str, bool case_insensitive) {
    while (*pattern) {
        if (*pattern == '*') {
            ++pattern;
            if (!*pattern) return true;
            while (*str) {
                if (wildmatch(pattern, str, case_insensitive)) return true;
                ++str;
            }
            return wildmatch(pattern, str, case_insensitive);
        } else if (*pattern == '?') {
            if (!*str) return false;
            ++pattern; ++str;
        } else {
            char pc = *pattern, sc = *str;
            if (case_insensitive) {
                pc = std::tolower((unsigned char)pc);
                sc = std::tolower((unsigned char)sc);
            }
            if (pc != sc) return false;
            ++pattern; ++str;
        }
    }
    return !*str;
}

// ---------- 判断文件是否可写（用于 /R 选项）----------
static bool is_writable(const std::string &path) {
    return access(path.c_str(), W_OK) == 0;
}

// ---------- 递归收集目标目录中的所有匹配文件 ----------
static void collect_target_files(const std::string &target_dir, const std::string &filename,
                                 bool recurse, std::vector<std::string> &matches) {
    DIR *d = opendir(target_dir.c_str());
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != nullptr) {
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;
        std::string full = target_dir + "/" + name;
        struct stat st;
        if (stat(full.c_str(), &st) == -1) continue;
        if (S_ISDIR(st.st_mode)) {
            if (recurse) {
                collect_target_files(full, filename, recurse, matches);
            }
        } else if (S_ISREG(st.st_mode)) {
            if (wildmatch(filename.c_str(), name.c_str(), true)) {
                matches.push_back(full);
            }
        }
    }
    closedir(d);
}

// ---------- 复制文件 ----------
static bool copy_file(const std::string &src, const std::string &dst) {
    std::ifstream in(src, std::ios::binary);
    if (!in) return false;
    std::ofstream out(dst, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << in.rdbuf();
    return out.good();
}

// ---------- 获取文件修改时间 ----------
static time_t file_mtime(const std::string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) return st.st_mtime;
    return 0;
}

// ---------- 用户提示 ----------
static bool prompt_user(const std::string &action, const std::string &src, const std::string &dst) {
    std::cout << action << " " << src << " -> " << dst << " ? (Y/N) ";
    std::string line;
    if (!std::getline(std::cin, line)) return false;
    return !line.empty() && (line[0] == 'y' || line[0] == 'Y');
}

// ---------- 帮助 ----------
void usage() {
    std::cout <<
        "REPLACE [drive1:][path1]source [drive2:][path2] [/A] [/P] [/R] [/S] [/U] [/W] [/?]\n\n"
        "Description:\n"
        "    Replaces files in the target directory with files from the source.\n\n"
        "    source   File pattern (may include wildcards) to copy from.\n"
        "    target   Directory to copy files to.\n"
        "    /A       Add new files only (do not replace existing files).\n"
        "    /P       Prompt for confirmation before replacing each file.\n"
        "    /R       Replace read-only files as well as unprotected files.\n"
        "    /S       Search subdirectories of the target for files to replace.\n"
        "    /U       Replace only files that are older than the source.\n"
        "    /W       Wait for a key press before starting.\n"
        "    /?       Display this help.\n\n"
        "Note: source can be a full path or a relative path with wildcards.\n"
        "target must be a directory.\n";
}

// ---------- 主入口 ----------
int main(int argc, char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "/?") == 0 || strcmp(argv[1], "--help") == 0) {
        usage();
        return 0;
    }

    bool opt_A = false, opt_P = false, opt_R = false, opt_S = false, opt_U = false, opt_W = false;
    std::string source_arg, target_arg;

    // 解析参数
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg[0] == '/' || arg[0] == '-') {
            for (size_t j = 1; j < arg.size(); ++j) {
                char c = std::toupper((unsigned char)arg[j]);
                if (c == 'A') opt_A = true;
                else if (c == 'P') opt_P = true;
                else if (c == 'R') opt_R = true;
                else if (c == 'S') opt_S = true;
                else if (c == 'U') opt_U = true;
                else if (c == 'W') opt_W = true;
                else if (c == '?') { usage(); return 0; }
                else {
                    std::cerr << "Unknown option: /" << c << "\n";
                    return 1;
                }
            }
        } else {
            if (source_arg.empty()) source_arg = arg;
            else if (target_arg.empty()) target_arg = arg;
            else {
                std::cerr << "Error: Too many file arguments.\n";
                return 1;
            }
        }
    }

    if (source_arg.empty() || target_arg.empty()) {
        std::cerr << "Error: Missing source pattern or target directory.\n";
        usage();
        return 1;
    }

    // 等待用户按键 (模拟插入磁盘)
    if (opt_W) {
        std::cout << "Press any key to continue...\n";
        std::cin.get();
    }

    // 解析源路径：分离目录与文件名模式
    std::string src_dir, src_pattern;
    size_t pos = source_arg.find_last_of("/");
    if (pos == std::string::npos) {
        src_dir = ".";
        src_pattern = source_arg;
    } else {
        src_dir = source_arg.substr(0, pos);
        src_pattern = source_arg.substr(pos + 1);
        if (src_dir.empty()) src_dir = "/";
    }

    // 收集匹配的源文件
    std::vector<std::string> src_files;
    DIR *sd = opendir(src_dir.c_str());
    if (sd == nullptr) {
        std::cerr << "Error: Source directory not found: " << src_dir << "\n";
        return 1;
    }
    struct dirent *ent;
    while ((ent = readdir(sd)) != nullptr) {
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;
        std::string full = src_dir + "/" + name;
        struct stat st;
        if (stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            if (wildmatch(src_pattern.c_str(), name.c_str(), true)) {
                src_files.push_back(full);
            }
        }
    }
    closedir(sd);

    if (src_files.empty()) {
        std::cerr << "No source files match the pattern.\n";
        return 2;
    }

    // 验证目标目录存在
    struct stat tgt_st;
    if (stat(target_arg.c_str(), &tgt_st) != 0 || !S_ISDIR(tgt_st.st_mode)) {
        std::cerr << "Error: Target is not a valid directory: " << target_arg << "\n";
        return 1;
    }

    int replaced = 0, added = 0, skipped = 0;

    // 遍历每个源文件
    for (const auto &src_path : src_files) {
        std::string fname = src_path.substr(src_path.find_last_of("/") + 1);

        // 获取目标中所有同名文件 (考虑 /S 递归)
        std::vector<std::string> targets;
        collect_target_files(target_arg, fname, opt_S, targets);

        // 获取源文件修改时间
        time_t src_mtime = file_mtime(src_path);

        // 决定操作模式
        bool do_replace = false;
        bool do_add     = false;

        if (targets.empty()) {
            // 目标中不存在同名文件
            if (opt_A) {
                do_add = true;
            } else {
                // 默认替换模式不添加新文件，跳过
                skipped++;
                std::cerr << "Skipping " << src_path << " (no matching file in target)\n";
                continue;
            }
        } else {
            // 存在同名文件
            if (opt_A) {
                // /A 模式：不替换已存在文件，跳过
                skipped++;
                std::cerr << "Skipping " << src_path << " (file already exists, /A active)\n";
                continue;
            }
            do_replace = true;
        }

        // 对每个匹配的目标文件进行操作（如果有多个，对每个都执行）
        for (auto tpath : targets) {
            if (do_add) {
                // 添加模式：目标文件路径应为 <target_dir>/fname
                // 但如果是 /S 且未找到，添加到哪里？只添加到目标根目录
                // 这里我们只在目标根目录下添加新文件
                if (tpath.empty() || tpath.find(target_arg) != 0) {
                    tpath = target_arg + "/" + fname;
                }
                // 检查是否真的不存在（避免竞争）
                if (access(tpath.c_str(), F_OK) == 0) {
                    skipped++;
                    std::cerr << "Skipping " << src_path << " (file already exists at " << tpath << ")\n";
                    continue;
                }
            }

            // 如果 /U 且源文件不比目标文件新，跳过
            if (opt_U && !do_add) {
                time_t tgt_mtime = file_mtime(tpath);
                if (src_mtime <= tgt_mtime) {
                    skipped++;
                    std::cerr << "Skipping " << src_path << " (source is not newer than target)\n";
                    continue;
                }
            }

            // 检查目标文件是否为只读
            bool is_readonly = !is_writable(tpath) && access(tpath.c_str(), F_OK) == 0;
            if (is_readonly && !opt_R) {
                std::cerr << "Cannot replace " << tpath << " (file is read-only, use /R to override)\n";
                skipped++;
                continue;
            }

            // 提示确认
            if (opt_P) {
                std::string action = do_add ? "Add" : "Replace";
                if (!prompt_user(action, src_path, tpath)) {
                    skipped++;
                    continue;
                }
            }

            // 执行替换/添加
            bool ok = copy_file(src_path, tpath);
            if (ok) {
                if (do_add) added++;
                else replaced++;
                std::cout << (do_add ? "Added " : "Replaced ") << tpath << "\n";
            } else {
                std::cerr << "Error copying " << src_path << " to " << tpath << "\n";
                skipped++;
            }

            // 如果 /A 模式，只需添加第一个目标（根目录文件），无需检查其他子目录匹配
            if (do_add) break;
        }

        // 如果是替换模式，没有在目标中找到任何文件（且未递归），已在上面处理
        // 如果是添加模式但目标中有文件，也已经处理了跳过
    }

    std::cout << "\nSummary: " << replaced << " replaced, " << added << " added, " << skipped << " skipped.\n";
    return (replaced + added > 0) ? 0 : 1;
}