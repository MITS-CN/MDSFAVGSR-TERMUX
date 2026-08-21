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

# 定义目标目录
target_dir="/data/data/com.termux/files/usr/bin/"

# 检查目标目录是否存在
if [ ! -d "${target_dir}" ]; then
    echo "错误：目标目录 ${target_dir} 不存在"
    exit 1
fi

prefixes=("temp.data" "temp.user" "temp.test" "temp.Releases")

total_deleted=0
echo "正在清理 ${target_dir} 下的临时文件..."

for prefix in "${prefixes[@]}"; do
    # 先检查是否有匹配文件
    matches=("${target_dir}${prefix}"*)
    if [ -e "${matches[0]}" ]; then
        for file in "${matches[@]}"; do
            rm -fv "$file"
            total_deleted=$((total_deleted + 1))
        done
    fi
done

echo ""
echo "清理完成！共删除 ${total_deleted} 个文件"