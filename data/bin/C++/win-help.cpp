#include <iostream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>

// 辅助：转为小写
std::string to_lower(const std::string& s) {
    std::string lower = s;
    for (auto& c : lower) c = tolower(c);
    return lower;
}

// 命令详细帮助数据库
std::unordered_map<std::string, std::string> command_help = {
    {"assoc",     "显示或修改文件扩展名关联。\n\nASSOC [.ext[=[fileType]]]\n\n  .ext  指定文件扩展名\n  fileType  指定要与该扩展名关联的文件类型\n\n不带参数时，显示所有当前文件扩展名关联。"},
    {"attrib",   "显示或更改文件属性。\n\nATTRIB [+R | -R] [+A | -A] [+S | -S] [+H | -H] [[drive:][path]filename] [/S [/D]]\n\n  +   设置属性\n  -   清除属性\n  R   只读文件属性\n  A   存档文件属性\n  S   系统文件属性\n  H   隐藏文件属性\n  /S  处理当前文件夹及其子文件夹中的匹配文件\n  /D  也处理文件夹"},
    {"break",    "设置或清除扩展式 CTRL+C 检查。\n\nBREAK [ON | OFF]\n\n  ON   允许在批处理程序执行期间检测到 CTRL+C 时暂停\n  OFF  禁止扩展检测"},
    {"bcdedit",  "设置启动数据库中的属性以控制启动加载。\n\nBCDEDIT /command [arguments]\n\n常用命令:\n  /enum        列出启动管理器中的条目\n  /set         设置条目的选项值\n  /create      创建新的启动条目"},
    {"cacls",    "显示或修改文件的访问控制列表(ACL)。\n\nCACLS filename [/T] [/E] [/C] [/G user:perm] [/R user [...]] [/P user:perm [...]] [/D user [...]]\n\n  /T  递归更改指定路径下所有文件的 ACL\n  /E  编辑 ACL 而不替换\n  /C  在出现拒绝访问错误时继续\n  /G  授予用户访问权限\n  /R  撤销用户的访问权限\n  /P  替换用户的访问权限\n  /D  拒绝用户访问"},
    {"call",     "从另一个批处理程序调用这一个。\n\nCALL [drive:][path]filename [batch-parameters]\n\n  batch-parameters  指定批处理程序所需的命令行信息。"},
    {"cd",       "显示当前目录的名称或将其更改。\n\nCD [/D] [drive:][path]\nCD [..]\n\n  ..   进入父目录\n  /D   同时更改驱动器和目录"},
    {"chcp",     "显示或设置活动代码页数。\n\nCHCP [nnn]\n\n  nnn  指定代码页编号"},
    {"chdir",    "同 CD，显示当前目录的名称或将其更改。"},
    {"chkdsk",   "检查磁盘并显示状态报告。\n\nCHKDSK [volume[[path]filename]]] [/F] [/V] [/R] [/X] [/I] [/C] [/L[:size]]\n\n  /F  修复磁盘上的错误\n  /R  查找坏扇区并恢复可读信息\n  /X  必要时强制卸除卷"},
    {"chkntfs",  "显示或修改启动时间磁盘检查。\n\nCHKNTFS volume [...]\nCHKNTFS /D\nCHKNTFS /T[:time]\nCHKNTFS /X volume [...]\nCHKNTFS /C volume [...]\n\n  /D  将系统恢复为默认行为\n  /T  设置倒计时时间\n  /X  排除某个卷的检查\n  /C  安排某个卷在启动时检查"},
    {"cls",      "清除屏幕。\n\nCLS"},
    {"cmd",      "打开另一个 Windows 命令解释程序窗口。\n\nCMD [/A | /U] [/Q] [/D] [/E:ON | /E:OFF] [/F:ON | /F:OFF] [/V:ON | /V:OFF] [[/S] [/C | /K] string]\n\n  /C  执行字符串指定的命令然后终止\n  /K  执行字符串指定的命令但保留\n  /S  修改 /C 或 /K 之后的字符串处理"},
    {"color",    "设置默认控制台前景和背景颜色。\n\nCOLOR [attr]\n\n  attr  颜色属性，由两个十六进制数字指定"},
    {"comp",     "比较两个或两套文件的内容。\n\nCOMP [data1] [data2] [/D] [/A] [/L] [/N=number] [/C]\n\n  /D  以十进制格式显示差异\n  /A  以 ASCII 字符显示差异\n  /L  显示发生差异的行数\n  /N  只比较每个文件前指定行数\n  /C  比较时不区分大小写"},
    {"compact",  "显示或更改 NTFS 分区上文件的压缩。\n\nCOMPACT [/C | /U] [/S[:dir]] [/A] [/I] [/F] [/Q] [filename [...]]\n\n  /C  压缩指定的文件\n  /U  解压缩指定的文件\n  /S  对给定目录及所有子目录中的文件执行操作\n  /A  显示具有隐藏或系统属性的文件\n  /I  忽略错误\n  /F  强制压缩操作\n  /Q  仅报告摘要信息"},
    {"convert",  "将 FAT 卷转换成 NTFS。你不能转换当前驱动器。\n\nCONVERT volume /FS:NTFS [/V] [/CvtArea:filename] [/NoSecurity] [/X]\n\n  volume     指定驱动器号、装载点或卷名\n  /FS:NTFS   指定目标文件系统\n  /V         详细模式"},
    {"copy",     "将至少一个文件复制到另一个位置。\n\nCOPY [/D] [/V] [/N] [/Y | /-Y] [/Z] [/L] [/A | /B] source [/A | /B] [+ source ...] [destination]\n\n  source       指定要复制的文件\n  /A           表示一个 ASCII 文本文件\n  /B           表示一个二进制文件\n  /D           允许解密要复制的目标文件\n  /V           验证新文件写入是否正确\n  /Y           不使用确认是否要覆盖现有目标文件的提示\n  /-Y          使用确认提示"},
    {"date",     "显示或设置日期。\n\nDATE [/T | date]\n\n  /T   只显示当前日期，不提示输入新日期"},
    {"del",      "删除至少一个文件。\n\nDEL [/P] [/F] [/S] [/Q] [/A[[:]attributes]] names\n\n  /P          删除每个文件前提示确认\n  /F          强制删除只读文件\n  /S          从所有子目录删除指定文件\n  /Q          安静模式，不提示确认\n  /A          根据属性选择要删除的文件"},
    {"dir",      "显示一个目录中的文件和子目录。\n\nDIR [drive:][path][filename] [/A[[:]attributes]] [/B] [/C] [/D] [/L] [/N] [/O[[:]sortorder]] [/P] [/Q] [/S] [/T[[:]timefield]] [/W] [/X] [/4]\n\n  常用选项：\n  /P     分页显示\n  /W     宽格式显示\n  /S     递归显示子目录"},
    {"diskpart", "显示或配置磁盘分区属性。\n\nDISKPART\n\n  进入 DiskPart 交互式命令行。"},
    {"doskey",   "编辑命令行、撤回 Windows 命令并创建宏。\n\nDOSKEY [/REINSTALL] [/LISTSIZE=size] [/MACROS[:ALL | :exename]] [/HISTORY] [/INSERT | /OVERSTRIKE] [/EXENAME=exename] [macro=text]"},
    {"driverquery", "显示当前设备驱动程序状态和属性。\n\nDRIVERQUERY [/S system [/U username [/P [password]]]] [/FO format] [/NH] [/SI] [/V]\n\n  /FO   输出格式: TABLE, LIST, CSV\n  /V    详细模式"},
    {"echo",     "显示消息，或将命令回显打开或关闭。\n\nECHO [ON | OFF]\nECHO [message]\n\n  不带参数时显示当前回显设置。"},
    {"endlocal", "结束批文件中环境更改的本地化。\n\nENDLOCAL\n\n  在 SETLOCAL 之后使用，恢复先前的环境。"},
    {"erase",    "同 DEL，删除一个或多个文件。"},
    {"exit",     "退出 CMD.EXE 程序(命令解释程序)。\n\nEXIT [/B] [exitCode]\n\n  /B         指定只退出当前批处理脚本\n  exitCode   指定退出代码"},
    {"fc",       "比较两个文件或两个文件集并显示它们之间的不同。\n\nFC [/A] [/C] [/L] [/LBn] [/N] [/OFF[LINE]] [/T] [/U] [/W] [/nnnn] [drive1:][path1]filename1 [drive2:][path2]filename2"},
    {"find",     "在一个或多个文件中搜索一个文本字符串。\n\nFIND [/V] [/C] [/N] [/I] [/OFF[LINE]] \"string\" [[drive:][path]filename[ ...]]\n\n  /V   显示所有未包含指定字符串的行\n  /C   仅显示包含字符串的行数\n  /N   显示行号\n  /I   忽略大小写"},
    {"findstr",  "在多个文件中搜索字符串。\n\nFINDSTR [/B] [/E] [/L] [/R] [/S] [/I] [/X] [/V] [/N] [/M] [/O] [/P] [/F:file] [/C:string] [/G:file] [/D:dir list] [/A:color] [strings] [[drive:][path]filename[ ...]]"},
    {"for",      "为一组文件中的每个文件运行一个指定的命令。\n\nFOR %variable IN (set) DO command [command-parameters]\n\n  %variable  可替换参数\n  (set)      指定一个或多个文件，可使用通配符\n  command    指定要对每个文件执行的命令"},
    {"format",   "格式化磁盘，以便用于 Windows。\n\nFORMAT volume [/FS:file-system] [/V:label] [/Q] [/A:size] [/C] [/X]\n\n  volume        驱动器号、装载点或卷名\n  /FS:file-system 指定文件系统类型(FAT, exFAT, NTFS, UDF)\n  /V:label      指定卷标\n  /Q            快速格式化"},
    {"fsutil",   "显示或配置文件系统属性。\n\nFSUTIL [behavior | dirty | file | fsinfo | hardlink | objectid | quota | reparsepoint | sparse | usn | volume] ..."},
    {"ftype",    "显示或修改在文件扩展名关联中使用的文件类型。\n\nFTYPE [fileType[=[openCommandString]]]\n\n  fileType   指定要检查或更改的文件类型\n  openCommandString  指定打开此类文件时要使用的可执行程序"},
    {"goto",     "将 Windows 命令解释程序定向到批处理程序中某个带标签的行。\n\nGOTO label\n\n  label  批处理程序中的标签"},
    {"gpresult", "显示计算机或用户的组策略信息。\n\nGPRESULT [/S system [/U username [/P [password]]]] [/SCOPE scope] [/USER targetusername] [/R | /V | /Z]"},
    {"help",     "提供 Windows 命令的帮助信息。\n\nHELP [command]\n\n  command  显示该命令的详细帮助信息。不带参数时列出所有可用命令。"},
    {"icacls",   "显示、修改、备份或还原文件和目录的 ACL。\n\nICACLS name /save aclfile [/T] [/C] [/L] [/Q]\nICACLS directory [/substitute SidOld SidNew [...]] /restore aclfile [/C] [/L] [/Q]"},
    {"if",       "在批处理程序中执行有条件的处理操作。\n\nIF [NOT] ERRORLEVEL number command\nIF [NOT] string1==string2 command\nIF [NOT] EXIST filename command"},
    {"label",    "创建、更改或删除磁盘的卷标。\n\nLABEL [drive:][label]\nLABEL [/MP] [volume] [label]\n\n  /MP  指定卷应被视为装载点或卷名"},
    {"md",       "创建一个目录。\n\nMD [drive:]path"},
    {"mkdir",    "同 MD，创建一个目录。"},
    {"mklink",   "创建符号链接和硬链接。\n\nMKLINK [[/D] | [/H] | [/J]] Link Target\n\n  /D  创建目录符号链接\n  /H  创建硬链接\n  /J  创建目录连接点"},
    {"mode",     "配置系统设备。\n\nMODE 显示所有设备的状态\nMODE [device] [/STATUS]\nMODE LPTn[:] [c][,[l][,r]]\nMODE CON[:] [COLS=c] [LINES=n]\n..."},
    {"more",     "逐屏显示输出。\n\nMORE [/E [/C] [/P] [/S] [/Tn] [+n]] < [drive:][path]filename\ncommand | MORE [/E [/C] [/P] [/S] [/Tn] [+n]]\n\n  /E  启用扩展功能\n  /C  清除屏幕后显示\n  /P  扩展字符\n  /S  将多个空白行缩为一行\n  /Tn 将制表符扩展为 n 个空格"},
    {"move",     "将一个或多个文件从一个目录移动到另一个目录。\n\nMOVE [/Y | /-Y] [drive:][path]filename1[,...] destination\n\n  /Y   禁止提示确认覆盖\n  /-Y  提示确认覆盖"},
    {"openfiles","显示远程用户为了文件共享而打开的文件。\n\nOPENFILES [/parameter] ..."},
    {"path",     "为可执行文件显示或设置搜索路径。\n\nPATH [[drive:]path[;...][;%PATH%]\nPATH ;"},
    {"pause",    "暂停批处理文件的处理并显示消息。\n\nPAUSE"},
    {"popd",     "还原通过 PUSHD 保存的当前目录的上一个值。\n\nPOPD"},
    {"print",    "打印一个文本文件。\n\nPRINT [/D:device] [[drive:][path]filename[ ...]]"},
    {"prompt",   "更改 Windows 命令提示。\n\nPROMPT [text]\n\n  text  指定新的命令提示字符串"},
    {"pushd",    "保存当前目录，然后对其进行更改。\n\nPUSHD [path | ..]\n\n  如果路径指定了网络位置，则 PUSHD 会创建一个临时驱动器号，然后切换到该驱动器。"},
    {"rd",       "删除目录。\n\nRD [/S] [/Q] [drive:]path\n\n  /S  删除指定目录和所有子目录及文件\n  /Q  安静模式，不要求确认"},
    {"recover",  "从损坏的或有缺陷的磁盘中恢复可读信息。\n\nRECOVER [drive:][path]filename"},
    {"rem",      "记录批处理文件或 CONFIG.SYS 中的注释(批注)。\n\nREM [comment]"},
    {"ren",      "重命名文件。\n\nRENAME [drive:][path]filename1 filename2\nREN [drive:][path]filename1 filename2"},
    {"rename",   "同 REN，重命名文件。"},
    {"replace",  "替换文件。\n\nREPLACE [drive1:][path1]filename [drive2:][path2] [/A] [/P] [/R] [/W]\nREPLACE [drive1:][path1]filename [drive2:][path2] [/P] [/R] [/S] [/W] [/U]"},
    {"rmdir",    "同 RD，删除目录。"},
    {"robocopy", "复制文件和目录树的高级实用工具。\n\nROBOCOPY source destination [file [file]...] [options]\n\n常用选项:\n  /S   复制子目录，非空\n  /E   复制子目录，包括空目录\n  /MOVE 移动文件\n  /MIR  镜像目录树"},
    {"set",      "显示、设置或删除 Windows 环境变量。\n\nSET [variable=[string]]\n\n  variable  指定环境变量名\n  string    指定要指派给变量的一系列字符"},
    {"setlocal", "开始本地化批处理文件中的环境更改。\n\nSETLOCAL\nSETLOCAL ENABLEDELAYEDEXPANSION\nSETLOCAL DISABLEDELAYEDEXPANSION"},
    {"sc",       "显示或配置服务(后台进程)。\n\nSC [\\server] command [service name] [options]"},
    {"schtasks", "安排在一台计算机上运行命令和程序。\n\nSCHTASKS /parameter [arguments]"},
    {"shift",    "调整批处理文件中可替换参数的位置。\n\nSHIFT [/n]\n\n  /n  从第 n 个参数开始移位"},
    {"shutdown", "允许通过本地或远程方式正确关闭计算机。\n\nSHUTDOWN [/i | /l | /s | /r | /g | /a | /p | /h | /e | /o] [/f] [/m \\computer] [/t xxx] [/d [p|u:]xx:yy] [/c \"comment\"]"},
    {"sort",     "对输入排序。\n\nSORT [/R] [/+n] [/M kilobytes] [/L locale] [/REC recordbytes] [[drive1:][path1]filename1] [/T [drive2:][path2]] [/O [drive3:][path3]filename3]\n\n  /R  反向排序\n  /+n 从第 n 列开始排序"},
    {"start",    "启动单独的窗口以运行指定的程序或命令。\n\nSTART [\"title\"] [/D path] [/I] [/MIN] [/MAX] [/SEPARATE | /SHARED] [/LOW | /NORMAL | /HIGH | /REALTIME | /ABOVENORMAL | /BELOWNORMAL] [/AFFINITY <hex affinity>] [/WAIT] [/B] [command/program] [parameters]"},
    {"subst",    "将路径与驱动器号关联。\n\nSUBST [drive1: [drive2:]path]\nSUBST drive1: /D\n\n  /D  删除虚拟驱动器"},
    {"systeminfo","显示计算机的特定属性和配置。\n\nSYSTEMINFO [/S system [/U username [/P [password]]]] [/FO format] [/NH]"},
    {"tasklist", "显示包括服务在内的所有当前运行的任务。\n\nTASKLIST [/S system [/U username [/P [password]]]] [/M [module] | /SVC | /V] [/FI filter] [/FO format] [/NH]"},
    {"taskkill", "中止或停止正在运行的进程或应用程序。\n\nTASKKILL [/S system [/U username [/P [password]]]] { [/FI filter] [/PID processid | /IM imagename] } [/F] [/T]"},
    {"time",     "显示或设置系统时间。\n\nTIME [/T | time]"},
    {"title",    "设置 CMD.EXE 会话的窗口标题。\n\nTITLE [string]"},
    {"tree",     "以图形方式显示驱动程序或路径的目录结构。\n\nTREE [drive:][path] [/F] [/A]\n\n  /F  显示每个文件夹中的文件名\n  /A  使用 ASCII 字符代替图形字符"},
    {"type",     "显示文本文件的内容。\n\nTYPE [drive:][path]filename"},
    {"ver",      "显示 Windows 的版本。\n\nVER"},
    {"verify",   "告诉 Windows 是否进行验证，以确保文件正确写入磁盘。\n\nVERIFY [ON | OFF]\n\n  不带参数显示当前设置。"},
    {"vol",      "显示磁盘卷标和序列号。\n\nVOL [drive:]"},
    {"xcopy",    "复制文件和目录树。\n\nXCOPY source [destination] [/A | /M] [/D[:date]] [/P] [/S [/E]] [/V] [/W] [/C] [/I] [/Q] [/F] [/L] [/G] [/H] [/R] [/T] [/U] [/K] [/N] [/O] [/X] [/Y] [/-Y] [/Z] [/EXCLUDE:file1[+file2]...]"},
    {"wmic",     "在交互式命令 shell 中显示 WMI 信息。\n\nWMIC [global switches] [alias] [verbs] [properties]\n\n  常用别名: process, service, logicaldisk, cpu"}
};

// 总帮助列表（原版输出）
const char* help_list = R"(有关某个命令的详细信息，请键入 HELP 命令名
ASSOC          显示或修改文件扩展名关联。
ATTRIB         显示或更改文件属性。
BREAK          设置或清除扩展式 CTRL+C 检查。
BCDEDIT        设置启动数据库中的属性以控制启动加载。
CACLS          显示或修改文件的访问控制列表(ACL)。
CALL           从另一个批处理程序调用这一个。
CD             显示当前目录的名称或将其更改。
CHCP           显示或设置活动代码页数。
CHDIR          显示当前目录的名称或将其更改。
CHKDSK         检查磁盘并显示状态报告。
CHKNTFS        显示或修改启动时间磁盘检查。
CLS            清除屏幕。
CMD            打开另一个 Windows 命令解释程序窗口。
COLOR          设置默认控制台前景和背景颜色。
COMP           比较两个或两套文件的内容。
COMPACT        显示或更改 NTFS 分区上文件的压缩。
CONVERT        将 FAT 卷转换成 NTFS。你不能转换
               当前驱动器。
COPY           将至少一个文件复制到另一个位置。
DATE           显示或设置日期。
DEL            删除至少一个文件。
DIR            显示一个目录中的文件和子目录。
DISKPART       显示或配置磁盘分区属性。
DOSKEY         编辑命令行、撤回 Windows 命令并
               创建宏。
DRIVERQUERY    显示当前设备驱动程序状态和属性。
ECHO           显示消息，或将命令回显打开或关闭。
ENDLOCAL       结束批文件中环境更改的本地化。
ERASE          删除一个或多个文件。
EXIT           退出 CMD.EXE 程序(命令解释程序)。
FC             比较两个文件或两个文件集并显示
               它们之间的不同。
FIND           在一个或多个文件中搜索一个文本字符串。
FINDSTR        在多个文件中搜索字符串。
FOR            为一组文件中的每个文件运行一个指定的命令。
FORMAT         格式化磁盘，以便用于 Windows。
FSUTIL         显示或配置文件系统属性。
FTYPE          显示或修改在文件扩展名关联中使用的文件
               类型。
GOTO           将 Windows 命令解释程序定向到批处理程序
               中某个带标签的行。
GPRESULT       显示计算机或用户的组策略信息。
HELP           提供 Windows 命令的帮助信息。
ICACLS         显示、修改、备份或还原文件和
               目录的 ACL。
IF             在批处理程序中执行有条件的处理操作。
LABEL          创建、更改或删除磁盘的卷标。
MD             创建一个目录。
MKDIR          创建一个目录。
MKLINK         创建符号链接和硬链接
MODE           配置系统设备。
MORE           逐屏显示输出。
MOVE           将一个或多个文件从一个目录移动到另一个
               目录。
OPENFILES      显示远程用户为了文件共享而打开的文件。
PATH           为可执行文件显示或设置搜索路径。
PAUSE          暂停批处理文件的处理并显示消息。
POPD           还原通过 PUSHD 保存的当前目录的上一个
               值。
PRINT          打印一个文本文件。
PROMPT         更改 Windows 命令提示。
PUSHD          保存当前目录，然后对其进行更改。
RD             删除目录。
RECOVER        从损坏的或有缺陷的磁盘中恢复可读信息。
REM            记录批处理文件或 CONFIG.SYS 中的注释(批注)。
REN            重命名文件。
RENAME         重命名文件。
REPLACE        替换文件。
RMDIR          删除目录。
ROBOCOPY       复制文件和目录树的高级实用工具
SET            显示、设置或删除 Windows 环境变量。
SETLOCAL       开始本地化批处理文件中的环境更改。
SC             显示或配置服务(后台进程)。
SCHTASKS       安排在一台计算机上运行命令和程序。
SHIFT          调整批处理文件中可替换参数的位置。
SHUTDOWN       允许通过本地或远程方式正确关闭计算机。
SORT           对输入排序。
START          启动单独的窗口以运行指定的程序或命令。
SUBST          将路径与驱动器号关联。
SYSTEMINFO     显示计算机的特定属性和配置。
TASKLIST       显示包括服务在内的所有当前运行的任务。
TASKKILL       中止或停止正在运行的进程或应用程序。
TIME           显示或设置系统时间。
TITLE          设置 CMD.EXE 会话的窗口标题。
TREE           以图形方式显示驱动程序或路径的目录
               结构。
TYPE           显示文本文件的内容。
VER            显示 Windows 的版本。
VERIFY         告诉 Windows 是否进行验证，以确保文件
               正确写入磁盘。
VOL            显示磁盘卷标和序列号。
XCOPY          复制文件和目录树。
WMIC           在交互式命令 shell 中显示 WMI 信息。

有关工具的详细信息，请参阅联机帮助中的命令行参考。
)";

int main(int argc, char* argv[]) {
    if (argc > 1) {
        // 用户键入了 HELP 命令名
        std::string cmd = to_lower(argv[1]);
        auto it = command_help.find(cmd);
        if (it != command_help.end()) {
            std::cout << it->second << std::endl;
        }
        else {
            std::cout << "此命令不受 HELP 支持。请尝试 \"HELP\" 以获取可用命令列表。" << std::endl;
        }
    }
    else {
        // 无参数，输出总列表
        std::cout << help_list;
    }
    return 0;
}