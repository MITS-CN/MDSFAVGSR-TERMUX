#!/data/data/com.termux/files/usr/bin/bash
# 检查 Termux 安装环境是否完整

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

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 统计
total=0
success=0
fail=0

# 检查命令是否存在
check_cmd() {
    local cmd=$1
    total=$((total+1))
    if command -v "$cmd" >/dev/null 2>&1; then
        echo -e "${GREEN}[✓]${NC} $cmd"
        success=$((success+1))
    else
        echo -e "${RED}[✗]${NC} $cmd (未找到)"
        fail=$((fail+1))
    fi
}

# 检查目录是否存在
check_dir() {
    local dir=$1
    total=$((total+1))
    if [ -d "$dir" ]; then
        echo -e "${GREEN}[✓]${NC} 目录 $dir"
        success=$((success+1))
    else
        echo -e "${RED}[✗]${NC} 目录 $dir (不存在)"
        fail=$((fail+1))
    fi
}

# 检查文件是否存在
check_file() {
    local file=$1
    total=$((total+1))
    if [ -f "$file" ]; then
        echo -e "${GREEN}[✓]${NC} 文件 $file"
        success=$((success+1))
    else
        echo -e "${RED}[✗]${NC} 文件 $file (不存在)"
        fail=$((fail+1))
    fi
}

echo "===== 开始检查 Termux 安装环境 ====="
echo

# 1. 检查基础软件包（从配置文件读取命令列表）
echo ">>> 基础工具"

# 配置文件路径
CMD_CONFIG_FILE="/storage/emulated/0/MITS/data/config/install/pkg/apps.config"

if [ ! -f "$CMD_CONFIG_FILE" ]; then
    echo -e "${YELLOW}[!]${NC} 命令配置文件不存在: $CMD_CONFIG_FILE"
    echo -e "${YELLOW}    跳过命令检查，请确保配置文件存在并包含每行一个的命令名。${NC}"
else
    # 读取命令列表到数组
    cmds=()
    while IFS= read -r line || [ -n "$line" ]; do
        # 跳过空行和注释行（以 # 开头）
        [[ -z "$line" || "$line" =~ ^[[:space:]]*# ]] && continue
        cmds+=("$line")
    done < "$CMD_CONFIG_FILE"

    # 循环检查每个命令
    for cmd in "${cmds[@]}"; do
        check_cmd "$cmd"
    done
fi

echo

# 2. 检查额外配置和资源
echo ">>> 额外配置和资源"
# zinit 安装
check_dir "$HOME/.zinit"
# 检查是否运行了 zsh 配置脚本（简单判断 .zshrc 是否包含 zinit）
if [ -f "$HOME/.zshrc" ] && grep -q "zinit" "$HOME/.zshrc"; then
    echo -e "${GREEN}[✓]${NC} zsh 配置 (zinit 已集成到 .zshrc)"
    success=$((success+1))
else
    echo -e "${RED}[✗]${NC} zsh 配置 (未找到 zinit 相关配置)"
    fail=$((fail+1))
fi
total=$((total+1))
# powerlevel10k
check_dir "$HOME/powerlevel10k"
# 存储目录
check_dir "$HOME/storage"  # termux-setup-storage 创建
# 用户自定义目录
CUSTOM_DIR="/storage/emulated/0/MITS/TEMP"
if [ -d "$CUSTOM_DIR" ]; then
    echo -e "${GREEN}[✓]${NC} 目录 $CUSTOM_DIR"
    success=$((success+1))
else
    echo -e "${YELLOW}[?]${NC} 目录 $CUSTOM_DIR (不存在，可能未挂载或权限不足)"
    fail=$((fail+1))
fi
total=$((total+1))
# json.hpp 文件
JSON_FILE="/storage/emulated/0/MITS/TEMP/json.hpp"
if [ -f "$JSON_FILE" ]; then
    echo -e "${GREEN}[✓]${NC} 文件 $JSON_FILE"
    success=$((success+1))
else
    echo -e "${YELLOW}[?]${NC} 文件 $JSON_FILE (不存在，可能未下载或路径问题)"
    fail=$((fail+1))
fi
total=$((total+1))

echo
echo "===== 检查完成 ====="
echo -e "总计: $total   ${GREEN}成功: $success${NC}   ${RED}失败: $fail${NC}"

# 返回非零如果有失败
if [ $fail -gt 0 ]; then
    echo "出现一点小错误"
    echo "正在调用修复脚本......"
    temp.data.fix.install.on.sh
else
    echo "一切正常"
fi