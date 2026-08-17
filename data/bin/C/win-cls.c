#include <stdio.h>

int main() {
    // 清屏并移动光标到左上角
    printf("\033[2J\033[H");
    return 0;
}