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
    system("help");
    return 0;
}