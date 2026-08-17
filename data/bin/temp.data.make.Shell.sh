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

PREFIX="${PREFIX:-/data/data/com.termux/files/usr}"

CONFIG="$HOME/storage/shared/MITS/data/config/Shell/install.config"
SRC_DIR="$HOME/storage/shared/MITS/data/bin/shell"
DEST_DIR="$PREFIX/bin"

[ -f "$CONFIG" ] || { echo "错误：找不到配置文件 $CONFIG" >&2; exit 1; }
[ -d "$SRC_DIR" ] || { echo "错误：找不到源目录 $SRC_DIR" >&2; exit 1; }

mkdir -p "$DEST_DIR"

while IFS= read -r name || [ -n "$name" ]; do
    # 清理行首行尾空白和 Windows 回车符
    name="$(printf '%s' "$name" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//' -e 's/\r$//')"

    case "$name" in
        ""|\#*) continue ;;
    esac

    src=""
    if [ -f "$SRC_DIR/$name" ]; then
        src="$SRC_DIR/$name"
    elif [ -f "$SRC_DIR/$name.sh" ]; then
        src="$SRC_DIR/$name.sh"
    else
        echo "警告：跳过 $name，未在 $SRC_DIR 找到 $name 或 $name.sh" >&2
        continue
    fi

    cp -f "$src" "$DEST_DIR/$name"
    chmod 755 "$DEST_DIR/$name"
    echo "已安装：$name -> $DEST_DIR/$name"
done < "$CONFIG"

hash -r 2>/dev/null || true
echo "全部安装完成。"