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

# 批量删除以 temp.data temp.user开头的文件
echo "正在清理 ${target_dir} 下以 temp.data 与 temp.user 开头的文件..."
rm -fv "${target_dir}temp.data"*
rm -fv "${target_dir}temp.user"*
rm -fv "${target_dir}#temp.user"*
rm -fv "${target_dir}#temp.data"*
rm -fv "${target_dir}#temp.test"*
rm -fv "${target_dir}#temp.Releases"*
# 清理完成提示
echo -e "\n清理完成！"