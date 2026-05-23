#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>
#include <signal.h>

// 忽略大小写比较
#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

// 终端设置 RAII 封装
class TerminalGuard {
    struct termios oldt;
    bool saved;
    bool active;
public:
    TerminalGuard() : saved(false), active(false) {}
    
    void set_nonblock(bool enable) {
        if (!isatty(STDIN_FILENO)) return;
        if (!saved) {
            tcgetattr(STDIN_FILENO, &oldt);
            saved = true;
        }
        struct termios newt = oldt;
        if (enable) {
            newt.c_lflag &= ~(ICANON | ECHO);
            newt.c_cc[VMIN] = 0;
            newt.c_cc[VTIME] = 0;
        }
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        active = enable;
    }
    
    ~TerminalGuard() {
        if (saved) {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        }
    }
};

void show_help() {
    std::cout << 
        "TIMEOUT [/T] timeout [/NOBREAK]\n\n"
        "Description:\n"
        "    Pauses the execution and waits for a specified number of seconds\n"
        "    or until a key is pressed. If no key is pressed, it continues\n"
        "    after the timeout.\n\n"
        "Parameters:\n"
        "    timeout    The number of seconds to wait (-1 to wait indefinitely\n"
        "               for a key press). Range: -1 .. 99999.\n"
        "    /T         Specifies the timeout value (optional).\n"
        "    /NOBREAK   Ignore key presses during the wait.\n\n"
        "Examples:\n"
        "    timeout 10\n"
        "    timeout /t 30 /nobreak\n"
        "    timeout -1\n";
}

// 清空输入缓冲区
void drain_input() {
    char buf[256];
    while (read(STDIN_FILENO, buf, sizeof(buf)) > 0) {}
}

int main(int argc, char* argv[]) {
    bool nobreak = false;
    int timeout = 0;
    int argIndex = 1;
    
    // 帮助请求
    if (argc >= 2 && (strcmp(argv[1], "/?") == 0 || strcmp(argv[1], "-?") == 0 || 
                      strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        show_help();
        return 0;
    }
    
    // 解析 /T 参数
    if (argc >= 2 && (strcasecmp(argv[1], "/t") == 0 || strcasecmp(argv[1], "-t") == 0)) {
        if (argc < 3) {
            std::cerr << "Error: Missing timeout value.\n";
            return 1;
        }
        timeout = std::atoi(argv[2]);
        argIndex = 3;
    } else if (argc >= 2) {
        timeout = std::atoi(argv[1]);
        argIndex = 2;
    } else {
        std::cerr << "Error: Missing timeout value.\n";
        show_help();
        return 1;
    }
    
    // 验证范围
    if (timeout < -1 || timeout > 99999) {
        std::cerr << "Error: Timeout must be between -1 and 99999.\n";
        return 1;
    }
    
    // 解析剩余参数
    for (int i = argIndex; i < argc; ++i) {
        if (strcasecmp(argv[i], "/nobreak") == 0) {
            nobreak = true;
        } else {
            std::cerr << "Unknown parameter: " << argv[i] << "\n";
            return 1;
        }
    }
    
    // -1 仅用于无限等待按键，不能与 /nobreak 组合
    if (timeout == -1 && nobreak) {
        std::cerr << "Error: /NOBREAK cannot be used with an infinite timeout (-1).\n";
        return 1;
    }
    
    if (timeout == 0) {
        return 0; // 等待 0 秒，直接退出
    }
    
    // 非终端自动启用 nobreak
    if (!isatty(STDIN_FILENO)) {
        nobreak = true;
    }
    
    TerminalGuard guard;
    if (!nobreak) {
        guard.set_nonblock(true);
    }
    
    int remaining = timeout;
    bool keyPressed = false;
    
    // 主循环
    while (true) {
        // 显示倒计时
        if (timeout == -1) {
            std::cout << "\rWaiting indefinitely, press a key to continue ... " << std::flush;
        } else if (nobreak) {
            std::cout << "\rWaiting for " << remaining << " seconds, no input accepted... " << remaining << std::flush;
        } else {
            std::cout << "\rWaiting for " << remaining << " seconds, press a key to continue ... " << remaining << std::flush;
        }
        
        // 检查终止条件
        if (timeout != -1 && remaining <= 0) {
            break;
        }
        
        // 等待 100ms 并检查输入
        struct timespec sleepTime = {0, 100000000}; // 100 ms
        bool secondElapsed = false;
        for (int i = 0; i < 10 && !secondElapsed; ++i) {
            // 睡眠，处理信号中断
            int ret;
            do {
                ret = nanosleep(&sleepTime, &sleepTime);
            } while (ret == -1 && errno == EINTR);
            
            if (!nobreak) {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(STDIN_FILENO, &fds);
                struct timeval tv = {0, 0};
                int sel_ret;
                do {
                    sel_ret = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
                } while (sel_ret == -1 && errno == EINTR);
                
                if (sel_ret > 0) {
                    drain_input();
                    keyPressed = true;
                    secondElapsed = true; // exit inner loop
                    break;
                }
            }
        }
        
        if (keyPressed) {
            break;
        }
        
        if (timeout != -1) {
            --remaining;
        }
    }
    
    // 恢复终端（析构自动）
    std::cout << std::endl; // 换行
    
    if (keyPressed) {
        std::cout << "Key pressed. Exiting.\n";
        return 1; // 非零表示被按键中断
    }
    
    return 0;
}