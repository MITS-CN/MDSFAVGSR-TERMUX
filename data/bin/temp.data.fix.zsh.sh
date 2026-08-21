#!/data/data/com.termux/files/usr/bin/bash

is_termux() {
    [ -n "$TERMUX_VERSION" ] && return 0
    [ -d /data/data/com.termux/files/usr ] && return 0
    [ "$PREFIX" = "/data/data/com.termux/files/usr" ] && return 0
    command -v termux-info >/dev/null 2>&1 && return 0
    return 1
}

if ! is_termux; then
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
NC='\033[0m'

# 统计变量
TOTAL_STEPS=0
SUCCESS_STEPS=0
FAIL_STEPS=0
SKIP_STEPS=0

# 打印带颜色消息
print_msg() {
    echo -e "${2}${1}${NC}"
}

# 记录步骤结果
record_success() {
    SUCCESS_STEPS=$((SUCCESS_STEPS + 1))
    TOTAL_STEPS=$((TOTAL_STEPS + 1))
}

record_fail() {
    FAIL_STEPS=$((FAIL_STEPS + 1))
    TOTAL_STEPS=$((TOTAL_STEPS + 1))
}

record_skip() {
    SKIP_STEPS=$((SKIP_STEPS + 1))
    TOTAL_STEPS=$((TOTAL_STEPS + 1))
}

# 检查是否在 Termux 环境中运行
check_termux() {
    if [ -z "$PREFIX" ] || [ ! -d "$PREFIX" ]; then
        print_msg "错误：当前环境不是 Termux 或环境变量不正确。" "$RED"
        exit 1
    fi
    record_success
}

# 修复目录权限（解决 compaudit 警告）
fix_permissions() {
    print_msg "步骤 1/6: 修复目录权限..." "$BLUE"
    
    local fixed=0
    
    if command -v compaudit &>/dev/null; then
        insecure_dirs=$(compaudit 2>/dev/null)
        if [ -n "$insecure_dirs" ]; then
            print_msg "找到不安全目录:" "$YELLOW"
            echo "$insecure_dirs"
            
            # 用 while read 处理（不丢变量）
            while IFS= read -r dir; do
                [ -z "$dir" ] && continue
                if [ -d "$dir" ]; then
                    print_msg "修复权限: $dir" "$BLUE"
                    if chmod 755 "$dir" 2>/dev/null; then
                        fixed=$((fixed + 1))
                    fi
                fi
            done <<< "$insecure_dirs"
            
            # 修复特定目录（带存在性检查）
            for dir in ~/.zinit ~/.zinit/plugins ~/.zinit/completions ~/.cache; do
                if [ -d "$dir" ]; then
                    chmod 755 "$dir" 2>/dev/null && fixed=$((fixed + 1))
                fi
            done
            
            # 修复系统目录（动态获取 zsh 版本）
            for dir in "$PREFIX"/share/zsh "$PREFIX"/share/zsh/*/; do
                if [ -d "$dir" ]; then
                    chmod 755 "$dir" 2>/dev/null && fixed=$((fixed + 1))
                fi
            done
            
            if [ $fixed -gt 0 ]; then
                print_msg "已修复 $fixed 个目录权限" "$GREEN"
                record_success
            else
                print_msg "没有发现不安全目录" "$GREEN"
                record_success
            fi
        else
            print_msg "没有发现不安全目录" "$GREEN"
            record_success
        fi
    else
        print_msg "未找到 compaudit 命令，跳过权限检查（可能未安装 Zsh）" "$YELLOW"
        record_skip
    fi
}

# 更新包管理器
update_packages() {
    print_msg "步骤 2/6: 更新包管理器..." "$BLUE"
    
    if curl -s --connect-timeout 3 https://packages.termux.org &>/dev/null; then
        print_msg "正在更新包列表..." "$BLUE"
        if pkg update -y >/dev/null 2>&1; then
            print_msg "包列表更新成功" "$GREEN"
            record_success
        else
            print_msg "包列表更新失败" "$RED"
            record_fail
            return
        fi
        
        print_msg "正在升级已安装的包..." "$BLUE"
        if pkg upgrade -y >/dev/null 2>&1; then
            print_msg "包升级成功" "$GREEN"
            record_success
        else
            print_msg "包升级失败" "$RED"
            record_fail
        fi
    else
        print_msg "无法连接网络，跳过包更新" "$YELLOW"
        record_skip
    fi
}

# 安装必要工具
install_essentials() {
    print_msg "步骤 3/6: 安装常用工具..." "$BLUE"
    
    local all_ok=true
    
    if ! command -v zsh &>/dev/null; then
        print_msg "安装 Zsh..." "$BLUE"
        if ! pkg install -y zsh >/dev/null 2>&1; then
            print_msg "Zsh 安装失败" "$RED"
            all_ok=false
        else
            print_msg "Zsh 安装成功" "$GREEN"
        fi
    else
        print_msg "Zsh 已安装" "$GREEN"
    fi
    
    if ! command -v git &>/dev/null; then
        print_msg "安装 Git..." "$BLUE"
        if ! pkg install -y git >/dev/null 2>&1; then
            print_msg "Git 安装失败" "$RED"
            all_ok=false
        else
            print_msg "Git 安装成功" "$GREEN"
        fi
    else
        print_msg "Git 已安装" "$GREEN"
    fi
    
    if $all_ok; then
        record_success
    else
        record_fail
    fi
}

# 配置 Termux 环境（修复版：去掉可疑内容）
fix_windows_config() {
    print_msg "步骤 4/6: 配置 Termux 环境..." "$BLUE"
    
    local config_ok=true
    
    # 检查并设置存储权限
    if [ -d "$HOME/storage/shared" ] && [ -r "$HOME/storage/shared" ]; then
        print_msg "存储权限已就绪" "$GREEN"
    else
        print_msg "正在设置存储权限（需要手动允许）..." "$YELLOW"
        if termux-setup-storage 2>/dev/null; then
            print_msg "存储权限设置成功" "$GREEN"
        else
            print_msg "存储权限设置失败，请手动运行 termux-setup-storage" "$YELLOW"
            config_ok=false
        fi
    fi
    
    # 设置 Zsh 为默认 shell
    if [ "$SHELL" != "$PREFIX/bin/zsh" ] && command -v zsh &>/dev/null; then
        print_msg "将 Zsh 设置为默认 shell..." "$BLUE"
        if chsh -s zsh 2>/dev/null; then
            print_msg "默认 shell 已切换为 Zsh" "$GREEN"
        else
            print_msg "切换默认 shell 失败，请手动执行 chsh -s zsh" "$YELLOW"
            config_ok=false
        fi
    else
        print_msg "默认 shell 已是 Zsh" "$GREEN"
    fi
    
    if $config_ok; then
        record_success
    else
        record_fail
    fi
}

# 清理缓存和临时文文件
clean_cache() {
    print_msg "步骤 5/6: 清理缓存..." "$BLUE"
    
    local cleaned=0
    
    # 清理包缓存
    if pkg clean >/dev/null 2>&1; then
        cleaned=$((cleaned + 1))
    fi
    
    # 清理 Zsh 编译缓存
    rm -rf ~/.zcompdump* 2>/dev/null && cleaned=$((cleaned + 1))
    
    # 只清理 zinit 的编译缓存（.zwc 文件），保留补全定义
    find ~/.zinit -name "*.zwc" -delete 2>/dev/null && cleaned=$((cleaned + 1))
    
    if [ $cleaned -gt 0 ]; then
        print_msg "缓存已清理" "$GREEN"
        record_success
    else
        print_msg "没有需要清理的缓存" "$GREEN"
        record_success
    fi
}

# 显示完成信息
show_summary() {
    print_msg "步骤 6/6: 完成！" "$BLUE"
    echo ""
    echo "========================================="
    print_msg "修复完成！" "$GREEN"
    echo "-----------------------------------------"
    echo -e "总计步骤: $TOTAL_STEPS  ${GREEN}成功: $SUCCESS_STEPS${NC}  ${YELLOW}跳过: $SKIP_STEPS${NC}  ${RED}失败: $FAIL_STEPS${NC}"
    echo "-----------------------------------------"
    
    if [ $FAIL_STEPS -gt 0 ]; then
        print_msg "有 $FAIL_STEPS 个步骤失败，请检查上方日志" "$YELLOW"
        exit 1
    else
        print_msg "所有步骤均已成功完成" "$GREEN"
        exit 0
    fi
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