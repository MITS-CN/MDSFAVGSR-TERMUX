#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cerrno>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <climits>

// ---------- 通配符匹配 （支持 * 和 ?）----------
static bool wildmatch(const char *pattern, const char *str, bool case_insensitive) {
    while (*pattern) {
        if (*pattern == '*') {
            ++pattern;
            if (!*pattern) return true;           // 末尾的 * 匹配一切
            while (*str) {
                if (wildmatch(pattern, str, case_insensitive)) return true;
                ++str;
            }
            return wildmatch(pattern, str, case_insensitive); // 允许 * 匹配空串
        }
        else if (*pattern == '?') {
            if (!*str) return false;
            ++pattern; ++str;
        }
        else {
            char pc = *pattern, sc = *str;
            if (case_insensitive) {
                pc = std::tolower((unsigned char)pc);
                sc = std::tolower((unsigned char)sc);
            }
            if (pc != sc) return false;
            ++pattern; ++str;
        }
    }
    return !*str;  // 模式耗尽时必须同时耗尽字符串
}

// ---------- 字符串分割 ----------
static std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> tokens;
    size_t start = 0, end;
    while ((end = s.find(delim, start)) != std::string::npos) {
        tokens.push_back(s.substr(start, end - start));
        start = end + 1;
    }
    tokens.push_back(s.substr(start));
    return tokens;
}

// ---------- 获取当前工作目录的绝对路径 ----------
static std::string current_dir_abs() {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf)) == nullptr) {
        return ".";
    }
    return buf;
}

// ---------- 使用帮助 ----------
static void usage() {
    std::cerr <<
        "WHERE [/Q] [/F] [/T] pattern [pattern ...]\n\n"
        "Description:\n"
        "    Displays the location of files that match the given search pattern.\n"
        "    Searches in the current directory and the directories listed in PATH.\n\n"
        "    pattern     File name or wildcard (e.g. '*.exe', 'test.?').\n"
        "    /Q          Quiet mode: only return exit code (0 = found, 1 = not found).\n"
        "    /F          Enclose output file names in double quotes.\n"
        "    /T          Display file size and last modification time.\n\n"
        "Examples:\n"
        "    where bash\n"
        "    where *.sh\n"
        "    where /Q /F python\n";
}

// ---------- 主程序 ----------
int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage();
        return 1;
    }

    bool quiet     = false;
    bool quote     = false;
    bool show_time = false;
    int  opt_end   = 1;

    // 解析开关
    for (opt_end = 1; opt_end < argc; ++opt_end) {
        std::string arg(argv[opt_end]);
        if (arg == "/?" || arg == "--help") {
            usage();
            return 0;
        }
        if (arg.size() >= 2 && (arg[0] == '/' || arg[0] == '-')) {
            for (size_t j = 1; j < arg.size(); ++j) {
                switch (std::toupper((unsigned char)arg[j])) {
                    case 'Q': quiet = true; break;
                    case 'F': quote = true; break;
                    case 'T': show_time = true; break;
                    default:
                        std::cerr << "Unknown option: " << arg << "\n";
                        return 1;
                }
            }
        } else {
            break;   // 遇到第一个非选项参数，停止解析开关
        }
    }

    if (opt_end >= argc) {
        std::cerr << "Error: No search patterns specified.\n";
        usage();
        return 1;
    }

    // 收集所有模式
    std::vector<std::string> patterns;
    for (int i = opt_end; i < argc; ++i) {
        patterns.push_back(argv[i]);
    }

    // 构建搜索目录列表：当前目录 + PATH
    std::vector<std::string> dirs;
    dirs.push_back(current_dir_abs());          // 当前目录优先
    const char *path_env = getenv("PATH");
    if (path_env) {
        auto path_dirs = split(path_env, ':');
        for (auto &d : path_dirs) {
            if (d.empty()) d = ".";
            dirs.push_back(d);
        }
    }

    bool any_missing = false;

    // 遍历每个模式
    for (const auto &pattern : patterns) {
        bool found = false;

        for (const auto &dir : dirs) {
            DIR *d = opendir(dir.c_str());
            if (!d) continue;

            struct dirent *ent;
            while ((ent = readdir(d)) != nullptr) {
                if (std::strcmp(ent->d_name, ".") == 0 ||
                    std::strcmp(ent->d_name, "..") == 0)
                    continue;

                // 不区分大小写匹配（Windows 风格）
                if (wildmatch(pattern.c_str(), ent->d_name, true)) {
                    found = true;
                    if (!quiet) {
                        // 拼接完整路径
                        std::string path = (dir == "." || dir.empty())
                                         ? std::string(ent->d_name)
                                         : dir + "/" + ent->d_name;

                        // 转换为绝对路径，便于阅读
                        char abs[PATH_MAX];
                        if (realpath(path.c_str(), abs) != nullptr) {
                            path = abs;
                        }

                        if (quote) std::cout << "\"" << path << "\"";
                        else       std::cout << path;

                        if (show_time) {
                            struct stat st;
                            if (stat(path.c_str(), &st) == 0) {
                                std::cout << "  " << st.st_size;
                                char mbuf[64];
                                struct tm *lt = localtime(&st.st_mtime);
                                if (lt) {
                                    std::strftime(mbuf, sizeof(mbuf),
                                                  "  %Y-%m-%d %H:%M:%S", lt);
                                    std::cout << mbuf;
                                }
                            }
                        }
                        std::cout << "\n";
                    }
                }
            }
            closedir(d);
        }

        if (!found) {
            any_missing = true;
            if (!quiet) {
                std::cerr << "INFO: Could not find files for the given pattern: "
                          << pattern << "\n";
            }
        }
    }

    return any_missing ? 1 : 0;
}