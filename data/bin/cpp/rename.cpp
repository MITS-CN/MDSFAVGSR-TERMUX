#include <iostream>
#include <cstdio> // for rename

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: rename oldname newname" << std::endl;
        return 1;
    }
    if (std::rename(argv[1], argv[2]) != 0) {
        std::cerr << "rename: cannot rename '" << argv[1] << "' to '" << argv[2] << "'" << std::endl;
        return 1;
    }
    return 0;
}