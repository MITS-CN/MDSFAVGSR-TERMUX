termux-setup-storage

# 检测并确保Termux拥有共享存储权限
ensure_storage_permission() {
    # 1. 验证共享存储是否可读
    if [ -r "$HOME/storage/shared" ]; then
        # 静默测试写入能力
        if touch "$HOME/storage/shared/.termux-perm-test-temp" 2>/dev/null; then
            rm -f "$HOME/storage/shared/.termux-perm-test-temp"
            return 0
        else
            echo "存储目录可读但不可写" >&2
        fi
    else
        echo "未检测到有效的存储权限" >&2
    fi

    # 2. 尝试主动请求授权
    echo "正在请求存储权限..." >&2
    termux-setup-storage
    
    # 3. 再次验证结果
    if [ -r "$HOME/storage/shared" ]; then
        if touch "$HOME/storage/shared/.termux-perm-test" 2>/dev/null; then
            rm -f "$HOME/storage/shared/.termux-perm-test"
            echo "存储权限获取成功" >&2
            return 0
        fi
    fi

    # 4. 如果仍然失败，返回错误
    echo "获取存储权限失败" >&2
    return 1
}

# 获取脚本所在目录的绝对路径（解析符号链接）
script_dir=$(cd "$(dirname "$0")" && pwd -P)

# 获取当前工作目录的绝对路径（解析符号链接）
current_dir=$(pwd -P)

# 检测是否为 Termux 环境
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

# 网络检测主函数
# 返回: 0 表示有可用网络连接，1 表示无连接
network_connected() {
    # 优先使用 curl（HTTP 检测最可靠）
    if command -v curl >/dev/null 2>&1; then
        if curl -s --max-time 3 -o /dev/null "http://connectivitycheck.gstatic.com/generate_204"; then
            return 0
        fi
    fi

    # 其次使用 ping
    if command -v ping >/dev/null 2>&1; then
        if ping -c 1 -W 2 8.8.8.8 >/dev/null 2>&1; then
            return 0
        fi
        if ping -c 1 -W 2 114.114.114.114 >/dev/null 2>&1; then
            return 0
        fi
    fi

    # 最后尝试 Termux Wi‑Fi API（仅检查 Wi‑Fi 状态，不保证互联网连通）
    if command -v termux-wifi-connectioninfo >/dev/null 2>&1; then
        if termux-wifi-connectioninfo 2>/dev/null | grep -q '"supplicant_state": "COMPLETED"'; then
            return 0
        fi
    fi

    return 1
}

# 仅检查Termux是否拥有共享存储权限
check_storage_permission() {
    # 检查符号链接是否可读
    [ -r "$HOME/storage/shared" ] || return 1

    # 测试写入能力
    if touch "$HOME/storage/shared/.termux-perm-test" 2>/dev/null; then
        rm -f "$HOME/storage/shared/.termux-perm-test"
        return 0
    else
        return 1
    fi
}

# 检查 Android 版本是否 ≥ 7.0
# 返回值: 0 表示满足条件，1 表示不满足或无法获取
check_android_version() {
    # 获取 Android 版本号字符串，例如 "7.0", "10", "12"
    android_ver=$(getprop ro.build.version.release 2>/dev/null)

    # 若无法获取，尝试通过 sdk_int 判断 (≥24 即为 Android 7+)
    if [ -z "$android_ver" ]; then
        sdk=$(getprop ro.build.version.sdk 2>/dev/null)
        if [ -n "$sdk" ] && [ "$sdk" -ge 24 ] 2>/dev/null; then
            return 0
        else
            echo "警告: 无法获取 Android 版本信息" >&2
            return 1
        fi
    fi

    # 提取主版本号（小数点前的部分）
    major_ver=$(echo "$android_ver" | cut -d. -f1)

    # 比较主版本号是否 ≥ 7
    if [ "$major_ver" -ge 7 ] 2>/dev/null; then
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

# 调用检测函数
if check_android_version; then
    echo "当前 Android 版本满足要求 (≥7.0)"
    # 继续执行需要 Android 7+ 的操作
else
    echo "当前 Android 版本过低或无法获取，需要 Android 7.0 及以上系统"
    exit 1
fi

# --- 你的原有代码 ---
echo "开始执行任务..."
echo "正在检查网络连接..."

# --- 调用网络检测 ---
if ! network_connected; then
    echo "错误：网络未连接，请检查网络后重试。" >&2
    exit 1
fi

echo "网络正常，继续执行..."

# 判断当前目录是否等于脚本目录
if [ "$current_dir" = "$script_dir" ]; then
    if ! ensure_storage_permission; then
        echo "脚本需要存储权限才能继续，请检查Termux权限设置后重试。"
        exit 1
    fi
    
    if ! check_storage_permission; then
        echo "未检测到存储权限，尝试获取..."
        termux-setup-storage
        if ! check_storage_permission; then
            echo "获取存储权限失败，脚本退出。"
            exit 1
        fi
    fi
    
    echo "存储权限就绪，开始执行任务..."
    echo "当前目录是脚本所在目录。"
    mkdir -p /storage/emulated/0/MITS/TEMP/
    mkdir -p /storage/emulated/0/MITS/data/bin/
    cp -r ./* /storage/emulated/0/MITS
    chmod +x /storage/emulated/0/MITS/data/bin/init.rc
    bash /storage/emulated/0/MITS/data/bin/init.rc
# 判断当前目录是否是脚本目录的子目录
elif [[ "$current_dir" == "$script_dir"/* ]]; then
    echo "当前目录是脚本所在目录的子目录,不予执行"
else
    echo "当前目录不在脚本所在目录内,不予执行"
fi  