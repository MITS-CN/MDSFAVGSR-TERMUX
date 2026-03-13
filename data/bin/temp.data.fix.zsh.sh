#!/data/data/com.termux/files/usr/bin/bash

echo "███████╗██╗██╗  ██╗    ████████╗███████╗██████╗ ███╗   ███╗██╗   ██╗██╗  ██╗"
echo "██╔════╝██║╚██╗██╔╝    ╚══██╔══╝██╔════╝██╔══██╗████╗ ████║╚██╗ ██╔╝╚██╗██╔╝"
echo "█████╗  ██║ ╚███╔╝        ██║   █████╗  ██████╔╝██╔████╔██║ ╚████╔╝  ╚███╔╝ "
echo "██╔══╝  ██║ ██╔██╗        ██║   ██╔══╝  ██╔══██╗██║╚██╔╝██║  ╚██╔╝   ██╔██╗ "
echo "██║     ██║██╔╝ ██╗       ██║   ███████╗██║  ██║██║ ╚═╝ ██║   ██║   ██╔╝ ██╗"
echo "╚═╝     ╚═╝╚═╝  ╚═╝       ╚═╝   ╚══════╝╚═╝  ╚═╝╚═╝     ╚═╝   ╚═╝   ╚═╝  ╚═╝"
echo ""
echo "Windows Termux 一键修复脚本"
echo "========================================="

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 函数：打印带颜色的消息
print_msg() {
    echo -e "${2}${1}${NC}"
}

fix_permissions() {
    print_msg "步骤 1/6: 修复目录权限..." "$BLUE"
    
    insecure_dirs=$(compaudit 2>/dev/null)
    if [ -n "$insecure_dirs" ]; then
        print_msg "找到不安全目录:" "$YELLOW"
        echo "$insecure_dirs"
        
        echo "$insecure_dirs" | while read -r dir; do
            if [ -d "$dir" ]; then
                print_msg "修复权限: $dir" "$BLUE"
                chmod 755 "$dir" 2>/dev/null
            fi
        done
        
        # 修复特定目录
        for dir in ~/.zinit ~/.zinit/plugins ~/.zinit/completions ~/.cache; do
            if [ -d "$dir" ]; then
                chmod 755 "$dir" 2>/dev/null
            fi
        done
        
        # 修复系统目录（需要小心）
        for dir in $PREFIX/share/zsh $PREFIX/share/zsh/*; do
            if [ -d "$dir" ]; then
                chmod 755 "$dir" 2>/dev/null
            fi
        done
        
        print_msg "✓ 目录权限已修复" "$GREEN"
    else
        print_msg "✓ 没有发现不安全目录" "$GREEN"
    fi
}

# 函数：更新包管理器
update_packages() {
    print_msg "步骤 3/6: 更新包管理器..." "$BLUE"
    
    # 检查网络连接
    if ping -c 1 packages.termux.org &>/dev/null; then
        print_msg "正在更新包列表..." "$BLUE"
        pkg update -y 2>/dev/null | tail -5
        
        print_msg "正在升级已安装的包..." "$BLUE"
        pkg upgrade -y 2>/dev/null | tail -5
        
        print_msg "✓ 包管理器已更新" "$GREEN"
    else
        print_msg "⚠ 无法连接网络，跳过包更新" "$YELLOW"
    fi
}

# 函数：优化 Windows 魔改配置
fix_windows_config() {
    print_msg "步骤 5/6: 优化 Windows 配置..." "$BLUE"
    
    # 备份原配置
    if [ -f ~/.zshrc ]; then
        cp ~/.zshrc ~/.zshrc.backup.$(date +%Y%m%d_%H%M%S)
        print_msg "已备份原配置到: ~/.zshrc.backup" "$GREEN"
    fi
    

fix_permissions()
update_packages()
fix_windows_config()