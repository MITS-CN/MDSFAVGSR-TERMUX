#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

void show_help(const char *prog) {
    printf("用法: %s [/D | /H | /J] <链接名> <目标>\n", prog);
    printf("  /D    创建目录符号链接\n");
    printf("  /H    创建硬链接\n");
    printf("  /J    目录联接 (Linux下等同于 /D)\n");
    printf("  无选项 创建文件符号链接\n");
}

int main(int argc, char *argv[]) {
    if (argc < 3 || strcmp(argv[1], "/?") == 0) {
        show_help(argv[0]);
        return 0;
    }

    int idx = 1;
    int link_type = 0; // 0=symlink_file, 1=symlink_dir, 2=hardlink
    if (argv[1][0] == '/') {
        if (strcmp(argv[1], "/D") == 0 || strcmp(argv[1], "/d") == 0) {
            link_type = 1;
            idx++;
        } else if (strcmp(argv[1], "/H") == 0 || strcmp(argv[1], "/h") == 0) {
            link_type = 2;
            idx++;
        } else if (strcmp(argv[1], "/J") == 0 || strcmp(argv[1], "/j") == 0) {
            fprintf(stderr, "警告: /J 在Linux下不支持，将创建目录符号链接代替。\n");
            link_type = 1;
            idx++;
        } else {
            fprintf(stderr, "未知选项: %s\n", argv[1]);
            show_help(argv[0]);
            return 1;
        }
    }

    if (argc - idx < 2) {
        fprintf(stderr, "错误: 需要指定链接名和目标。\n");
        show_help(argv[0]);
        return 1;
    }

    const char *linkname = argv[idx];
    const char *target = argv[idx + 1];

    if (access(target, F_OK) != 0) {
        fprintf(stderr, "错误: 目标 '%s' 不存在。\n", target);
        return 1;
    }

    int ret;
    if (link_type == 2) { // hardlink
        ret = link(target, linkname);
    } else { // symlink (file or dir)
        ret = symlink(target, linkname);
    }

    if (ret == 0) {
        printf("成功创建: %s -> %s\n", linkname, target);
        return 0;
    } else {
        perror("创建失败");
        return 1;
    }
}