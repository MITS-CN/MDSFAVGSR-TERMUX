#include <iostream>
#include <fstream>
#include <cstring>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <cstdlib>

void usage() {
    std::cout <<
        "COMP [data1] [data2] [/D] [/A] [/L] [/N=number] [/C] [/M] [/?]\n\n"
        "Description:\n"
        "    Compares the contents of two files byte by byte.\n\n"
        "    data1, data2   Files to compare.\n"
        "    /D             Display differences in decimal format.\n"
        "    /A             Display differences as ASCII characters.\n"
        "    /L             Display line numbers for differences.\n"
        "    /N=number      Compare only the first 'number' of lines.\n"
        "    /C             Perform case-insensitive comparison.\n"
        "    /M             Do not prompt for more files (always no prompt).\n"
        "    /?             Display this help.\n";
}

// 根据选项将字节转换为显示字符串
std::string byte_to_str(unsigned char b, bool ascii, bool decimal) {
    if (ascii) {
        if (std::isprint(b)) {
            return std::string("'") + char(b) + "'";
        } else {
            std::ostringstream oss;
            oss << "0x" << std::hex << std::uppercase << (int)b;
            return oss.str();
        }
    }
    if (decimal) {
        return std::to_string((int)b);
    }
    // 默认十六进制
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << (int)b;
    return oss.str();
}

int main(int argc, char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "/?") == 0 || strcmp(argv[1], "--help") == 0) {
        usage();
        return 0;
    }

    bool opt_D = false, opt_A = false, opt_L = false, opt_C = false, opt_M = false;
    int  opt_N = -1;
    std::string file1, file2;

    // 解析参数
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg[0] == '/' || arg[0] == '-') {
            std::string opt = arg.substr(1);
            for (size_t j = 0; j < opt.size(); ++j) {
                char c = std::toupper((unsigned char)opt[j]);
                if (c == 'D') opt_D = true;
                else if (c == 'A') opt_A = true;
                else if (c == 'L') opt_L = true;
                else if (c == 'C') opt_C = true;
                else if (c == 'M') opt_M = true;
                else if (c == '?') { usage(); return 0; }
                else if (c == 'N') {
                    // 处理 /N=number 或 /N:number
                    size_t eq = arg.find('=', j);
                    if (eq == std::string::npos) eq = arg.find(':', j);
                    if (eq != std::string::npos) {
                        opt_N = std::atoi(arg.c_str() + eq + 1);
                        j = opt.size(); // 跳过该参数剩余字符
                    } else {
                        std::cerr << "Error: /N requires a value, e.g. /N=10\n";
                        return 1;
                    }
                } else {
                    std::cerr << "Unknown option: /" << c << "\n";
                    return 1;
                }
            }
        } else {
            if (file1.empty()) file1 = arg;
            else if (file2.empty()) file2 = arg;
            else {
                std::cerr << "Error: Too many file arguments.\n";
                return 1;
            }
        }
    }

    if (file1.empty() || file2.empty()) {
        std::cerr << "Error: Missing file names.\n";
        usage();
        return 1;
    }

    // 打开文件（二进制模式）
    std::ifstream f1(file1, std::ios::binary);
    std::ifstream f2(file2, std::ios::binary);
    if (!f1) { std::cerr << "Error: Cannot open " << file1 << "\n"; return 1; }
    if (!f2) { std::cerr << "Error: Cannot open " << file2 << "\n"; return 1; }

    // 获取文件大小
    f1.seekg(0, std::ios::end); long size1 = f1.tellg(); f1.seekg(0, std::ios::beg);
    f2.seekg(0, std::ios::end); long size2 = f2.tellg(); f2.seekg(0, std::ios::beg);
    bool sizeMismatch = (size1 != size2);

    std::cout << "Comparing " << file1 << " and " << file2 << "...\n";
    if (sizeMismatch) {
        std::cout << "Files are different sizes.\n";
    }

    int  diffCount = 0;
    const int MAX_DIFFS = 10;
    long offset = 0;      // 字节偏移（从0开始）
    int  lineNum = 1;     // 行号（1-based）
    int  colNum  = 1;     // 列号（用于 /L 显示）

    while (true) {
        // 检查是否已比较完要求的行数
        if (opt_N > 0 && lineNum > opt_N) break;

        char b1, b2;
        bool r1 = f1.get(b1) ? true : false;
        bool r2 = f2.get(b2) ? true : false;

        if (!r1 && !r2) break;          // 两个文件均结束
        if (!r1 || !r2) {               // 其中一个先结束（大小不同）
            break;
        }

        unsigned char ub1 = static_cast<unsigned char>(b1);
        unsigned char ub2 = static_cast<unsigned char>(b2);

        bool differ = false;
        if (opt_C) {
            differ = (std::tolower(ub1) != std::tolower(ub2));
        } else {
            differ = (ub1 != ub2);
        }

        if (differ) {
            diffCount++;
            if (diffCount <= MAX_DIFFS) {
                std::cout << "Compare error at ";
                if (opt_L) {
                    std::cout << "line " << lineNum << " (offset 0x" << std::hex << offset << ")";
                } else {
                    std::cout << "offset 0x" << std::hex << offset;
                }
                std::cout << std::dec << "\n";
                std::cout << "file1 = " << byte_to_str(ub1, opt_A, opt_D)
                          << ", file2 = " << byte_to_str(ub2, opt_A, opt_D) << "\n";
            }
            if (diffCount > MAX_DIFFS) {
                std::cout << "More than 10 differences - stopping comparison.\n";
                break;
            }
        }

        // 更新行/列（基于文件1的换行符）
        if (b1 == '\n') {
            lineNum++;
            colNum = 1;
        } else {
            colNum++;
        }
        offset++;
    }

    if (diffCount == 0 && !sizeMismatch) {
        std::cout << "Files compare OK\n";
        return 0;
    } else {
        if (diffCount > 0) {
            std::cout << "Found " << diffCount << " difference(s).\n";
        }
        return 1;
    }
}