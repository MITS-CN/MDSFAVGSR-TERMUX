#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cerrno>

#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

void usage() {
    std::cout <<
        "SORT [/R] [/+n] [/O output] [/C] [/U] [/I] [/?] [file ...]\n\n"
        "Description:\n"
        "    Sorts text read from files or standard input.\n\n"
        "    /R        Reverse sort order (descending).\n"
        "    /+n       Begin comparison at character n (1-based).\n"
        "    /O output Write output to the specified file.\n"
        "    /C        Check if the input is already sorted.\n"
        "    /U        Output only unique lines (remove duplicates).\n"
        "    /I        Ignore case when comparing.\n"
        "    /?        Display this help.\n\n"
        "Examples:\n"
        "    sort file.txt\n"
        "    sort /R /+2 data.txt\n"
        "    sort /U /I input.txt /O output.txt\n"
        "    sort /C /U file.txt\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "/?") == 0 || strcmp(argv[1], "--help") == 0) {
        usage();
        return 0;
    }

    bool reverse = false;
    bool unique = false;
    bool check = false;
    bool ignoreCase = false;
    int keyStart = 0;          // 0-based
    std::string outputFile;
    std::vector<std::string> inputFiles;

    // 解析参数
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg[0] == '/' || arg[0] == '-') {
            // 处理 /O 可能紧跟文件名
            if (arg.size() >= 2 && std::toupper(arg[1]) == 'O') {
                if (arg.size() > 2) {
                    outputFile = arg.substr(2);
                } else {
                    if (i + 1 < argc) {
                        outputFile = argv[++i];
                    } else {
                        std::cerr << "Error: /O requires a filename.\n";
                        return 1;
                    }
                }
                continue;
            }
            // 处理其他单字符选项
            for (size_t j = 1; j < arg.size(); ++j) {
                char c = std::toupper((unsigned char)arg[j]);
                switch (c) {
                    case 'R': reverse = true; break;
                    case 'U': unique = true; break;
                    case 'C': check = true; break;
                    case 'I': ignoreCase = true; break;
                    case '+':
                        keyStart = std::atoi(arg.c_str() + j + 1) - 1;
                        if (keyStart < 0) keyStart = 0;
                        j = arg.size(); // 跳过剩余字符
                        break;
                    case '?':
                        usage();
                        return 0;
                    default:
                        std::cerr << "Unknown option: /" << c << "\n";
                        return 1;
                }
            }
        } else {
            // 输入文件
            inputFiles.push_back(arg);
        }
    }

    // 读取所有行
    std::vector<std::string> lines;
    if (inputFiles.empty()) {
        // 从标准输入读取
        std::string line;
        while (std::getline(std::cin, line)) {
            lines.push_back(line);
        }
    } else {
        // 从文件读取（支持多个文件串联）
        for (const auto &fname : inputFiles) {
            std::ifstream in(fname);
            if (!in) {
                std::cerr << "Error: Cannot open " << fname << "\n";
                return 1;
            }
            std::string line;
            while (std::getline(in, line)) {
                lines.push_back(line);
            }
        }
    }

    // 定义比较函数（基于键，可能忽略大小写）
    auto cmp = [&](const std::string &a, const std::string &b) -> bool {
        std::string keyA = (keyStart < a.size()) ? a.substr(keyStart) : "";
        std::string keyB = (keyStart < b.size()) ? b.substr(keyStart) : "";
        int res;
        if (ignoreCase) {
            res = strcasecmp(keyA.c_str(), keyB.c_str());
        } else {
            res = keyA.compare(keyB);
        }
        return reverse ? (res > 0) : (res < 0);
    };

    // /C 模式：检查是否已排序
    if (check) {
        bool sorted = true;
        for (size_t i = 1; i < lines.size(); ++i) {
            if (cmp(lines[i], lines[i - 1])) {
                sorted = false;
                break;
            }
            if (unique) {
                bool equal = false;
                if (ignoreCase) {
                    equal = (strcasecmp(lines[i].c_str(), lines[i - 1].c_str()) == 0);
                } else {
                    equal = (lines[i] == lines[i - 1]);
                }
                if (equal) {
                    sorted = false;
                    break;
                }
            }
        }
        if (sorted) {
            // 排序正确，无输出，返回 0
            return 0;
        } else {
            // 发现顺序错误或重复
            return 1;
        }
    }

    // 执行排序
    std::sort(lines.begin(), lines.end(), cmp);

    // 去重
    if (unique) {
        std::vector<std::string> uniqueLines;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i == 0) {
                uniqueLines.push_back(lines[i]);
            } else {
                bool equal = false;
                if (ignoreCase) {
                    equal = (strcasecmp(lines[i].c_str(), uniqueLines.back().c_str()) == 0);
                } else {
                    equal = (lines[i] == uniqueLines.back());
                }
                if (!equal) {
                    uniqueLines.push_back(lines[i]);
                }
            }
        }
        lines = std::move(uniqueLines);
    }

    // 输出结果
    if (!outputFile.empty()) {
        std::ofstream out(outputFile);
        if (!out) {
            std::cerr << "Error: Cannot open output file: " << outputFile << "\n";
            return 1;
        }
        for (const auto &l : lines) {
            out << l << "\n";
        }
    } else {
        for (const auto &l : lines) {
            std::cout << l << "\n";
        }
    }

    return 0;
}