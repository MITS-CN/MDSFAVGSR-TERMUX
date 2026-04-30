#!/data/data/com.termux/files/usr/bin/bash
# 检查 Windows 风格命令是否已正确编译并安装到 Termux

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

# 安装脚本中的命令列表（与安装脚本保持一致）
apps=("version" "tasklist" "taskkill" "winver" "diskpart" "systeminfo" "pause" "netsh" "ver" "help" "type" "rename" "move" "ren" "04fe76d6671ee2c9c77d7268291744d374387517fe2c2f10f15e7a7e70797b5e")

# 预期的安装目录（与安装脚本中的 BIN_DIR 一致）
BIN_DIR="/data/data/com.termux/files/usr/bin"

echo "===== 检查 Windows 风格命令安装情况 ====="
echo "预期安装目录: $BIN_DIR"
echo

# 1. 检查每个命令是否可执行
for app in "${apps[@]}"; do
    total=$((total+1))
    if command -v "$app" >/dev/null 2>&1; then
        echo -e "${GREEN}[✓]${NC} $app 已安装并可执行"
        success=$((success+1))
    else
        echo -e "${RED}[✗]${NC} $app 未找到或不可执行"
        fail=$((fail+1))
    fi
done

echo

# 2. 检查 termux-elf-cleaner 是否可用（安装脚本中用来清理 ELF 标志）
total=$((total+1))
if command -v termux-elf-cleaner >/dev/null 2>&1; then
    echo -e "${GREEN}[✓]${NC} termux-elf-cleaner 已安装"
    success=$((success+1))
else
    echo -e "${YELLOW}[!]${NC} termux-elf-cleaner 未安装，建议执行 'pkg install termux-elf-cleaner'"
    fail=$((fail+1))
fi

echo

# 3. 可选：检查源文件目录是否存在（仅作参考）
SRC_DIR="/storage/emulated/0/MITS/data/bin/cpp"
if [ -d "$SRC_DIR" ]; then
    echo -e "${GREEN}[✓]${NC} 源文件目录存在: $SRC_DIR"
else
    echo -e "${YELLOW}[!]${NC} 源文件目录不存在: $SRC_DIR (如果已编译安装可忽略)"
fi

echo
echo "===== 检查完成 ====="
echo -e "总计: $total   ${GREEN}成功: $success${NC}   ${RED}失败: $fail${NC}"

if [ $fail -gt 0 ]; then
    echo "部分命令缺失，请检查编译安装脚本是否成功执行。"
    temp.data.fix.cmd.sh
else
    echo "所有命令均已正确安装。"
fi