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

#!/data/data/com.termux/files/usr/bin/bash

# ============================================
# Termux 辅助脚本：确保已安装 Termux:Styling 和 Termux:API
# 功能：检测 → 自动下载最新 APK → 安装 → 重检与修复 → 人工提醒
# ============================================

set -e

PACKAGES=("com.termux.styling" "com.termux.api")
REPOS=("termux/termux-styling" "termux/termux-api")
DISPLAY_NAMES=("Termux:Styling" "Termux:API")
TMPDIR="$HOME/.termux_apk_installer"
mkdir -p "$TMPDIR"

# --------------------------------------------------
# 函数：检测某个包是否已安装（通过 /data/app 目录）
# --------------------------------------------------
is_installed() {
    local pkg="$1"
    # 无后缀
    [ -d "/data/app/${pkg}" ] && return 0
    # 带数字后缀 1~9
    for i in {1..9}; do
        [ -d "/data/app/${pkg}-${i}" ] && return 0
    done
    return 1
}

# --------------------------------------------------
# 函数：从 GitHub 获取最新 APK 下载链接
# 参数：仓库名（如 termux/termux-api）
# 返回值：输出下载 URL，失败返回 1
# --------------------------------------------------
get_latest_apk_url() {
    local repo="$1"
    local api_url="https://api.github.com/repos/${repo}/releases/latest"
    local json

    # 尝试使用 jq 解析
    if command -v jq >/dev/null 2>&1; then
        json=$(curl -sL "$api_url" 2>/dev/null)
        echo "$json" | jq -r '.assets[] | select(.name | endswith(".apk")) | .browser_download_url' | head -1
    else
        # 降级：用 grep/sed 手动解析（简单但脆弱）
        json=$(curl -sL "$api_url" 2>/dev/null)
        echo "$json" | grep -oP '"browser_download_url":\s*"\K[^"]+\.apk' | head -1
    fi
}

# --------------------------------------------------
# 函数：下载文件
# 参数：URL, 目标路径
# --------------------------------------------------
download_apk() {
    local url="$1"
    local dest="$2"
    echo "  下载: $url"
    if command -v curl >/dev/null 2>&1; then
        curl -L -o "$dest" "$url" || return 1
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$dest" "$url" || return 1
    else
        echo "错误: 需要 curl 或 wget 才能下载。" >&2
        return 1
    fi
}

# --------------------------------------------------
# 函数：尝试安装 APK（调起系统安装器）
# --------------------------------------------------
install_apk() {
    local apk_path="$1"
    echo "  启动安装器，请在弹出窗口中手动确认安装。"
    termux-open "$apk_path" 2>/dev/null || am start -a android.intent.action.VIEW -d "file://${apk_path}" -t application/vnd.android.package-archive
}

# --------------------------------------------------
# 处理单个应用：检测、下载、安装、重试、提醒
# --------------------------------------------------
handle_pkg() {
    local idx="$1"
    local pkg="${PACKAGES[$idx]}"
    local repo="${REPOS[$idx]}"
    local dname="${DISPLAY_NAMES[$idx]}"
    local apk_path="${TMPDIR}/${pkg}.apk"
    
    echo "========================================="
    echo "处理 ${dname} (包名: ${pkg})"
    echo "========================================="
    
    # 1. 检测是否已安装
    if is_installed "$pkg"; then
        echo "✓ 已安装，跳过。"
        return 0
    fi
    
    echo "未安装，开始下载最新版 APK..."
    
    # 2. 获取下载链接
    local apk_url
    apk_url=$(get_latest_apk_url "$repo")
    if [ -z "$apk_url" ]; then
        echo "错误: 无法获取 ${dname} 的下载链接，使用备用固定链接..."
        # 备用固定链接（可能过时，但作为最后手段）
        case "$pkg" in
            com.termux.styling)
                apk_url="https://f-droid.org/repo/com.termux.styling_30.apk" ;;
            com.termux.api)
                apk_url="https://f-droid.org/repo/com.termux.api_51.apk" ;;
        esac
        if [ -z "$apk_url" ]; then
            echo "错误: 无可用下载链接，请手动安装 ${dname}" >&2
            return 1
        fi
    fi
    
    # 3. 下载
    rm -f "$apk_path"
    if ! download_apk "$apk_url" "$apk_path"; then
        echo "下载失败，请检查网络并手动安装 ${dname}。" >&2
        return 1
    fi
    echo "  下载完成: $(ls -lh "$apk_path" | awk '{print $5}')"
    
    # 4. 安装（需要人工点击）
    install_apk "$apk_path"
    
    # 5. 等待安装并再次检测（给用户一些时间）
    echo "  等待 5 秒后重新检测..."
    sleep 5
    if is_installed "$pkg"; then
        echo "✓ ${dname} 安装成功！"
        return 0
    fi
    
    # 6. 第一次安装失败，进入修复流程
    echo "提示: 未检测到安装成功，尝试修复..."
    
    # 修复尝试1：重新下载并安装（可能下载不完整）
    echo "  重新下载..."
    rm -f "$apk_path"
    if ! download_apk "$apk_url" "$apk_path"; then
        echo "修复失败（下载错误），跳过。"
    else
        install_apk "$apk_path"
        sleep 5
        if is_installed "$pkg"; then
            echo "✓ 修复后安装成功！"
            return 0
        fi
    fi
    
    # 修复尝试2：使用 am start 直接打开 Play 商店或 F-Droid 页面
    echo "  尝试打开应用商店页面（需手动安装）..."
    am start -a android.intent.action.VIEW -d "https://f-droid.org/packages/${pkg}/" 2>/dev/null || \
    am start -a android.intent.action.VIEW -d "market://details?id=${pkg}" 2>/dev/null || true
    sleep 3
    
    # 最终检测
    if is_installed "$pkg"; then
        echo "✓ 最终检测通过，${dname} 已安装。"
        return 0
    fi
    
    # 7. 仍然失败，提醒用户
    echo "-------------------------------------------------"
    echo "⚠  自动安装失败，请手动完成以下操作："
    echo "   1. 打开 F-Droid 或 Google Play 商店"
    echo "   2. 搜索并安装 '${dname}'"
    echo "   3. 或在浏览器中打开: https://f-droid.org/packages/${pkg}/"
    echo "-------------------------------------------------"
    return 1
}

# --------------------------------------------------
# 主流程
# --------------------------------------------------
echo "Termux 关键组件安装/检查脚本"
echo ""

FAIL_COUNT=0
for i in "${!PACKAGES[@]}"; do
    if ! handle_pkg "$i"; then
        FAIL_COUNT=$((FAIL_COUNT+1))
    fi
    echo ""
done

# 清理临时文件
rm -rf "$TMPDIR"

if [ $FAIL_COUNT -gt 0 ]; then
    echo "有 ${FAIL_COUNT} 个组件未能自动安装，请按照提示手动处理。"
    exit 1
else
    echo "所有组件均已就绪。"
    exit 0
fi
