#!/data/data/com.termux/files/usr/bin/bash

# ==========================================
# Windows Termux 一键修复脚本
# 适用于在 Windows 上运行的 Termux 魔改版
# ==========================================

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

echo "███████╗██╗██╗  ██╗    ████████╗███████╗███████╗██████╗ ███╗   ███╗██╗   ██╗██╗  ██╗"
echo "██╔════╝██║╚██╗██╔╝    ╚══██╔══╝██╔════╝██╔════╝██╔══██╗████╗ ████║╚██╗ ██╔╝╚██╗██╔╝"
echo "█████╗  ██║ ╚███╔╝        ██║   █████╗  ███████╗██████╔╝██╔████╔██║ ╚████╔╝  ╚███╔╝ "
echo "██╔══╝  ██║ ██╔██╗        ██║   ██╔══╝  ╚════██║██╔══██╗██║╚██╔╝██║  ╚██╔╝   ██╔██╗ "
echo "██║     ██║██╔╝ ██╗       ██║   ███████╗███████║██║  ██║██║ ╚═╝ ██║   ██║   ██╔╝ ██╗"
echo "╚═╝     ╚═╝╚═╝  ╚═╝       ╚═╝   ╚══════╝╚══════╝╚═╝  ╚═╝╚═╝     ╚═╝   ╚═╝   ╚═╝  ╚═╝"
echo ""
echo "Windows Termux 一键修复脚本"
echo "========================================="

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' #NULL

# 函数：打印带颜色的消息
print_msg() {
    echo -e "${2}${1}${NC}"
}

# 函数：检查是否在 Termux 环境中运行
check_termux() {
    if [ -z "$PREFIX" ] || [ ! -d "$PREFIX" ]; then
        print_msg "错误：当前环境不是 Termux 或环境变量不正确。" "$RED"
        exit 1
    fi
}

# 函数：修复目录权限（解决 compaudit 警告）
fix_permissions() {
    print_msg "步骤 1/6: 修复目录权限..." "$BLUE"
    
    # 检查 compaudit 命令是否存在（Zsh 特有）
    if command -v compaudit &>/dev/null; then
        insecure_dirs=$(compaudit 2>/dev/null)
        if [ -n "$insecure_dirs" ]; then
            print_msg "找到不安全目录:" "$YELLOW"
            echo "$insecure_dirs"
            
            echo "$insecure_dirs" | while read -r dir; do
                if [ -d "$dir" ]; then
                    print_msg "修复权限: $dir" "$BLUE"
                    chmod 755 "$dir" 2>/dev/null || true
                fi
            done
            
            # 修复特定目录
            for dir in ~/.zinit ~/.zinit/plugins ~/.zinit/completions ~/.cache; do
                if [ -d "$dir" ]; then
                    chmod 755 "$dir" 2>/dev/null || true
                fi
            done
            
            # 修复系统目录（需要小心）
            for dir in $PREFIX/share/zsh $PREFIX/share/zsh/*; do
                if [ -d "$dir" ]; then
                    chmod 755 "$dir" 2>/dev/null || true
                fi
            done
            
            print_msg "✓ 目录权限已修复" "$GREEN"
        else
            print_msg "✓ 没有发现不安全目录" "$GREEN"
        fi
    else
        print_msg "⚠ 未找到 compaudit 命令，跳过权限检查（可能未安装 Zsh）" "$YELLOW"
    fi
}

# 函数：更新包管理器
update_packages() {
    print_msg "步骤 2/6: 更新包管理器..." "$BLUE"
    
    # 检查网络连接（使用 curl 替代 ping，更可靠）
    if curl -s --connect-timeout 3 https://packages.termux.org &>/dev/null; then
        print_msg "正在更新包列表..." "$BLUE"
        pkg update -y 2>&1 | tail -5
        
        print_msg "正在升级已安装的包..." "$BLUE"
        pkg upgrade -y 2>&1 | tail -5
        
        print_msg "✓ 包管理器已更新" "$GREEN"
    else
        print_msg "⚠ 无法连接网络，跳过包更新" "$YELLOW"
    fi
}

# 函数：安装必要工具（可选）
install_essentials() {
    print_msg "步骤 3/6: 安装常用工具..." "$BLUE"
    
    # 检查并安装 Zsh（如果未安装）
    if ! command -v zsh &>/dev/null; then
        print_msg "安装 Zsh..." "$BLUE"
        pkg install -y zsh 2>&1 | tail -5
    fi
    
    # 检查并安装 Git（用于插件管理）
    if ! command -v git &>/dev/null; then
        print_msg "安装 Git..." "$BLUE"
        pkg install -y git 2>&1 | tail -5
    fi
    
    print_msg "✓ 工具安装完成" "$GREEN"
}

# 函数：优化 Windows 魔改配置
fix_windows_config() {
    print_msg "步骤 4/6: 优化 Windows 配置..." "$BLUE"
    
    print_msg "该步骤已被禁用" "$RED"
    print_msg "原因：" "$RED"
    print_msg "试图修改配置" "$RED"
    
    # 确保存储权限已设置（如果未设置）
    if [ ! -d "$HOME/storage" ]; then
        print_msg "正在设置存储权限（需要允许）..." "$BLUE"
        termux-setup-storage 2>/dev/null || true
    fi
    
    # 设置 Zsh 为默认 shell（如果当前不是）
    if [ "$SHELL" != "$PREFIX/bin/zsh" ] && command -v zsh &>/dev/null; then
        print_msg "将 Zsh 设置为默认 shell..." "$BLUE"
        chsh -s zsh 2>/dev/null || true
    fi
    
    print_msg "⚠ Windows 配置错误" "$GREEN"
}

# 函数：清理缓存和临时文件
clean_cache() {
    print_msg "步骤 5/6: 清理缓存..." "$BLUE"
    
    # 清理包缓存
    pkg clean 2>/dev/null || true
    
    # 清理 Zsh 缓存
    rm -rf ~/.zcompdump* 2>/dev/null || true
    rm -rf ~/.zinit/completions/* 2>/dev/null || true
    
    print_msg "✓ 缓存已清理" "$GREEN"
}

# 函数：显示完成信息
show_summary() {
    print_msg "步骤 6/6: 完成！" "$BLUE"
    echo ""
    print_msg "修复完成！建议执行以下操作：" "$GREEN"
    echo "  1. 重启 Termux 或执行 'source ~/.zshrc' 使配置生效"
    echo "  2. 如需使用 Zsh，请输入 'zsh' 或重新打开会话"
    echo "  3. 如有问题，请检查备份文件 ~/.zshrc.backup.*"
    echo ""
    print_msg "感谢使用 Windows Termux 修复脚本" "$GREEN"
}

# 主流程
main() {
    check_termux
    fix_permissions
    update_packages
    install_essentials
    fix_windows_config
    clean_cache
    show_summary
}

# 执行主函数
main