#include <iostream>
#include <iostream>
#include <fstream>
#include </storage/emulated/0/MITS/TEMP/json.hpp>  // 如果头文件在子目录中，可能需要 "nlohmann/json.hpp" 或直接 "json.hpp"

using json = nlohmann::json;

int main() {
    // 读取 JSON 文件
    std::ifstream file("/data/data/com.termux/files/usr/etc/MITS/config.json");
    if (!file.is_open()) {
        std::cerr << "无法打开 config.json" << std::endl;
        return 1;
    }

    json data;
    file >> data;  // 从文件流解析 JSON
    file.close();

    // 获取配置项（如果键不存在，则使用提供的默认值）
    std::string version = data.value("MITS_version", "Build.IS0000(main:ERROR)_ERORR");
    std::string builder = data.value("MITS_build_by", "ERROR");

    // 输出
    std::cout << "" << std::endl;
    std::cout << "这是一个基于termux的定制界面（还在内测中）" << std::endl;
    std::cout << "MITS_version " << version << std::endl;
    std::cout << "MITS_build_by " << builder << std::endl;
    std::cout << "" << std::endl;
    std::cout << "正文：" << std::endl;
    std::cout << "欢迎使用Win_termux" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "我们使用了大量开源项目来构造此项目" << std::endl;
    std::cout << "此版本为雏形，欢迎各位来github开发" << std::endl;
    std::cout << "顺便一说" << std::endl;
    std::cout << "在构造此项目时" << std::endl;
    std::cout << "我们使用了ai辅助开发" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "下面是原版help 信息" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "GNU bash, version 5.3.9(1)-release (aarch64-unknown-linux-android)\n"
             "These shell commands are defined internally.  Type `help' to see this list.\n"
             "Type `help name' to find out more about the function `name'.\n"
             "Use `info bash' to find out more about the shell in general.\n"
             "Use `man -k' or `info' to find out more about commands not in this list.\n"
             "\n"
             "A star (*) next to a name means that the command is disabled.\n"
             "\n"
             " ! PIPELINE                                           history [-c] [-d offset] [n] or history -anrw [fi>\n"
             " job_spec [&]                                         if COMMANDS; then COMMANDS; [ elif COMMANDS; then>\n"
             " (( expression ))                                     jobs [-lnprs] [jobspec ...] or jobs -x command [a>\n"
             " . [-p path] filename [arguments]                     kill [-s sigspec | -n signum | -sigspec] pid | jo>\n"
             " :                                                    let arg [arg ...]\n"
             " [ arg... ]                                           local [option] name[=value] ...\n"
             " [[ expression ]]                                     logout [n]\n"
             " alias [-p] [name[=value] ... ]                       mapfile [-d delim] [-n count] [-O origin] [-s cou>\n"
             " bg [job_spec ...]                                    popd [-n] [+N | -N]\n"
             " bind [-lpsvPSVX] [-m keymap] [-f filename] [-q nam>  printf [-v var] format [arguments]\n"
             " break [n]                                            pushd [-n] [+N | -N | dir]\n"
             " builtin [shell-builtin [arg ...]]                    pwd [-LP]\n"
             " caller [expr]                                        read [-Eers] [-a array] [-d delim] [-i text] [-n >\n"
             " case WORD in [PATTERN [| PATTERN]...) COMMANDS ;;]>  readarray [-d delim] [-n count] [-O origin] [-s c>\n"
             " cd [-L|[-P [-e]]] [-@] [dir]                         readonly [-aAf] [name[=value] ...] or readonly -p\n"
             " command [-pVv] command [arg ...]                     return [n]\n"
             " compgen [-V varname] [-abcdefgjksuv] [-o option] [>  select NAME [in WORDS ... ;] do COMMANDS; done\n"
             " complete [-abcdefgjksuv] [-pr] [-DEI] [-o option] >  set [-abefhkmnptuvxBCEHPT] [-o option-name] [--] >\n"
             " compopt [-o|+o option] [-DEI] [name ...]             shift [n]\n"
             " continue [n]                                         shopt [-pqsu] [-o] [optname ...]\n"
             " coproc [NAME] command [redirections]                 source [-p path] filename [arguments]\n"
             " declare [-aAfFgiIlnrtux] [name[=value] ...] or dec>  suspend [-f]\n"
             " dirs [-clpv] [+N] [-N]                               test [expr]\n"
             " disown [-h] [-ar] [jobspec ... | pid ...]            time [-p] pipeline\n"
             " echo [-neE] [arg ...]                                times\n"
             " enable [-a] [-dnps] [-f filename] [name ...]         trap [-Plp] [[action] signal_spec ...]\n"
             " eval [arg ...]                                       true\n"
             " exec [-cl] [-a name] [command [argument ...]] [red>  type [-afptP] name [name ...]\n"
             " exit [n]                                             typeset [-aAfFgiIlnrtux] name[=value] ... or type>\n"
             " export [-fn] [name[=value] ...] or export -p [-f]    ulimit [-SHabcdefiklmnpqrstuvxPRT] [limit]\n"
             " false                                                umask [-p] [-S] [mode]\n"
             " fc [-e ename] [-lnr] [first] [last] or fc -s [pat=>  unalias [-a] name [name ...]\n"
             " fg [job_spec]                                        unset [-f] [-v] [-n] [name ...]\n"
             " for NAME [in WORDS ... ] ; do COMMANDS; done         until COMMANDS; do COMMANDS-2; done\n"
             " for (( exp1; exp2; exp3 )); do COMMANDS; done        variables - Names and meanings of some shell vari>\n"
             " function name { COMMANDS ; } or name () { COMMANDS>  wait [-fn] [-p var] [id ...]\n"
             " getopts optstring name [arg ...]                     while COMMANDS; do COMMANDS-2; done\n"
             " hash [-lr] [-p pathname] [-dt] [name ...]            { COMMANDS ; }\n"
             " help [-dms] [pattern ...]" << std::endl;
    return 0;
}