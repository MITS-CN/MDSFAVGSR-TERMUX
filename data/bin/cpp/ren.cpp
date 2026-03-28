#include <iostream>
#include <cstdio>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ren oldname newname" << std::endl;
        return 1;
    }
    if (std::rename(argv[1], argv[2]) != 0) {
        std::cerr << "ren: cannot rename '" << argv[1] << "' to '" << argv[2] << "'" << std::endl;
        return 1;
    }
    return 0;
}