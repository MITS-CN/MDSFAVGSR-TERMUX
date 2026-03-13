#include <iostream>
#include <termios.h>
#include <unistd.h>

int main() {
    std::cout << "Press any key to continue . . . " << std::flush;

    // 保存当前终端设置
    struct termios old_tio, new_tio;
    tcgetattr(STDIN_FILENO, &old_tio);
    new_tio = old_tio;

    // 禁用规范模式和回显，使输入立即生效
    new_tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

    // 读取一个字符
    getchar();

    // 恢复原始终端设置
    tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);

    return 0;
}