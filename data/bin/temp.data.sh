
#!/data/data/com.termux/files/usr/bin/bash
#
# 用途：按顺序执行一组 pkg 安装/更新命令，出错时调用 temp.data.check.install.sh
#

is_termux() {
    [ -n "$TERMUX_VERSION" ] && return 0
    [ -d /data/data/com.termux/files/usr ] && return 0
    [ "$PREFIX" = "/data/data/com.termux/files/usr" ] && return 0
    command -v termux-info >/dev/null 2>&1 && return 0
    return 1
}

if ! is_termux; then
    echo "当前环境不是 Termux，退出。"
    exit 1
fi

# 加载包含 check_network 的公共库
source "$HOME/storage/shared/MITS/data/General_architecture_shell/git_auto_mirror"

# ============================================
# 本地扩展：URL 代理转换（不改动 git_auto_mirror）
# ============================================
proxy_url() {
    local url="$1"
    # 若已包含代理前缀，直接返回（避免重复）
    [[ "$url" == *"gh-proxy.org"* ]] && { echo "$url"; return; }

    # 调用 git_auto_mirror 中的 check_network（返回 1 表示国内）
    check_network
    local is_cn=$?
    if [ $is_cn -eq 1 ]; then
        # 国内：将 github.com 或 raw.githubusercontent.com 替换为代理前缀
        echo "$url" | sed -E 's#https://(raw\.)?github\.com/#https://gh-proxy.org/https://\1github.com/#'
    else
        # 国外或检测失败：保持原样
        echo "$url"
    fi
}

# ============================================
# 错误处理（优化版）
# ============================================
ERROR_COUNT=0
error_handler() {
    local exit_code=$?
    ERROR_COUNT=$((ERROR_COUNT + 1))
    if [ $ERROR_COUNT -gt 3 ]; then
        echo "错误：连续失败 3 次，停止执行。"
        exit 1
    fi
    echo "警告：命令执行失败（退出码 $exit_code），尝试调用修复脚本..."
    temp.data.check.install.sh
}

# 启用错误陷阱（但避免 set -e 过于激进，使用 trap 捕获 ERR）
set -e
trap 'error_handler' ERR

echo "==================== 开始测试 pkg 相关操作 ===================="

# 1. 检查必要命令
for cmd in curl wget git; do
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "错误：缺少 $cmd，请先安装 (pkg install $cmd)" >&2
        exit 1
    fi
done

# 2. 修复 dpkg 并安装 proot-distro（忽略 postinst 错误）
dpkg --configure -a || true
pkg install proot-distro --no-postinst -y || true

# 3. 镜像检查与系统更新
echo ">>> 1. 检查镜像: pkg --check-mirror update"
pkg --check-mirror update

echo ">>> 2. pkg update"
pkg update -y

echo ">>> 3. pkg upgrade"
pkg upgrade -y

# 4. 批量安装软件包（从配置文件读取）
PACKAGE_LIST_FILE="$HOME/storage/shared/MITS/data/config/install_config/pkg/all.list"
if [ ! -f "$PACKAGE_LIST_FILE" ]; then
    echo "错误：包列表文件不存在: $PACKAGE_LIST_FILE"
    exit 1
fi

packages=()
while IFS= read -r line || [ -n "$line" ]; do
    line="${line#"${line%%[![:space:]]*}"}"
    line="${line%"${line##*[![:space:]]}"}"
    [[ -z "$line" || "$line" =~ ^# ]] && continue
    packages+=("$line")
done < "$PACKAGE_LIST_FILE"

if [ ${#packages[@]} -gt 0 ]; then
    echo ">>> 批量安装 ${#packages[@]} 个软件包"
    pkg install -y "${packages[@]}"
else
    echo "警告：包列表为空，跳过安装。"
fi

# 5. 安装 Zinit（使用代理转换）
echo ">>> 安装 Zinit"
zinit_installer=$(proxy_url "https://raw.githubusercontent.com/zdharma-continuum/zinit/main/scripts/install.sh")
sh -c "$(curl -fsSL "$zinit_installer")"

# 6. 运行 zsh 配置脚本（gitee 国内源，无需代理）
echo ">>> 运行 zsh 配置脚本"
bash -c "$(curl -fsSL https://gitee.com/mo2/zsh/raw/2/2)"

# 7. 克隆 powerlevel10k（使用代理转换）
echo ">>> 克隆 powerlevel10k"
p10k_url=$(proxy_url "https://github.com/romkatv/powerlevel10k.git")
git clone --depth=1 "$p10k_url" ~/powerlevel10k

# 8. 存储与临时目录
echo ">>> 设置存储权限"
if [ -f ~/storage/tmp.config ]; then
    source ~/storage/tmp.config
    if [ "$ensure_storage_permission" = "true" ]; then
        echo "权限已启用"
    else
        echo "权限未启用"
    fi
else
    echo "警告：~/storage/tmp.config 不存在，跳过存储权限设置。"
fi

echo ">>> 创建目录 $HOME/storage/shared/MITS/TEMP/"
mkdir -p "$HOME/storage/shared/MITS/TEMP/"

# 9. 下载 json.hpp（使用代理转换）
echo ">>> 下载 json.hpp"
json_url=$(proxy_url "https://github.com/nlohmann/json/releases/latest/download/json.hpp")
wget -O "$HOME/storage/shared/MITS/TEMP/json.hpp" "$json_url"

# 10. 最终系统更新
echo ">>> 再次 pkg update && upgrade"
pkg update -y
pkg upgrade -y

echo "==================== 所有操作成功完成 ===================="

# 取消错误陷阱
trap - ERR
set +e