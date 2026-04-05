#!/data/data/com.termux/files/usr/bin/bash
termux-setup-storage

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 统计
total_actions=0
success_actions=0
fail_actions=0

# 执行命令并统计结果
run_action() {
    local description="$1"
    local command="$2"
    total_actions=$((total_actions + 1))
    echo -e "${BLUE}>>>${NC} $description"
    if eval "$command" >/dev/null 2>&1; then
        echo -e "${GREEN}[✓]${NC} $description"
        success_actions=$((success_actions + 1))
        return 0
    else
        echo -e "${RED}[✗]${NC} $description"
        fail_actions=$((fail_actions + 1))
        return 1
    fi
}

# 直接运行命令，不统计（用于内部子步骤）
run_silent() {
    eval "$1" >/dev/null 2>&1 || true
}

echo -e "${GREEN}===== Termux 环境修复开始 =====${NC}\n"

# 1. 更新包管理器
run_action "更新软件包列表" "pkg update -y"
run_action "升级已安装的软件包" "pkg upgrade -y"

# 2. 定义需要安装的软件包列表（命令名 -> 包名映射）
declare -A PACKAGES=(
    [clang]=clang
    [gcc]=gcc-11              # Termux 中 gcc 命令由 gcc-11 包提供
    [openssl]=openssl-tool
    [python]=python
    [zsh]=zsh
    [git]=git
    [pv]=pv
    [pulseaudio]=pulseaudio
    [proot]=proot
    [zstd]=zstd
    [bat]=bat
    [termux-dialog]=termux-api
    [aria2c]=aria2
    [eza]=eza
    [fzf]=fzf
    [wget]=wget
    [tree]=tree
    [rsync]=rsync
    [neofetch]=neofetch
    [traceroute]=traceroute
    [dig]=dnsutils
    [nano]=nano
    [rush]=rush
    [jq]=jq
    [sqlite3]=sqlite
    [ifconfig]=net-tools
    [ack]=ack
    [termux-elf-cleaner]=termux-elf-cleaner
    [vim]=vim
    [proot-distro]=proot-distro
)

# 安装缺失的软件包（直接批量安装，pkg install 会自动跳过已安装的）
run_action "安装所有必需的软件包" "pkg install -y ${PACKAGES[@]}"

# 处理 gcc 符号链接（如果 gcc-11 已安装但命令名是 gcc-11）
if command -v gcc-11 >/dev/null 2>&1 && ! command -v gcc >/dev/null 2>&1; then
    run_action "创建 gcc 符号链接指向 gcc-11" "ln -sf $(which gcc-11) $PREFIX/bin/gcc"
fi

# 3. 安装 Zinit（zsh 插件管理器）
if [ ! -d "$HOME/.zinit" ]; then
    run_action "克隆 Zinit 到 ~/.zinit" "git clone https://github.com/zdharma-continuum/zinit.git $HOME/.zinit"
else
    echo -e "${GREEN}[✓]${NC} Zinit 已存在，跳过克隆"
    success_actions=$((success_actions + 1))
    total_actions=$((total_actions + 1))
fi

# 4. 安装 Powerlevel10k 主题
if [ ! -d "$HOME/powerlevel10k" ]; then
    run_action "克隆 Powerlevel10k 到 ~/powerlevel10k" "git clone --depth=1 https://github.com/romkatv/powerlevel10k.git $HOME/powerlevel10k"
else
    echo -e "${GREEN}[✓]${NC} Powerlevel10k 已存在，跳过克隆"
    success_actions=$((success_actions + 1))
    total_actions=$((total_actions + 1))
fi

# 5. 配置 ~/.zshrc
ZSHRC="$HOME/.zshrc"
if [ ! -f "$ZSHRC" ]; then
    echo "# Termux 默认 zsh 配置" > "$ZSHRC"
fi

# 检查是否已包含 zinit 配置
if ! grep -q "zinit" "$ZSHRC"; then
    # 备份原文件
    cp "$ZSHRC" "${ZSHRC}.bak"
    cp -r /storage/emulated/0/MITS/data/bin/.zshrc /data/data/com.termux/files/home/.zshrc
    run_action "添加 Zinit 和 Powerlevel10k 配置到 ~/.zshrc" "true"
else
    echo -e "${GREEN}[✓]${NC} ~/.zshrc 已包含 zinit 配置，跳过修改"
    success_actions=$((success_actions + 1))
    total_actions=$((total_actions + 1))
fi

# 6. 设置存储权限
if [ ! -d "$HOME/storage" ]; then
    run_action "运行 termux-setup-storage 创建存储目录" "termux-setup-storage"
else
    echo -e "${GREEN}[✓]${NC} ~/storage 已存在，跳过设置"
    success_actions=$((success_actions + 1))
    total_actions=$((total_actions + 1))
fi

# 7. 创建外部自定义目录
CUSTOM_DIR="/storage/emulated/0/MITS/TEMP"
if [ ! -d "$CUSTOM_DIR" ]; then
    run_action "创建目录 $CUSTOM_DIR" "mkdir -p $CUSTOM_DIR"
else
    echo -e "${GREEN}[✓]${NC} 目录 $CUSTOM_DIR 已存在，跳过创建"
    success_actions=$((success_actions + 1))
    total_actions=$((total_actions + 1))
fi

# 8. 下载 json.hpp（若缺失）
JSON_FILE="$CUSTOM_DIR/json.hpp"
if [ ! -f "$JSON_FILE" ]; then
    # 默认 URL（nlohmann/json 的 develop 分支单头文件）
    DEFAULT_URL="https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp"
    echo -e "${YELLOW}[?]${NC} 文件 $JSON_FILE 不存在"
    read -p "是否尝试从 $DEFAULT_URL 下载？[y/N] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        run_action "下载 json.hpp" "wget -O '$JSON_FILE' '$DEFAULT_URL'"
    else
        echo -e "${YELLOW}[!]${NC} 请手动将 json.hpp 放置到 $JSON_FILE"
        fail_actions=$((fail_actions + 1))
        total_actions=$((total_actions + 1))
    fi
else
    echo -e "${GREEN}[✓]${NC} 文件 $JSON_FILE 已存在，跳过下载"
    success_actions=$((success_actions + 1))
    total_actions=$((total_actions + 1))
fi

# 9. 可选：将 zsh 设为默认 shell（需要用户确认）
if [[ $SHELL != *"zsh" ]]; then
    echo -e "${YELLOW}[?]${NC} 当前默认 shell 不是 zsh"
    read -p "是否将默认 shell 切换为 zsh？[y/N] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        run_action "切换默认 shell 为 zsh" "chsh -s zsh"
    fi
fi

# 10. 额外修复：清理 termux-elf-cleaner 可能引起的库问题（可选）
run_action "运行 termux-elf-cleaner 修复 ELF 文件" "termux-elf-cleaner $PREFIX/lib 2>/dev/null || true"

echo
echo -e "${GREEN}===== 修复完成 =====${NC}"
echo -e "总计操作: $total_actions   ${GREEN}成功: $success_actions${NC}   ${RED}失败: $fail_actions${NC}"

if [ $fail_actions -gt 0 ]; then
    echo -e "${YELLOW}部分操作失败，请根据提示手动处理。${NC}"
    exit 1
else
    echo -e "${GREEN}所有修复操作成功完成！建议重启 Termux 或执行 'exec zsh' 进入新环境。${NC}"
    exit 0
fi