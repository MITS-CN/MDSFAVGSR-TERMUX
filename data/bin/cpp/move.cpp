#include <iostream>
#include <filesystem>
namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: move source destination" << std::endl;
        return 1;
    }
    try {
        fs::path src(argv[1]);
        fs::path dst(argv[2]);
        // 若目标为已存在目录，则将源移动到该目录下
        if (fs::exists(dst) && fs::is_directory(dst)) {
            dst = dst / src.filename();
        }
        fs::rename(src, dst);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "move: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}