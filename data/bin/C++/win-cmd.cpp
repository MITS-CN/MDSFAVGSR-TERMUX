#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

// 简单帮助
void print_usage(const char* prog) {
    std::cout << R"(启动 Windows 命令解释器的一个新实例

CMD [/A | /U] [/Q] [/D] [/E:ON | /E:OFF] [/F:ON | /F:OFF] [/V:ON | /V:OFF]
    [[/S] [/C | /K] string]

/C      执行字符串指定的命令然后终止
/K      执行字符串指定的命令但保留
/S      修改 /C 或 /K 之后的字符串处理(见下)
/Q      关闭回显
/D      禁止从注册表执行 AutoRun 命令(见下)
/A      使向管道或文件的内部命令输出成为 ANSI
/U      使向管道或文件的内部命令输出成为
        Unicode
/T:fg   设置前台/背景颜色(详细信息见 COLOR /?)
/E:ON   启用命令扩展(见下)
/E:OFF  禁用命令扩展(见下)
/F:ON   启用文件和目录名完成字符(见下)
/F:OFF  禁用文件和目录名完成字符(见下)
/V:ON   使用 ! 作为分隔符启用延迟的环境变量
        扩展。例如，/V:ON 会允许 !var! 在执行时
        扩展变量 var。var 语法会在输入时
        扩展变量，这与在一个 FOR
        循环内不同。
/V:OFF  禁用延迟的环境扩展。

注意，如果字符串加有引号，可以接受用命令分隔符 "&&"
分隔多个命令。另外，由于兼容性
原因，/X 与 /E:ON 相同，/Y 与 /E:OFF 相同，且 /R 与
/C 相同。任何其他开关都将被忽略。

如果指定了 /C 或 /K，则会将该开关之后的
命令行的剩余部分作为一个命令行处理，其中，会使用下列逻辑
处理引号(")字符:

    1.  如果符合下列所有条件，则会保留
        命令行上的引号字符:

        - 不带 /S 开关
        - 正好两个引号字符
        - 在两个引号字符之间无任何特殊字符，
          特殊字符指下列字符: &<>()@^|
        - 在两个引号字符之间至少有
          一个空格字符
        - 在两个引号字符之间的字符串是某个
          可执行文件的名称。

    2.  否则，老办法是看第一个字符
        是否是引号字符，如果是，则去掉首字符并
        删除命令行上最后一个引号，保留
        最后一个引号之后的所有文本。

如果 /D 未在命令行上被指定，当 CMD.EXE 开始时，它会寻找
以下 REG_SZ/REG_EXPAND_SZ 注册表变量。如果其中一个或
两个都存在，这两个变量会先被执行。

    HKEY_LOCAL_MACHINE\Software\Microsoft\Command Processor\AutoRun

        和/或

    HKEY_CURRENT_USER\Software\Microsoft\Command Processor\AutoRun

命令扩展是按默认值启用的。你也可以使用 /E:OFF ，为某一
特定调用而停用扩展。你
可以在机器上和/或用户登录会话上
启用或停用 CMD.EXE 所有调用的扩展，这要通过设置使用
REGEDIT.EXE 的注册表中的一个或两个 REG_DWORD 值:

    HKEY_LOCAL_MACHINE\Software\Microsoft\Command Processor\EnableExtensions

        和/或

    HKEY_CURRENT_USER\Software\Microsoft\Command Processor\EnableExtensions

到 0x1 或 0x0。用户特定设置
比机器设置有优先权。命令行
开关比注册表设置有优先权。

在批处理文件中，SETLOCAL ENABLEEXTENSIONS 或 DISABLEEXTENSIONS 参数
比 /E:ON 或 /E:OFF 开关有优先权。请参阅 SETLOCAL /? 获取详细信息。

命令扩展包括对下列命令所做的
更改和/或添加:

    DEL or ERASE
    COLOR
    CD or CHDIR
    MD or MKDIR
    PROMPT
    PUSHD
    POPD
    SET
    SETLOCAL
    ENDLOCAL
    IF
    FOR
    CALL
    SHIFT
    GOTO
    START (同时包括对外部命令调用所做的更改)
    ASSOC
    FTYPE

有关特定详细信息，请键入 commandname /? 查看。

延迟环境变量扩展不按默认值启用。你
可以用/V:ON 或 /V:OFF 开关，为 CMD.EXE 的某个调用而
启用或停用延迟环境变量扩展。你
可以在机器上和/或用户登录会话上启用或停用 CMD.EXE 所有
调用的延迟扩展，这要通过设置使用 REGEDIT.EXE 的注册表中的
一个或两个 REG_DWORD 值:

    HKEY_LOCAL_MACHINE\Software\Microsoft\Command Processor\DelayedExpansion

        和/或

    HKEY_CURRENT_USER\Software\Microsoft\Command Processor\DelayedExpansion

到 0x1 或 0x0。用户特定设置
比机器设置有优先权。命令行开关
比注册表设置有优先权。

在批处理文件中，SETLOCAL ENABLEDELAYEDEXPANSION 或 DISABLEDELAYEDEXPANSION
参数比 /V:ON 或 /V:OFF 开关有优先权。请参阅 SETLOCAL /?
获取详细信息。

如果延迟环境变量扩展被启用，
惊叹号字符可在执行时间被用来
代替一个环境变量的数值。

你可以用 /F:ON 或 /F:OFF 开关为 CMD.EXE 的某个
调用而启用或禁用文件名完成。你可以在计算上和/或
用户登录会话上启用或禁用 CMD.EXE 所有调用的完成，
这可以通过使用 REGEDIT.EXE 设置注册表中的下列
 REG_DWORD 的全部或其中之一:

    HKEY_LOCAL_MACHINE\Software\Microsoft\Command Processor\CompletionChar
    HKEY_LOCAL_MACHINE\Software\Microsoft\Command Processor\PathCompletionChar

        和/或

    HKEY_CURRENT_USER\Software\Microsoft\Command Processor\CompletionChar
    HKEY_CURRENT_USER\Software\Microsoft\Command Processor\PathCompletionChar

由一个控制字符的十六进制值作为一个特定参数(例如，0x4
是Ctrl-D，0x6 是 Ctrl-F)。用户特定设置优先于机器设置。
命令行开关优先于注册表设置。

如果完成是用 /F:ON 开关启用的，两个要使用的控制符是:
目录名完成用 Ctrl-D，文件名完成用 Ctrl-F。要停用
注册表中的某个字符，请用空格(0x20)的数值，因为此字符
不是控制字符。

如果键入两个控制字符中的一个，完成会被调用。完成功能将
路径字符串带到光标的左边，如果没有通配符，将通配符附加
到左边，并建立相符的路径列表。然后，显示第一个相符的路
径。如果没有相符的路径，则发出嘟嘟声，不影响显示。之后，
重复按同一个控制字符会循环显示相符路径的列表。将 Shift
键跟控制字符同时按下，会倒着显示列表。如果对该行进行了
任何编辑，并再次按下控制字符，保存的相符路径的列表会被
丢弃，新的会被生成。如果在文件和目录名完成之间切换，会
发生同样现象。两个控制字符之间的唯一区别是文件完成字符
符合文件和目录名，而目录完成字符只符合目录名。如果文件
完成被用于内置式目录命令(CD、MD 或 RD)，就会使用目录
完成。
用引号将相符路径括起来，完成代码可以正确处理含有空格
或其他特殊字符的文件名。同时，如果备份，然后从行内调用
文件完成，完成被调用时位于光标右方的文字会被调用。

需要引号的特殊字符是:
     <space>
     &()[]{}^=;!'+,`~
)";
}

// 解析 /T:XY 颜色，返回 ANSI 转义前缀
std::string color_from_t(const std::string& arg) {
    // 格式: /T:AB   A 背景色 (0-F), B 前景色 (0-F)
    if (arg.size() < 4 || arg[2] != ':') return "";
    char bgc = std::toupper(arg[3]);
    char fgc = (arg.size() > 4) ? std::toupper(arg[4]) : '7'; // 默认白字

    // 简化映射：使用 16 色 ANSI 代码
    int fg = -1, bg = -1;
    auto map_color = [](char c) -> int {
        // 0=黑,1=蓝,2=绿,3=青,4=红,5=紫,6=黄,7=白,8=灰,9=亮蓝,A=亮绿,B=亮青,C=亮红,D=亮紫,E=亮黄,F=亮白
        switch (c) {
        case '0': return 30; // 黑
        case '1': return 34; // 蓝
        case '2': return 32; // 绿
        case '3': return 36; // 青
        case '4': return 31; // 红
        case '5': return 35; // 紫
        case '6': return 33; // 黄
        case '7': return 37; // 白
        case '8': return 90; // 亮黑(灰)
        case '9': return 94; // 亮蓝
        case 'A': return 92; // 亮绿
        case 'B': return 96; // 亮青
        case 'C': return 91; // 亮红
        case 'D': return 95; // 亮紫
        case 'E': return 93; // 亮黄
        case 'F': return 97; // 亮白
        default: return -1;
        }
        };
    fg = map_color(fgc);
    bg = map_color(bgc);
    if (fg == -1 || bg == -1) return "";
    // 背景色 = 前景色 + 10
    int bgcode = fg + 10;  // 实际上 ANSI 背景码是 fg+10 吗？标准是 40-47,100-107
    // 正确转换：30-37 黑~白 -> 40-47 背景；90-97 -> 100-107
    auto fg2bg = [](int fgcode) -> int {
        if (fgcode >= 30 && fgcode <= 37) return fgcode + 10;
        if (fgcode >= 90 && fgcode <= 97) return fgcode + 10; // 90+10=100 亮色背景
        return 40; // fallback
        };
    return "\033[" + std::to_string(fg) + ";" + std::to_string(fg2bg(fg)) + "m";
}

int main(int argc, char* argv[]) {
    bool quiet = false;
    bool no_rc = false;
    bool ansi_mode = false;
    bool unicode_mode = false;
    std::string color_escape;
    std::string cmd_flag;  // "/C" 或 "/K"
    std::string command;

    // 解析参数，直到遇见 /C 或 /K
    int i = 1;
    for (; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "/C" || arg == "/K") {
            cmd_flag = arg;
            ++i;
            break;
        }
        else if (arg == "/Q") quiet = true;
        else if (arg == "/D") no_rc = true;
        else if (arg == "/A") ansi_mode = true;
        else if (arg == "/U") unicode_mode = true;
        else if (arg.rfind("/T:", 0) == 0) color_escape = color_from_t(arg);
        else if (arg == "/E:ON" || arg == "/E:OFF") 
            std::cerr << "警告: /E 开关被忽略 (bash 扩展始终启用)\n";
        else if (arg == "/V:ON" || arg == "/V:OFF")
            std::cerr << "警告: /V 开关被忽略 (无延迟扩展)\n";
        else if (arg == "/F:ON" || arg == "/F:OFF")
            std::cerr << "警告: /F 开关被忽略 (readline 补全不可控)\n";
        else if (arg == "/S") { /* 忽略 */ }
        else if (arg == "/?" || arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        else {
            std::cerr << "未知选项: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    if (cmd_flag.empty()) {
        std::cerr << "错误: 必须指定 /C 或 /K\n";
        print_usage(argv[0]);
        return 1;
    }

    // 拼接命令（从 i 到 argc）
    for (; i < argc; ++i) {
        if (!command.empty()) command += " ";
        command += argv[i];
    }

    // ---------- 1. 确定要使用的 Shell（优先 $SHELL）----------
    const char* shell = getenv("SHELL");
    if (!shell || access(shell, X_OK) != 0) {
        // 回退方案
        shell = "/data/data/com.termux/files/usr/bin/bash";
        if (access(shell, X_OK) != 0) shell = "/bin/bash";
        if (access(shell, X_OK) != 0) shell = "/bin/sh";
    }

    // ---------- 2. 构建命令前缀（环境变量、颜色）----------
    std::string prefix;
    if (ansi_mode) prefix += "export LANG=C; ";
    else if (unicode_mode) prefix += "export LANG=en_US.UTF-8; ";
    if (!color_escape.empty()) {
        // 注意：转义双引号，避免 shell 解析错误
        prefix += "printf '%s' \"" + color_escape + "\"; ";
    }

    // ---------- 3. 根据 /C 或 /K 构建完整命令 ----------
    std::string final_cmd;
    if (cmd_flag == "/C") {
        // /C: 执行命令后退出
        final_cmd = prefix + command;
    } 
    else { // /K: 执行命令后进入交互式 shell
        std::string interactive_cmd = "; exec " + std::string(shell) + " -i";
        if (no_rc) {
            // 对 bash/zsh 添加 --norc --noprofile
            std::string base = std::string(shell);
            if (base.find("bash") != std::string::npos || 
                base.find("zsh") != std::string::npos) {
                interactive_cmd = "; exec " + base + " --norc --noprofile -i";
            }
        }
        final_cmd = prefix + command + interactive_cmd;
    }

    // ---------- 4. 准备 exec 参数 ----------
    std::vector<const char*> args;
    args.push_back(shell);

    // 对于 /C 且 no_rc：让初始 shell 也不加载 rc 文件
    if (no_rc && cmd_flag == "/C") {
        // 仅对 bash/zsh 有效；其他 shell 忽略
        std::string base = std::string(shell);
        if (base.find("bash") != std::string::npos) {
            args.push_back("--norc");
            args.push_back("--noprofile");
        } else if (base.find("zsh") != std::string::npos) {
            args.push_back("--norc");
            args.push_back("--noprofile");
        }
    }

    args.push_back("-c");
    args.push_back(final_cmd.c_str());
    args.push_back(nullptr);

    // 执行
    execvp(shell, const_cast<char* const*>(args.data()));
    std::perror("exec 失败");
    return 1;
}