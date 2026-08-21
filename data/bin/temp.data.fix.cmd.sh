#!/data/data/com.termux/files/usr/bin/bash

is_termux() {
    # 方法1：检查 Termux 特有的环境变量
    [ -n "$TERMUX_VERSION" ] && return 0

    # 方法2：检查 Termux 专属的目录结构
    [ -d /data/data/com.termux/files/usr ] && return 0

    # 方法3：检查 PREFIX 环境变量（Termux 的标准路径）
    [ "$PREFIX" = "/data/data/com.termux/files/usr" ] && return 0

    # 方法4：检查是否存在 termux-info 命令
    command -v termux-info >/dev/null 2>&1 && return 0

    return 1
}

if is_termux; then
    echo "当前环境是 Termux。"
else
    echo "当前环境不是 Termux。"
    exit 1
fi

echo "修复编译环境中......"

pkg update -y
pkg upgrade -y

pkg install clang -y
pkg install rust -y

mkdir -p "$HOME/storage/shared/MITS/TEMP/"

source ~/storage/tmp.config
if [ "$ensure_storage_permission" = "true" ]; then
    echo "文件权限已启用"
else
    echo "文件权限未启用（已使用备用方案）"
fi

wget -O "$HOME/storage/shared/MITS/TEMP/json.hpp" "https://github.com/nlohmann/json/releases/latest/download/json.hpp" || {
    echo "错误：json.hpp 下载失败" >&2
    exit 1
}

temp.data.config.sh

pkg install termux-elf-cleaner -y

echo "重新安装中......"


bash "$HOME/storage/shared/MITS/data/bin/temp.data.make.C++.sh" || {
    echo "C++ 编译失败" >&2
    exit 1
}
bash "$HOME/storage/shared/MITS/data/bin/temp.data.make.C.sh" || {
    echo "C 编译失败" >&2
    exit 1
}
bash "$HOME/storage/shared/MITS/data/bin/temp.data.make.Rust.sh" || {
    echo "Rust 编译失败" >&2
    exit 1
}