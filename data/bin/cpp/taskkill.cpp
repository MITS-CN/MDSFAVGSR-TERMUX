#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/types.h>

// 将字符串转换为大写（用于参数比较）
std::string toUpper(const std::string& s) {
    std::string result;
    for (char c : s) result.push_back(std::toupper(c));
    return result;
}

// 将字符串转换为小写（用于值比较）
std::string toLower(const std::string& s) {
    std::string result;
    for (char c : s) result.push_back(std::tolower(c));
    return result;
}

// 去除字符串首尾空白
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// 进程信息结构体
struct ProcessInfo {
    int pid;
    int ppid;
    std::string name;
    std::string user;
    unsigned long memory_kb;
    char state;
};

// 从 /proc/[pid]/stat 获取进程状态和 PPID
char getProcessState(int pid, int& ppid) {
    std::string path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream file(path);
    if (!file.is_open()) return '?';
    std::string line;
    std::getline(file, line);
    file.close();

    size_t lparen = line.find('(');
    size_t rparen = line.rfind(')');
    if (lparen == std::string::npos || rparen == std::string::npos) return '?';
    
    size_t pos = rparen + 1;
    while (pos < line.size() && line[pos] == ' ') ++pos;
    if (pos >= line.size()) return '?';
    char state = line[pos];
    
    int field = 0;
    for (size_t i = pos; i < line.size(); ++i) {
        if (line[i] == ' ') {
            ++field;
            if (field == 3) {
                size_t start = i + 1;
                size_t end = line.find(' ', start);
                if (end == std::string::npos) end = line.size();
                ppid = std::atoi(line.substr(start, end - start).c_str());
                break;
            }
        }
    }
    return state;
}

// 获取进程详细信息
bool getProcessStatus(int pid, std::string& name, unsigned long& memory_kb, std::string& user) {
    std::string path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream file(path);
    if (!file.is_open()) return false;

    name.clear();
    memory_kb = 0;
    user = "?";
    unsigned int uid = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (line.compare(0, 5, "Name:") == 0) {
            name = trim(line.substr(5));
        } else if (line.compare(0, 6, "VmRSS:") == 0) {
            char* end;
            memory_kb = std::strtoul(line.substr(6).c_str(), &end, 10);
        } else if (line.compare(0, 4, "Uid:") == 0) {
            std::istringstream iss(line.substr(4));
            iss >> uid;
            struct passwd* pw = getpwuid(uid);
            if (pw) user = pw->pw_name;
            else user = std::to_string(uid);
        }
    }
    file.close();
    return !name.empty();
}

// 获取所有进程
std::vector<ProcessInfo> getAllProcesses() {
    std::vector<ProcessInfo> procs;
    DIR* dir = opendir("/proc");
    if (!dir) return procs;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            bool isNum = true;
            for (char* p = entry->d_name; *p; ++p) {
                if (!isdigit(*p)) { isNum = false; break; }
            }
            if (!isNum) continue;
            int pid = atoi(entry->d_name);
            ProcessInfo info;
            info.pid = pid;
            info.ppid = -1;
            info.state = getProcessState(pid, info.ppid);
            if (!getProcessStatus(pid, info.name, info.memory_kb, info.user)) {
                continue;
            }
            procs.push_back(info);
        }
    }
    closedir(dir);
    return procs;
}

// 筛选器
struct Filter {
    std::string field;
    std::string op;
    std::string value;
};

bool parseFilter(const std::string& filterStr, Filter& filter) {
    std::istringstream iss(filterStr);
    std::string field, op, value;
    if (!(iss >> field >> op)) return false;
    std::getline(iss >> std::ws, value);
    if (!value.empty() && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    filter.field = toLower(field);
    filter.op = toLower(op);
    filter.value = value;
    return true;
}

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
    return false;
}

// 获取子进程
std::vector<int> getChildPIDs(int pid, const std::vector<ProcessInfo>& allProcs) {
    std::vector<int> children;
    for (const auto& p : allProcs) {
        if (p.ppid == pid) children.push_back(p.pid);
    }
    return children;
}

void getAllDescendants(int pid, const std::vector<ProcessInfo>& allProcs, std::set<int>& result) {
    if (result.count(pid)) return;
    result.insert(pid);
    for (int child : getChildPIDs(pid, allProcs)) {
        getAllDescendants(child, allProcs, result);
    }
}

bool killProcess(int pid, bool force) {
    int sig = force ? SIGKILL : SIGTERM;
    return kill(pid, sig) == 0;
}

void printHelp() {
    std::cout << "Microsoft Windows [Version 10.0.19044.1706]" << std::endl;
    std::cout << "(c) Microsoft Corporation. All rights reserved." << std::endl;
    std::cout << std::endl;
    std::cout << "C:\\Users\\Termux>taskkill /?" << std::endl;
    std::cout << std::endl;
    std::cout << "TASKKILL [/S system [/U username [/P [password]]]]" << std::endl;
    std::cout << "         { [/FI filter] [/PID processid | /IM imagename] } [/T] [/F]" << std::endl;
    std::cout << std::endl;
    std::cout << "描述:" << std::endl;
    std::cout << "    使用该工具按照进程 ID (PID) 或映像名称终止任务。" << std::endl;
    std::cout << std::endl;
    std::cout << "参数列表:" << std::endl;
    std::cout << "    /S    system           指定要连接的远程系统。（Termux 不支持）" << std::endl;
    std::cout << "    /U    [domain\\]user    指定应该在哪个用户上下文执行这个命令。（不支持）" << std::endl;
    std::cout << "    /P    [password]       为提供的用户上下文指定密码。（不支持）" << std::endl;
    std::cout << "    /FI   filter           应用筛选器以选择一组进程。允许使用 \"*\"。" << std::endl;
    std::cout << "    /PID  processid         指定要终止的进程的 PID。" << std::endl;
    std::cout << "    /IM   imagename         指定要终止的进程的映像名称。可以使用通配符 '*'。" << std::endl;
    std::cout << "    /T                      终止指定的进程和由它启用的子进程。" << std::endl;
    std::cout << "    /F                      指定要强制终止进程。" << std::endl;
    std::cout << "    /?                      显示此帮助消息。" << std::endl;
    std::cout << std::endl;
    std::cout << "筛选器:" << std::endl;
    std::cout << "    筛选器名称      有效运算符                有效值" << std::endl;
    std::cout << "    -----------     ---------------           -------------------------" << std::endl;
    std::cout << "    STATUS          eq, ne                    RUNNING | NOT RESPONDING" << std::endl;
    std::cout << "    IMAGENAME       eq, ne                    映像名称" << std::endl;
    std::cout << "    PID             eq, ne, gt, lt, ge, le    PID 值" << std::endl;
    std::cout << "    SESSION         eq, ne, gt, lt, ge, le    会话编号" << std::endl;
    std::cout << "    CPUTIME         eq, ne, gt, lt, ge, le    CPU 时间格式为 hh:mm:ss" << std::endl;
    std::cout << "    MEMUSAGE        eq, ne, gt, lt, ge, le    内存使用量，单位为 KB" << std::endl;
    std::cout << "    USERNAME        eq, ne                    用户名，格式为 [domain\\]user" << std::endl;
    std::cout << "    MODULES         eq, ne                    DLL 名称" << std::endl;
    std::cout << "    SERVICES        eq, ne                    服务名称" << std::endl;
    std::cout << "    WINDOWTITLE     eq, ne                    窗口标题" << std::endl;
    std::cout << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "    TASKKILL /IM notepad.exe" << std::endl;
    std::cout << "    TASKKILL /PID 1230 /PID 1241 /PID 1253" << std::endl;
    std::cout << "    TASKKILL /F /IM cmd.exe" << std::endl;
    std::cout << "    TASKKILL /T /IM iexplore.exe" << std::endl;
    std::cout << "    TASKKILL /FI \"USERNAME eq NT AUTHORITY\\SYSTEM\" /IM svchost.exe" << std::endl;
}

int main(int argc, char* argv[]) {
    bool force = false;
    bool tree = false;
    std::vector<int> targetPids;
    std::vector<std::string> targetNames;
    std::vector<Filter> filters;

    if (argc == 1) {
        printHelp();
        return 0;
    }

    int i = 1;
    while (i < argc) {
        std::string arg = argv[i];
        std::string upperArg = toUpper(arg); // 转为大写用于比较

        if (upperArg == "/?" || upperArg == "-?" || upperArg == "--HELP") {
            printHelp();
            return 0;
        } else if (upperArg == "/F" || upperArg == "-F") {
            force = true;
            ++i;
        } else if (upperArg == "/T" || upperArg == "-T") {
            tree = true;
            ++i;
        } else if (upperArg == "/PID" || upperArg == "-PID") {
            if (i + 1 < argc) {
                int pid = std::atoi(argv[i+1]);
                if (pid > 0) targetPids.push_back(pid);
                else std::cerr << "错误: 无效的 PID: " << argv[i+1] << std::endl;
                i += 2;
            } else {
                std::cerr << "错误: /PID 需要进程 ID 参数。" << std::endl;
                return 1;
            }
        } else if (upperArg == "/IM" || upperArg == "-IM") {
            if (i + 1 < argc) {
                targetNames.push_back(argv[i+1]);
                i += 2;
            } else {
                std::cerr << "错误: /IM 需要映像名称参数。" << std::endl;
                return 1;
            }
        } else if (upperArg == "/FI" || upperArg == "-FI") {
            if (i + 1 < argc) {
                Filter f;
                if (parseFilter(argv[i+1], f)) {
                    filters.push_back(f);
                } else {
                    std::cerr << "错误: 无法解析筛选器: " << argv[i+1] << std::endl;
                    return 1;
                }
                i += 2;
            } else {
                std::cerr << "错误: /FI 需要筛选器字符串。" << std::endl;
                return 1;
            }
        } else if (upperArg == "/S" || upperArg == "-S" || 
                   upperArg == "/U" || upperArg == "-U" || 
                   upperArg == "/P" || upperArg == "-P") {
            // 忽略远程参数，但跳过可能的下一个参数
            if (i + 1 < argc && argv[i+1][0] != '/') {
                i += 2;
            } else {
                ++i;
            }
        } else {
            std::cerr << "错误: 未知参数: " << arg << std::endl;
            std::cerr << "输入 \"taskkill /?\" 查看帮助。" << std::endl;
            return 1;
        }
    }

    if (targetPids.empty() && targetNames.empty() && filters.empty()) {
        std::cerr << "错误: 没有指定任何进程。请使用 /PID、/IM 或 /FI 参数。" << std::endl;
        return 1;
    }

    std::vector<ProcessInfo> allProcs = getAllProcesses();
    if (allProcs.empty()) {
        std::cerr << "错误: 无法读取进程列表。" << std::endl;
        return 1;
    }

    std::set<int> toKill;

    // 按 PID 添加
    for (int pid : targetPids) toKill.insert(pid);

    // 按映像名称添加（支持 * 通配符，简单包含匹配）
    for (const std::string& namePattern : targetNames) {
        std::string lowerPattern = toLower(namePattern);
        bool hasWildcard = (lowerPattern.find('*') != std::string::npos);
        for (const auto& proc : allProcs) {
            std::string procNameLower = toLower(proc.name);
            if (hasWildcard) {
                // 移除所有 * 后进行包含检查
                std::string search = lowerPattern;
                search.erase(std::remove(search.begin(), search.end(), '*'), search.end());
                if (procNameLower.find(search) != std::string::npos) {
                    toKill.insert(proc.pid);
                }
            } else {
                if (procNameLower == lowerPattern) {
                    toKill.insert(proc.pid);
                }
            }
        }
    }

    // 按筛选器添加
    for (const auto& filter : filters) {
        for (const auto& proc : allProcs) {
            if (matchesFilter(proc, filter)) {
                toKill.insert(proc.pid);
            }
        }
    }

    if (toKill.empty()) {
        std::cout << "信息: 没有找到匹配的进程。" << std::endl;
        return 0;
    }

    // 扩展进程树
    if (tree) {
        std::set<int> expanded;
        for (int pid : toKill) {
            getAllDescendants(pid, allProcs, expanded);
        }
        toKill = expanded;
    }

    int success = 0, fail = 0;
    for (int pid : toKill) {
        if (kill(pid, 0) != 0 && errno == ESRCH) {
            std::cout << "信息: 进程 " << pid << " 已不存在。" << std::endl;
            continue;
        }
        if (killProcess(pid, force)) {
            std::cout << "成功: 已终止 PID 为 " << pid << " 的进程。" << std::endl;
            ++success;
        } else {
            std::cerr << "错误: 无法终止 PID 为 " << pid << " 的进程。权限不足或进程不存在。" << std::endl;
            ++fail;
        }
    }

    return fail ? 1 : 0;
}