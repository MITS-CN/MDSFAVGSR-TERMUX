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

pkg update
pkg upgrade

pkg install clang -y

termux-setup-storage

wget -O /storage/emulated/0/MITS/TEMP/json.hpp "https://github.com/nlohmann/json/releases/latest/download/json.hpp" 

temp.data.config.sh

pkg install termux-elf-cleaner -y

apps=("version" "tasklist" "taskkill" "winver" "diskpart" "timeout" "systeminfo" "pause" "netsh" "ver" "help" "type" "rename" "comp" "replace" "move" "ren" "04fe76d6671ee2c9c77d7268291744d374387517fe2c2f10f15e7a7e70797b5e")

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
        if termux-elf-cleaner "$BIN_DIR/$app" >/dev/null 2>&1; then
            echo "$app 已清理 ELF 标志"
        else
            echo "警告：清理 $app 的 ELF 标志失败" >&2
        fi
        
    else
        echo "警告: termux-elf-cleaner 未安装，建议执行 'pkg install termux-elf-cleaner' 以消除 linker 警告"
    fi

    echo "$app 安装成功"
done

echo "所有程序安装完成！"


#!/data/data/com.termux/files/usr/bin/bash

# ============================================================
# Rust 程序批量编译安装脚本 (Termux)
# 将 /storage/emulated/0/.../Rust 下的多个项目编译后部署到 $PREFIX/bin
# ============================================================

set -euo pipefail

# ------------------------- 配置 -------------------------
# 共享存储上的 Rust 项目根目录
SOURCE_BASE="/storage/emulated/0/MITS/data/bin/Rust"

# 本地构建缓存目录（必须在 ext4 文件系统上，如 $HOME）
BUILD_BASE="$HOME/.cache/rust-builds"

# Termux 可执行文件安装目录
BIN_DIR="/data/data/com.termux/files/usr/bin"

# Rust 编译优化级别
RUSTFLAGS="-C opt-level=2"

# 默认需要编译的项目列表（可被命令行参数覆盖）
DEFAULT_APPS=("del")

# ------------------------- 函数 -------------------------
is_termux() {
    [ -n "${TERMUX_VERSION-}" ] && return 0
    [ -d /data/data/com.termux/files/usr ] && return 0
    [ "${PREFIX-}" = "/data/data/com.termux/files/usr" ] && return 0
    command -v termux-info >/dev/null 2>&1 && return 0
    return 1
}

# 打印错误并退出
die() {
    echo "错误: $*" >&2
    exit 1
}

# 编译单个项目
build_app() {
    local app="$1"
    local source_dir="$SOURCE_BASE/$app"
    local build_dir="$BUILD_BASE/$app"

    # 检查源码目录是否完整
    if [ ! -d "$source_dir" ]; then
        echo "警告: 源码目录 $source_dir 不存在，跳过 $app"
        return 1
    fi
    if [ ! -f "$source_dir/Cargo.toml" ]; then
        echo "警告: $source_dir/Cargo.toml 不存在，跳过 $app"
        return 1
    fi
    if [ ! -f "$source_dir/src/main.rs" ]; then
        echo "警告: $source_dir/src/main.rs 不存在，跳过 $app"
        return 1
    fi

    echo ">>> 开始处理 $app"

    # 创建构建目录并同步源码（排除 target 目录，避免复制大量中间文件）
    mkdir -p "$build_dir"
    echo "    同步源码到 $build_dir ..."
    rsync -a --delete --exclude='target' "$source_dir/" "$build_dir/"

    # 编译
    echo "    正在编译 $app (release)..."
    cd "$build_dir"
    if ! RUSTFLAGS="$RUSTFLAGS" cargo build --release; then
        echo "    编译 $app 失败！"
        return 1
    fi

    # 检查产物
    local release_bin="$build_dir/target/release/$app"
    if [ ! -f "$release_bin" ]; then
        echo "    错误：找不到编译产物 $release_bin"
        return 1
    fi

    # 安装到 BIN_DIR
    echo "    安装 $app 到 $BIN_DIR ..."
    cp -f "$release_bin" "$BIN_DIR/$app"
    chmod +x "$BIN_DIR/$app"

    # 清理 ELF 标志
    if command -v termux-elf-cleaner >/dev/null 2>&1; then
        if termux-elf-cleaner "$BIN_DIR/$app" >/dev/null 2>&1; then
            echo "    ELF 标志已清理"
        else
            echo "    警告：清理 ELF 标志失败" >&2
        fi
    else
        echo "    提示: 安装 termux-elf-cleaner 可消除 linker 警告 (pkg install termux-elf-cleaner)" >&2
    fi

    echo "    $app 安装成功！"
    return 0
}

# ------------------------- 主逻辑 -------------------------
# 检查运行环境
if ! is_termux; then
    die "当前环境不是 Termux，脚本退出。"
fi

# 确定要编译的程序列表
if [ $# -eq 0 ]; then
    apps=("${DEFAULT_APPS[@]}")
else
    apps=("$@")
fi

# 检查 rsync 是否可用（用于高效同步）
if ! command -v rsync >/dev/null 2>&1; then
    echo "未检测到 rsync，使用 cp 进行简单复制（可能较慢）"
    use_rsync=false
else
    use_rsync=true
fi

# 创建构建根目录
mkdir -p "$BUILD_BASE"

# 遍历编译
failed_apps=()
for app in "${apps[@]}"; do
    if $use_rsync; then
        # 使用 rsync 版本
        build_app "$app" || failed_apps+=("$app")
    else
        # 回退到 cp 方案（简单覆盖）
        source_dir="$SOURCE_BASE/$app"
        build_dir="$BUILD_BASE/$app"
        if [ ! -d "$source_dir" ] || [ ! -f "$source_dir/Cargo.toml" ] || [ ! -f "$source_dir/src/main.rs" ]; then
            echo "警告: $app 源码不完整，跳过"
            failed_apps+=("$app")
            continue
        fi
        echo ">>> 开始处理 $app"
        mkdir -p "$build_dir"
        echo "    复制源码到 $build_dir ..."
        cp -r "$source_dir"/. "$build_dir"/
        # 移除可能残留的 target 目录（防止干扰）
        rm -rf "$build_dir/target"
        cd "$build_dir"
        echo "    正在编译 $app (release)..."
        if ! RUSTFLAGS="$RUSTFLAGS" cargo build --release; then
            echo "    编译 $app 失败！"
            failed_apps+=("$app")
            continue
        fi
        release_bin="$build_dir/target/release/$app"
        if [ ! -f "$release_bin" ]; then
            echo "    错误：找不到编译产物 $release_bin"
            failed_apps+=("$app")
            continue
        fi
        echo "    安装 $app 到 $BIN_DIR ..."
        cp -f "$release_bin" "$BIN_DIR/$app"
        chmod +x "$BIN_DIR/$app"
        if command -v termux-elf-cleaner >/dev/null 2>&1; then
            termux-elf-cleaner "$BIN_DIR/$app" >/dev/null 2>&1 || echo "    警告：清理 ELF 标志失败" >&2
        fi
        echo "    $app 安装成功！"
    fi
done

# 汇总结果
echo "=================================="
if [ ${#failed_apps[@]} -eq 0 ]; then
    echo "所有程序编译安装完成！"
else
    echo "以下程序编译或安装失败："
    for f in "${failed_apps[@]}"; do
        echo "  - $f"
    done
    exit 1
fi