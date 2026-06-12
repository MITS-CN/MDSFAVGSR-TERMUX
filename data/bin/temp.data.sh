#!/data/data/com.termux/files/usr/bin/bash
#
# test_pkg_install.sh
# 用途：按顺序执行一组 pkg 安装/更新命令，出错时调用 temp.data.check.install.sh
#

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

# 错误处理函数
error_handler() {
    echo "安装脚本执行出错，调用修复脚本中......"
    temp.data.check.install.sh
}

# 修复部分设备无法正常安装导致误触发修复

chmod 777 /data/data/com.termux/files/usr/var/lib/dpkg/info/proot-distro.postinst
cp -r /data/data/com.termux/files/usr/var/lib/dpkg/info/proot-distro.postinst /data/data/com.termux/files/usr/var/lib/dpkg/info/proot-distro.postinst.bak

cat > /data/data/com.termux/files/usr/var/lib/dpkg/info/proot-distro.postinst << 'EOF'
#!/bin/sh
exit 0
EOF

chmod 755 /data/data/com.termux/files/usr/var/lib/dpkg/info/proot-distro.postinst
dpkg --configure proot-distro

# 启用遇到错误绑定错误陷阱
trap 'error_handler' ERR

echo "==================== 开始测试 pkg 相关操作 ===================="

# --- 检查镜像并更新 ---
echo ">>> 1. 检查镜像: pkg --check-mirror update"
pkg --check-mirror update

echo ">>> 2. pkg update"
pkg update

echo ">>> 3. pkg upgrade"
pkg upgrade -y

# ------------------------------------------------------------------
# 从配置文件读取所有需要安装的软件包（一行一个包名）
# ------------------------------------------------------------------
PACKAGE_LIST_FILE="$HOME/storage/shared/MITS/data/config/install/pkg/all.list"
if [ ! -f "$PACKAGE_LIST_FILE" ]; then
    echo "错误: 包列表文件不存在: $PACKAGE_LIST_FILE"
    exit 1
fi

# 读取非空、非注释行，存入数组
packages=()
while IFS= read -r line || [ -n "$line" ]; do
    # 去除首尾空白
    line="${line#"${line%%[![:space:]]*}"}"
    line="${line%"${line##*[![:space:]]}"}"
    # 跳过空行和注释
    [[ -z "$line" || "$line" =~ ^# ]] && continue
    packages+=("$line")
done < "$PACKAGE_LIST_FILE"

if [ ${#packages[@]} -eq 0 ]; then
    echo "警告: 包列表为空，跳过安装"
else
    echo ">>> 批量安装所有必需的软件包 (共 ${#packages[@]} 个)"
    pkg install -y "${packages[@]}"
fi

# 注意：list-installed 理论上是一个参数，而非包名，已从列表中移除（若需要可手动执行）

# --- 外部脚本与配置 ---
echo ">>> 安装 zinit"
sh -c "$(curl -fsSL https://raw.githubusercontent.com/zdharma-continuum/zinit/main/scripts/install.sh)"

echo ">>> 运行 zsh 配置脚本"
bash -c "$(curl -L gitee.com/mo2/zsh/raw/2/2)"

echo ">>> 克隆 powerlevel10k"
git clone --depth=1 https://bgithub.xyz/romkatv/powerlevel10k.git ~/powerlevel10k

# --- 存储与文件准备 ---
echo ">>> 设置存储权限"
termux-setup-storage

echo ">>> 创建目录 $HOME/storage/shared/MITS/TEMP/"
mkdir -p "$HOME/storage/shared/MITS/TEMP/"

echo ">>> 下载 json.hpp"
wget -O "$HOME/storage/shared/MITS/TEMP/json.hpp" \
    "https://github.com/nlohmann/json/releases/latest/download/json.hpp"

# --- 最终更新 ---
echo ">>> 再次 pkg update"
pkg update

echo ">>> 再次 pkg upgrade"
pkg upgrade -y

echo "==================== 所有操作成功完成 ===================="

# 取消错误陷阱
trap - ERR
set +e

# 恢复 proot-distro 原始 postinst 脚本

cp -r /data/data/com.termux/files/usr/var/lib/dpkg/info/proot-distro.postinst.bak /data/data/com.termux/files/usr/var/lib/dpkg/info/proot-distro.postinst
chmod 755 /data/data/com.termux/files/usr/var/lib/dpkg/info/proot-distro.postinst

dpkg --configure proot-distro