#include <iostream>
#include <fstream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: type file1 [file2 ...]" << std::endl;
        return 1;
    }
    for (int i = 1; i < argc; ++i) {
        std::ifstream file(argv[i]);
        if (!file) {
            std::cerr << "type: " << argv[i] << ": No such file or directory" << std::endl;
            continue;
        }
        std::cout << file.rdbuf();
        file.close();
    }
    return 0;
}