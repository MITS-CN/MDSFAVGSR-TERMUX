#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 1024

typedef struct {
    char name[64];
    unsigned long size;
    int used;
    char dependency[256];
    char status[16];
    char address[32];
} ModuleInfo;

int get_modules(ModuleInfo **modules) {
    FILE *fp = fopen("/proc/modules", "r");
    if (!fp) {
        perror("无法打开 /proc/modules");
        return -1;
    }
    char line[MAX_LINE];
    int count = 0;
    int capacity = 32;
    *modules = malloc(capacity * sizeof(ModuleInfo));
    if (!*modules) {
        fclose(fp);
        return -1;
    }
    while (fgets(line, sizeof(line), fp)) {
        if (count >= capacity) {
            capacity *= 2;
            *modules = realloc(*modules, capacity * sizeof(ModuleInfo));
            if (!*modules) {
                fclose(fp);
                return -1;
            }
        }
        ModuleInfo *m = &(*modules)[count];
        sscanf(line, "%63s %lu %d %255s %15s %31s",
               m->name, &m->size, &m->used, m->dependency, m->status, m->address);
        count++;
    }
    fclose(fp);
    return count;
}

void format_size(unsigned long size, char *buf) {
    if (size >= 1024)
        sprintf(buf, "%lu KB", size / 1024);
    else
        sprintf(buf, "%lu B", size);
}

void print_table(ModuleInfo *modules, int count, int no_header, int verbose) {
    if (!no_header) {
        if (verbose) {
            printf("%-20s %10s %8s %-20s %-10s %-16s\n",
                   "模块名称", "大小", "引用数", "依赖模块", "状态", "内存地址");
            printf("%s\n", "-------------------------------------------------------------------------------");
        } else {
            printf("%-20s %10s %-20s\n", "模块名称", "大小", "依赖模块");
            printf("%s\n", "-------------------------------------------------------");
        }
    }
    for (int i = 0; i < count; i++) {
        ModuleInfo *m = &modules[i];
        char size_str[32];
        format_size(m->size, size_str);
        if (verbose) {
            printf("%-20s %10s %8d %-20s %-10s %-16s\n",
                   m->name, size_str, m->used,
                   strcmp(m->dependency, "-") == 0 ? "(无)" : m->dependency,
                   m->status, m->address);
        } else {
            printf("%-20s %10s %-20s\n",
                   m->name, size_str,
                   strcmp(m->dependency, "-") == 0 ? "(无)" : m->dependency);
        }
    }
}

void print_csv(ModuleInfo *modules, int count, int verbose) {
    if (verbose) {
        printf("\"模块名称\",\"大小(KB)\",\"引用数\",\"依赖模块\",\"状态\",\"内存地址\"\n");
    } else {
        printf("\"模块名称\",\"大小(KB)\",\"依赖模块\"\n");
    }
    for (int i = 0; i < count; i++) {
        ModuleInfo *m = &modules[i];
        double size_kb = m->size / 1024.0;
        if (verbose) {
            printf("\"%s\",%.2f,%d,\"%s\",\"%s\",\"%s\"\n",
                   m->name, size_kb, m->used,
                   strcmp(m->dependency, "-") == 0 ? "" : m->dependency,
                   m->status, m->address);
        } else {
            printf("\"%s\",%.2f,\"%s\"\n",
                   m->name, size_kb,
                   strcmp(m->dependency, "-") == 0 ? "" : m->dependency);
        }
    }
}

void print_list(ModuleInfo *modules, int count, int verbose) {
    for (int i = 0; i < count; i++) {
        ModuleInfo *m = &modules[i];
        char size_str[32];
        format_size(m->size, size_str);
        printf("驱动名称: %s\n", m->name);
        printf("文件大小: %s\n", size_str);
        if (verbose) {
            printf("引用计数: %d\n", m->used);
            printf("依赖模块: %s\n", strcmp(m->dependency, "-") == 0 ? "(无)" : m->dependency);
            printf("状态: %s\n", m->status);
            printf("内存地址: %s\n", m->address);
            printf("\n");
        } else {
            printf("依赖模块: %s\n", strcmp(m->dependency, "-") == 0 ? "(无)" : m->dependency);
            printf("\n");
        }
    }
}

void show_help(const char *prog) {
    printf("用法: %s [/v] [/fo table|csv|list] [/nh] [/?]\n", prog);
    printf("  /v          显示详细信息 (大小、引用计数、依赖、状态、地址)\n");
    printf("  /fo table   以表格格式输出 (默认)\n");
    printf("  /fo csv     以 CSV 格式输出\n");
    printf("  /fo list    以列表格式输出\n");
    printf("  /nh         不显示表头 (仅对 table/csv 有效)\n");
    printf("  /?          显示帮助\n");
}

int main(int argc, char *argv[]) {
    int verbose = 0;
    int no_header = 0;
    const char *format = "table";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "/v") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "/nh") == 0) {
            no_header = 1;
        } else if (strcmp(argv[i], "/?") == 0 || strcmp(argv[i], "/h") == 0) {
            show_help(argv[0]);
            return 0;
        } else if (strncmp(argv[i], "/fo", 3) == 0) {
            if (i + 1 < argc) {
                if (strcmp(argv[i+1], "table") == 0) format = "table";
                else if (strcmp(argv[i+1], "csv") == 0) format = "csv";
                else if (strcmp(argv[i+1], "list") == 0) format = "list";
                else {
                    fprintf(stderr, "无效的 /fo 参数: %s\n", argv[i+1]);
                    show_help(argv[0]);
                    return 1;
                }
                i++; // 跳过参数值
            } else {
                fprintf(stderr, "缺少 /fo 参数\n");
                show_help(argv[0]);
                return 1;
            }
        } else {
            fprintf(stderr, "未知参数: %s\n", argv[i]);
            show_help(argv[0]);
            return 1;
        }
    }

    ModuleInfo *modules;
    int count = get_modules(&modules);
    if (count < 0) {
        fprintf(stderr, "无法读取驱动程序列表。\n");
        return 2;
    }
    if (count == 0) {
        printf("没有找到任何内核模块（驱动程序）。\n");
        free(modules);
        return 0;
    }

    if (strcmp(format, "table") == 0)
        print_table(modules, count, no_header, verbose);
    else if (strcmp(format, "csv") == 0)
        print_csv(modules, count, verbose);
    else if (strcmp(format, "list") == 0)
        print_list(modules, count, verbose);

    free(modules);
    return 0;
}