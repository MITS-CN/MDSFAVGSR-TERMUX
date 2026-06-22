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
A=0   # 0:正常, 1:用户取消, 2:备份失败, 3:复制失败
# 检测网络连接（基于 ICMP）
# 参数: 可指定测试目标（默认 8.8.8.8）
# 返回: 0 表示网络可达，1 表示不可达
check_network_ping() {
    target="${1:-8.8.8.8}"          # 默认 Google DNS
    timeout="${2:-3}"               # 超时秒数

    # -c 1 只发一个包，-W 设置超时
    if ping -c 1 -W "$timeout" "$target" >/dev/null 2>&1; then
        return 0
    else
        return 1
    fi
}

if is_termux; then
    echo "当前环境是 Termux。"
else
    echo "当前环境不是 Termux。"
    exit 1
fi

if check_network_ping; then
    echo "网络已连接"
else
    echo "网络未连接" >&2
    exit 1
fi

set -euo pipefail

# ---------- 颜色与提示函数 ----------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

info()  { echo -e "${GREEN}[✓]${NC} $*"; }
warn()  { echo -e "${YELLOW}[!]${NC} $*"; }
error() { echo -e "${RED}[✗]${NC} $*" >&2; }
title() { echo -e "${CYAN}==>${NC} $*"; }

# ---------- 配置（可按需修改） ----------
# 源备份目录（与你的命令保持一致）
SRC_DIR="$HOME/storage/shared/MITS/data/something/!data!data!com.termux!files!home!.termux"

# 目标 Termux 配置目录
DEST_DIR="/data/data/com.termux/files/home/.termux"

# ---------- 安全与友好检查 ----------
title "Termux .termux 配置恢复脚本"

# 1. 检查源目录是否存在
if [ ! -d "$SRC_DIR" ]; then
    error "源目录不存在："
    error "  $SRC_DIR"
    error "请检查路径是否正确，或确认备份已存在。"
    exit 1
fi

# 2. 检查源目录是否为空（避免无意义操作）
if [ -z "$(ls -A "$SRC_DIR" 2>/dev/null)" ]; then
    warn "源目录为空，无需复制任何文件。"
    exit 0
fi

# 3. 检查目标目录是否存在，不存在则尝试创建
if [ ! -d "$DEST_DIR" ]; then
    warn "目标目录不存在，将自动创建：$DEST_DIR"
    if ! mkdir -p "$DEST_DIR"; then
        error "无法创建目标目录，请检查权限。"
        exit 1
    fi
    info "目标目录已创建。"
fi

# 4. 显示操作摘要，请求用户确认
echo
info "即将执行以下操作："
echo "   源目录 : $SRC_DIR"
echo "   目标目录 : $DEST_DIR"
echo "   * 如果目标目录内有同名文件，将被覆盖。"
echo
read -p "是否继续？(y/n) " -n 1 -r
echo

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    info "操作已取消。"
    A=1
fi

# 5. （可选）备份当前目标目录
# 只有 A 不为 1 时才执行备份
if [ "$A" != 1 ]; then
    if [ -d "$DEST_DIR" ] && [ -n "$(ls -A "$DEST_DIR" 2>/dev/null)" ]; then
        BACKUP="${DEST_DIR}_backup_$(date +%Y%m%d_%H%M%S)"
        echo
        warn "目标目录已有文件，正在创建备份："
        warn "  $BACKUP"
        if cp -r "$DEST_DIR" "$BACKUP"; then
            info "备份完成。"
        else
            error "备份失败，操作终止。"
            A=2
        fi
    fi
fi

# 6. 执行复制（使用 cp -r，并注意通配符在脚本中的正确展开）
# 根据状态执行复制或跳过
if [ "$A" -eq 0 ]; then
    info "正在复制文件..."
    # 注意：通配符不要用引号括起来，否则不会展开
    if cp -r "$SRC_DIR"/* "$DEST_DIR"/; then
        info "恢复完成！.termux 配置已更新。"
    else
        error "复制过程中出现错误，请检查备份或权限。"
        A=3   # 3 表示复制失败，方便后续脚本判断
    fi
elif [ "$A" -eq 1 ]; then
    warn "用户已取消操作，未执行复制。"
elif [ "$A" -eq 2 ]; then
    error "因备份失败，已跳过复制步骤。"
else
    # 如果 A 有其他意外值，也报错并跳过
    error "未知状态 A=$A，跳过复制步骤。"
fi

# 7. 提示用户可能需要重启 termux
echo
info "提示：部分更改可能需要重启 Termux 才能生效。"