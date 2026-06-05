#!/data/data/com.termux/files/usr/bin/bash

is_termux() {
    [ -n "$TERMUX_VERSION" ] && return 0
    [ -d /data/data/com.termux/files/usr ] && return 0
    [ "$PREFIX" = "/data/data/com.termux/files/usr" ] && return 0
    command -v termux-info >/dev/null 2>&1 && return 0
    return 1
}

if ! is_termux; then
    echo "当前环境不是 Termux。"
    exit 1
fi

# 读取需要编译的程序列表（每行一个程序名）
apps=()
while IFS= read -r line || [ -n "$line" ]; do
    apps+=("$line")
done < "/storage/emulated/0/MITS/data/config/C/apps.config"

# 使用 C 编译器（clang 或 gcc）
CC="clang"                    # 或者 "gcc"
CFLAGS="-O2 -Wall -Wextra"    # 优化 + 常见警告
SRC_DIR="/storage/emulated/0/MITS/data/bin/C"
BIN_DIR="/data/data/com.termux/files/usr/bin"

for app in "${apps[@]}"; do
    src="$SRC_DIR/$app.c"
    if [ ! -f "$src" ]; then
        echo "警告：源文件 $src 不存在，跳过 $app"
        continue
    fi

    tmp=$(mktemp) || { echo "创建临时文件失败"; exit 1; }
    echo "正在编译 $app ..."
    if ! $CC $CFLAGS -o "$tmp" "$src"; then
        echo "编译 $app 失败"
        rm -f "$tmp"
        exit 1
    fi

    cp -f "$tmp" "$BIN_DIR/$app"
    chmod +x "$BIN_DIR/$app"
    rm -f "$tmp"

    # 清理 ELF 标志（消除 Termux linker 警告）
    if command -v termux-elf-cleaner >/dev/null 2>&1; then
        if termux-elf-cleaner "$BIN_DIR/$app" >/dev/null 2>&1; then
            echo "$app 已清理 ELF 标志"
        else
            echo "警告：清理 $app 的 ELF 标志失败" >&2
        fi
    else
        echo "提示: 安装 termux-elf-cleaner 可消除 linker 警告 (pkg install termux-elf-cleaner)" >&2
    fi

    echo "$app 安装成功"
done

echo "所有程序安装完成！"