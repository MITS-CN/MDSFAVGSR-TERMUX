#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <dirent.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

// 进程信息结构体
struct ProcessInfo {
    int pid;
    std::string name;           // 进程名称
    std::string user;           // 用户名
    char state;                  // 状态 (R, S, D, Z, T)
    unsigned long memory_kb;     // 物理内存 (RSS) KB
    std::string window_title;    // 窗口标题（Termux 中不可用，留空）
    std::vector<std::string> modules; // 加载的模块（仅当 /M 时收集）
    std::vector<std::string> services; // 服务（仅当 /SVC 时收集，Linux 无此概念）
};

// 将字符串转换为小写
std::string toLower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

// 去除字符串首尾空白
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// 获取进程状态 (从 /proc/[pid]/stat 中读取)
char getProcessState(int pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream file(path);
    if (!file.is_open()) return '?';
    std::string line;
    std::getline(file, line);
    file.close();

    // 第三个字段是状态，但进程名可能包含空格，需要跳过括号内的内容
    size_t lparen = line.find('(');
    size_t rparen = line.rfind(')');
    if (lparen == std::string::npos || rparen == std::string::npos) return '?';
    // 状态在右括号之后的下一个字段，跳过空格
    size_t pos = rparen + 1;
    while (pos < line.size() && line[pos] == ' ') ++pos;
    if (pos >= line.size()) return '?';
    return line[pos];
}

// 获取进程名称和内存 (从 /proc/[pid]/status)
bool getProcessStatus(int pid, std::string& name, unsigned long& memory_kb) {
    std::string path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream file(path);
    if (!file.is_open()) return false;

    name.clear();
    memory_kb = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (line.compare(0, 5, "Name:") == 0) {
            name = trim(line.substr(5));
        } else if (line.compare(0, 6, "VmRSS:") == 0) {
            // 格式如 "VmRSS:    1234 kB"
            std::string valStr = line.substr(6);
            char* end;
            memory_kb = std::strtoul(valStr.c_str(), &end, 10);
        }
    }
    file.close();
    return !name.empty();
}

// 获取进程的用户名 (通过 /proc/[pid]/status 中的 Uid 和 getpwuid)
std::string getProcessUser(int pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream file(path);
    if (!file.is_open()) return "?";
    std::string line;
    while (std::getline(file, line)) {
        if (line.compare(0, 4, "Uid:") == 0) {
            std::istringstream iss(line.substr(4));
            unsigned int uid;
            iss >> uid;  // 实际有效 UID 是第一个字段
            struct passwd* pw = getpwuid(uid);
            if (pw) return pw->pw_name;
            else return std::to_string(uid);
        }
    }
    return "?";
}

// 获取进程加载的模块 (从 /proc/[pid]/maps 中提取 .so 文件)
std::vector<std::string> getProcessModules(int pid) {
    std::vector<std::string> modules;
    std::string path = "/proc/" + std::to_string(pid) + "/maps";
    std::ifstream file(path);
    if (!file.is_open()) return modules;

    std::string line;
    std::map<std::string, bool> seen;
    while (std::getline(file, line)) {
        // 查找路径部分，通常以 '/' 开头
        size_t pos = line.find('/');
        if (pos != std::string::npos) {
            std::string modPath = line.substr(pos);
            // 取最后一个 '/' 之后的部分作为模块名（简化）
            size_t slash = modPath.rfind('/');
            if (slash != std::string::npos) {
                std::string modName = modPath.substr(slash + 1);
                if (!modName.empty() && !seen[modName]) {
                    modules.push_back(modName);
                    seen[modName] = true;
                }
            } else {
                if (!modPath.empty() && !seen[modPath]) {
                    modules.push_back(modPath);
                    seen[modPath] = true;
                }
            }
        }
    }
    file.close();
    return modules;
}

// 获取所有进程的 PID 列表
std::vector<int> getAllPids() {
    std::vector<int> pids;
    DIR* dir = opendir("/proc");
    if (!dir) return pids;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            bool isNum = true;
            for (char* p = entry->d_name; *p; ++p) {
                if (!isdigit(*p)) { isNum = false; break; }
            }
            if (isNum) pids.push_back(atoi(entry->d_name));
        }
    }
    closedir(dir);
    return pids;
}

// 收集所有进程的完整信息（可根据需要加载模块或服务）
std::vector<ProcessInfo> collectProcesses(bool needModules, bool needServices) {
    std::vector<ProcessInfo> procs;
    std::vector<int> pids = getAllPids();
    for (int pid : pids) {
        ProcessInfo info;
        info.pid = pid;
        info.state = getProcessState(pid);
        if (!getProcessStatus(pid, info.name, info.memory_kb)) {
            continue; // 跳过无法读取的进程
        }
        info.user = getProcessUser(pid);
        if (needModules) {
            info.modules = getProcessModules(pid);
        }
        // Linux 下无服务概念，留空
        if (needServices) {
            // 可以尝试读取 cgroup 或别的，但这里留空
        }
        procs.push_back(info);
    }
    return procs;
}

// 筛选器条件结构
struct Filter {
    std::string field;
    std::string op;
    std::string value;
};

// 解析筛选器字符串，如 "PID eq 123" 或 "IMAGENAME ne systemd"
bool parseFilter(const std::string& filterStr, Filter& filter) {
    std::istringstream iss(filterStr);
    std::string field, op, value;
    if (!(iss >> field >> op)) return false;
    // 读取剩余部分作为值（可能包含空格）
    std::getline(iss >> std::ws, value);
    // 去除可能的外围引号
    if (!value.empty() && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    filter.field = toLower(field);
    filter.op = toLower(op);
    filter.value = value;
    return true;
}

// 应用筛选器，判断进程是否符合条件
bool matchesFilter(const ProcessInfo& proc, const Filter& filter) {
    if (filter.field == "imagename") {
        std::string name = toLower(proc.name);
        std::string val = toLower(filter.value);
        if (filter.op == "eq") return name == val;
        else if (filter.op == "ne") return name != val;
    } else if (filter.field == "pid") {
        int pidVal = std::atoi(filter.value.c_str());
        if (filter.op == "eq") return proc.pid == pidVal;
        else if (filter.op == "ne") return proc.pid != pidVal;
        else if (filter.op == "gt") return proc.pid > pidVal;
        else if (filter.op == "lt") return proc.pid < pidVal;
        else if (filter.op == "ge") return proc.pid >= pidVal;
        else if (filter.op == "le") return proc.pid <= pidVal;
    } else if (filter.field == "username") {
        std::string user = toLower(proc.user);
        std::string val = toLower(filter.value);
        if (filter.op == "eq") return user == val;
        else if (filter.op == "ne") return user != val;
    } else if (filter.field == "memusage") {
        unsigned long memVal = std::strtoul(filter.value.c_str(), nullptr, 10);
        if (filter.op == "eq") return proc.memory_kb == memVal;
        else if (filter.op == "ne") return proc.memory_kb != memVal;
        else if (filter.op == "gt") return proc.memory_kb > memVal;
        else if (filter.op == "lt") return proc.memory_kb < memVal;
        else if (filter.op == "ge") return proc.memory_kb >= memVal;
        else if (filter.op == "le") return proc.memory_kb <= memVal;
    } else if (filter.field == "status") {
        std::string state(1, proc.state);
        std::string val = toLower(filter.value);
        if (filter.op == "eq") return toLower(state) == val;
        else if (filter.op == "ne") return toLower(state) != val;
    }
    // 其他字段暂不支持，返回 true（不过滤）
    return true;
}

// 格式化输出表格行
void printTableRow(const ProcessInfo& proc, bool verbose, bool showSvc, bool showModules, int maxNameWidth = 25) {
    std::string name = proc.name;
    if (name.length() > 25) name = name.substr(0, 22) + "...";
    std::cout << std::left << std::setw(25) << name
              << std::setw(8) << proc.pid
              << std::setw(16) << proc.user
              << std::setw(8) << "1"; // 会话# 固定为1

    if (verbose) {
        // 状态、CPU时间（模拟）、窗口标题（不可用）
        std::string stateStr(1, proc.state);
        std::string cpuTime = "0:00:00"; // 简化，不收集
        std::string windowTitle = "N/A";
        std::cout << std::setw(12) << std::to_string(proc.memory_kb) + "K"
                  << std::setw(16) << stateStr
                  << std::setw(24) << proc.user
                  << std::setw(12) << cpuTime
                  << windowTitle;
    } else {
        std::cout << std::setw(12) << std::to_string(proc.memory_kb) + "K";
    }

    if (showSvc) {
        // 服务列（Linux 无）
        std::cout << "  N/A";
    }
    if (showModules && !proc.modules.empty()) {
        std::cout << "  Modules: ";
        for (size_t i = 0; i < proc.modules.size() && i < 3; ++i) {
            if (i > 0) std::cout << ",";
            std::cout << proc.modules[i];
        }
        if (proc.modules.size() > 3) std::cout << "...";
    }
    std::cout << std::endl;
}

// 打印 LIST 格式
void printList(const ProcessInfo& proc, bool verbose, bool showSvc, bool showModules) {
    std::cout << "映像名称:    " << proc.name << std::endl;
    std::cout << "PID:         " << proc.pid << std::endl;
    std::cout << "会话名:      " << proc.user << std::endl;
    std::cout << "会话#:       1" << std::endl;
    std::cout << "内存使用:    " << proc.memory_kb << " K" << std::endl;
    if (verbose) {
        std::string stateStr(1, proc.state);
        std::cout << "状态:        " << stateStr << std::endl;
        std::cout << "用户名:      " << proc.user << std::endl;
        std::cout << "CPU 时间:    0:00:00" << std::endl;
        std::cout << "窗口标题:    N/A" << std::endl;
    }
    if (showSvc) {
        std::cout << "服务:        无" << std::endl;
    }
    if (showModules && !proc.modules.empty()) {
        std::cout << "模块信息:" << std::endl;
        for (const auto& mod : proc.modules) {
            std::cout << "  " << mod << std::endl;
        }
    }
    std::cout << std::endl;
}

// 打印 CSV 行
void printCsvRow(const ProcessInfo& proc, bool verbose, bool showSvc, bool showModules) {
    std::cout << "\"" << proc.name << "\","
              << "\"" << proc.pid << "\","
              << "\"" << proc.user << "\","
              << "\"1\","
              << "\"" << proc.memory_kb << "\"";
    if (verbose) {
        std::string stateStr(1, proc.state);
        std::cout << ",\"" << stateStr << "\""
                  << ",\"" << proc.user << "\""
                  << ",\"0:00:00\""
                  << ",\"N/A\"";
    }
    if (showSvc) {
        std::cout << ",\"\"";
    }
    if (showModules) {
        std::string modStr;
        for (size_t i = 0; i < proc.modules.size(); ++i) {
            if (i > 0) modStr += ";";
            modStr += proc.modules[i];
        }
        std::cout << ",\"" << modStr << "\"";
    }
    std::cout << std::endl;
}

// 打印帮助信息
void printHelp() {
    std::cout << "Microsoft Windows [Version 10.0.19044.1706]" << std::endl;
    std::cout << "(c) Microsoft Corporation. All rights reserved." << std::endl;
    std::cout << std::endl;
    std::cout << "C:\\Users\\Termux>tasklist /?" << std::endl;
    std::cout << std::endl;
    std::cout << "TASKLIST [/S system [/U username [/P [password]]]] [/M [module] | /SVC | /V]" << std::endl;
    std::cout << "         [/FI filter] [/FO format] [/NH]" << std::endl;
    std::cout << std::endl;
    std::cout << "描述:" << std::endl;
    std::cout << "    该工具显示在本地或远程机器上当前运行的进程列表。" << std::endl;
    std::cout << std::endl;
    std::cout << "参数列表:" << std::endl;
    std::cout << "    /S   system           指定连接到的远程系统。（Termux 不支持）" << std::endl;
    std::cout << "    /U   [domain\\]user    指定应该在哪个用户上下文执行这个命令。（不支持）" << std::endl;
    std::cout << "    /P   [password]       为提供的用户上下文指定密码。（不支持）" << std::endl;
    std::cout << "    /M   [module]         列出当前使用所给 exe/dll 名称的所有任务。" << std::endl;
    std::cout << "                          如果没有指定模块名称，显示所有加载的模块。" << std::endl;
    std::cout << "    /SVC                  显示每个进程中的服务。（Linux 下不支持）" << std::endl;
    std::cout << "    /V                    显示详细任务信息。" << std::endl;
    std::cout << "    /FI   filter          显示一系列符合筛选器指定条件的任务。" << std::endl;
    std::cout << "    /FO   format          指定输出格式。有效值: TABLE、LIST、CSV。" << std::endl;
    std::cout << "    /NH                   指定列标题不应该在输出中显示。只对 TABLE 和 CSV 格式有效。" << std::endl;
    std::cout << "    /?                    显示此帮助消息。" << std::endl;
    std::cout << std::endl;
    std::cout << "筛选器:" << std::endl;
    std::cout << "    筛选器名称      有效运算符                有效值" << std::endl;
    std::cout << "    -----------     ---------------           -------------------------" << std::endl;
    std::cout << "    STATUS          eq, ne                    RUNNING | NOT RESPONDING" << std::endl;
    std::cout << "    IMAGENAME       eq, ne                    映像名称" << std::endl;
    std::cout << "    PID             eq, ne, gt, lt, ge, le    PID 值" << std::endl;
    std::cout << "    SESSION         eq, ne, gt, lt, ge, le    会话编号" << std::endl;
    std::cout << "    SESSIONNAME     eq, ne                    会话名称" << std::endl;
    std::cout << "    CPUTIME         eq, ne, gt, lt, ge, le    CPU 时间，格式为 hh:mm:ss" << std::endl;
    std::cout << "    MEMUSAGE        eq, ne, gt, lt, ge, le    内存使用量，单位是 KB" << std::endl;
    std::cout << "    USERNAME        eq, ne                    用户名，格式为 [domain\\]user" << std::endl;
    std::cout << "    SERVICES        eq, ne                    服务名称" << std::endl;
    std::cout << "    WINDOWTITLE     eq, ne                    窗口标题" << std::endl;
    std::cout << "    MODULES         eq, ne                    DLL 名称" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "    TASKLIST" << std::endl;
    std::cout << "    TASKLIST /M" << std::endl;
    std::cout << "    TASKLIST /V" << std::endl;
    std::cout << "    TASKLIST /SVC" << std::endl;
    std::cout << "    TASKLIST /M wbem*" << std::endl;
    std::cout << "    TASKLIST /S system /FO LIST" << std::endl;
    std::cout << "    TASKLIST /S system /U domain\\username /FO CSV /NH" << std::endl;
    std::cout << "    TASKLIST /S system /U username /P password /FO TABLE /FI \"USERNAME ne NT*\"" << std::endl;
}

int main(int argc, char* argv[]) {
    // 解析命令行参数
    bool verbose = false;
    bool showSvc = false;
    bool showModules = false;
    std::string moduleFilter; // 如果 /M 后跟模块名，则只显示加载该模块的进程
    std::string format = "TABLE";
    bool noHeader = false;
    std::vector<Filter> filters;

    // 模拟 Windows 风格头部
    std::cout << "Microsoft Windows [Version 10.0.19044.1706]" << std::endl;
    std::cout << "(c) Microsoft Corporation. All rights reserved." << std::endl;
    std::cout << std::endl;

    // 如果没有参数，直接显示任务列表
    if (argc == 1) {
        // 继续执行，无特殊选项
    } else {
        // 简单解析参数（不处理 /S, /U, /P）
        int i = 1;
        while (i < argc) {
            std::string arg = argv[i];
            if (arg == "/?" || arg == "-?" || arg == "--help") {
                printHelp();
                return 0;
            } else if (arg == "/V" || arg == "-V") {
                verbose = true;
                ++i;
            } else if (arg == "/SVC") {
                showSvc = true;
                ++i;
            } else if (arg == "/M") {
                showModules = true;
                // 检查下一个参数是否是模块名（不以 '/' 开头）
                if (i + 1 < argc && argv[i+1][0] != '/') {
                    moduleFilter = argv[i+1];
                    ++i;
                }
                ++i;
            } else if (arg == "/FO") {
                if (i + 1 < argc) {
                    format = argv[i+1];
                    ++i;
                } else {
                    std::cerr << "错误: /FO 需要格式参数。" << std::endl;
                    return 1;
                }
                ++i;
            } else if (arg == "/FI") {
                if (i + 1 < argc) {
                    Filter f;
                    if (parseFilter(argv[i+1], f)) {
                        filters.push_back(f);
                    } else {
                        std::cerr << "错误: 无法解析筛选器: " << argv[i+1] << std::endl;
                        return 1;
                    }
                    ++i;
                } else {
                    std::cerr << "错误: /FI 需要筛选器字符串。" << std::endl;
                    return 1;
                }
                ++i;
            } else if (arg == "/NH") {
                noHeader = true;
                ++i;
            } else if (arg == "/S" || arg == "/U" || arg == "/P") {
                // 忽略这些远程参数，但要跳过可能的下一个参数
                if (i + 1 < argc && argv[i+1][0] != '/') {
                    i += 2;
                } else {
                    ++i;
                }
            } else {
                std::cerr << "错误: 未知参数: " << arg << std::endl;
                std::cerr << "输入 \"tasklist /?\" 查看帮助。" << std::endl;
                return 1;
            }
        }
    }

    // 收集进程信息
    std::vector<ProcessInfo> processes = collectProcesses(showModules, showSvc);

    // 应用筛选器
    if (!filters.empty()) {
        std::vector<ProcessInfo> filtered;
        for (const auto& proc : processes) {
            bool match = true;
            for (const auto& f : filters) {
                if (!matchesFilter(proc, f)) {
                    match = false;
                    break;
                }
            }
            if (match) filtered.push_back(proc);
        }
        processes = filtered;
    }

    // 如果指定了 moduleFilter，进一步筛选只包含该模块的进程
    if (showModules && !moduleFilter.empty()) {
        std::vector<ProcessInfo> filtered;
        std::string lowerMod = toLower(moduleFilter);
        for (const auto& proc : processes) {
            for (const auto& mod : proc.modules) {
                if (toLower(mod).find(lowerMod) != std::string::npos) {
                    filtered.push_back(proc);
                    break;
                }
            }
        }
        processes = filtered;
    }

    // 按 PID 排序
    std::sort(processes.begin(), processes.end(),
              [](const ProcessInfo& a, const ProcessInfo& b) { return a.pid < b.pid; });

    // 输出
    std::string formatLower = toLower(format);
    if (formatLower == "table") {
        if (!noHeader) {
            if (verbose) {
                std::cout << std::left << std::setw(25) << "映像名称"
                          << std::setw(8) << "PID"
                          << std::setw(16) << "会话名"
                          << std::setw(8) << "会话#"
                          << std::setw(12) << "内存使用"
                          << std::setw(16) << "状态"
                          << std::setw(24) << "用户名"
                          << std::setw(12) << "CPU 时间"
                          << "窗口标题";
            } else {
                std::cout << std::left << std::setw(25) << "映像名称"
                          << std::setw(8) << "PID"
                          << std::setw(16) << "会话名"
                          << std::setw(8) << "会话#"
                          << "内存使用";
            }
            if (showSvc) std::cout << "  服务";
            if (showModules) std::cout << "  模块";
            std::cout << std::endl;
            std::cout << std::string(100, '-') << std::endl;
        }
        for (const auto& proc : processes) {
            printTableRow(proc, verbose, showSvc, showModules);
        }
    } else if (formatLower == "list") {
        for (const auto& proc : processes) {
            printList(proc, verbose, showSvc, showModules);
        }
    } else if (formatLower == "csv") {
        if (!noHeader) {
            std::cout << "\"映像名称\",\"PID\",\"会话名\",\"会话#\",\"内存使用\"";
            if (verbose) std::cout << ",\"状态\",\"用户名\",\"CPU 时间\",\"窗口标题\"";
            if (showSvc) std::cout << ",\"服务\"";
            if (showModules) std::cout << ",\"模块\"";
            std::cout << std::endl;
        }
        for (const auto& proc : processes) {
            printCsvRow(proc, verbose, showSvc, showModules);
        }
    } else {
        std::cerr << "错误: 无效的输出格式: " << format << std::endl;
        return 1;
    }

    return 0;
}