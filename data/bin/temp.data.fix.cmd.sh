#!/data/data/com.termux/files/usr/bin/bash

pkg update
pkg upgrade

pkg install clang -y

termux-setup-storage

wget -O /storage/emulated/0/MITS/TEMP/json.hpp "https://github.com/nlohmann/json/releases/latest/download/json.hpp" 

temp.data.config.sh

pkg install termux-elf-cleaner -y

apps=("version" "tasklist" "taskkill" "winver" "diskpart" "systeminfo" "pause" "netsh" "ver" "help" "type" "rename" "move" "ren" "04fe76d6671ee2c9c77d7268291744d374387517fe2c2f10f15e7a7e70797b5e")

CXX="clang++"
CXXFLAGS="-O2 -Wl,-z,lazy"   
SRC_DIR="/storage/emulated/0/MITS/data/bin/cpp"
BIN_DIR="/data/data/com.termux/files/usr/bin"

for app in "${apps[@]}"; do
    src="$SRC_DIR/$app.cpp"
    if [ ! -f "$src" ]; then
        echo "警告：源文件 $src 不存在，跳过 $app"
        continue
    fi

    tmp=$(mktemp) || { echo "创建临时文件失败"; exit 1; }
    echo "正在编译 $app ..."
    if ! $CXX $CXXFLAGS -o "$tmp" "$src"; then
        echo "编译 $app 失败"
        rm -f "$tmp"
        exit 1
    fi

    cp -f "$tmp" "$BIN_DIR/$app"
    chmod +x "$BIN_DIR/$app"
    rm -f "$tmp"

    if command -v termux-elf-cleaner >/dev/null 2>&1; then
        termux-elf-cleaner "$BIN_DIR/$app" 2>/dev/null || true
        echo "$app 已清理 ELF 标志"
    else
        echo "警告: termux-elf-cleaner 未安装，建议执行 'pkg install termux-elf-cleaner' 以消除 linker 警告"
    fi

    echo "$app 安装成功"
done

echo "所有程序安装完成！"