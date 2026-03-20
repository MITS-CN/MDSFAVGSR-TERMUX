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
    std::string version = data.value("MITS_build_version", "0000");
    std::string builder = data.value("MITS_build_by", "NULL");

    // 输出
    std::cout << "MITS_build_version " << version << std::endl;
    std::cout << "MITS_build_by " << builder << std::endl;

    return 0;
}