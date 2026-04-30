#!/data/data/com.termux/files/usr/bin/bash

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

chmod 750 /data/data/com.termux/files/home/.zinit/completions
chmod 750 /data/data/com.termux/files/usr/share/zsh
chmod 750 /data/data/com.termux/files/usr/share/zsh/5.7.1
chmod 750 /data/data/com.termux/files/usr/share/zsh/site-functions
chmod 750 /data/data/com.termux/files/usr/share/zsh/5.7.1/functions

directories=(
    "/data/data/com.termux/files/home/.zinit/completions"
    "/data/data/com.termux/files/usr/share/zsh"
    "/data/data/com.termux/files/usr/share/zsh/5.7.1"
    "/data/data/com.termux/files/usr/share/zsh/site-functions"
    "/data/data/com.termux/files/usr/share/zsh/5.7.1/functions"
)

expected_perm=750

echo "检查以下目录权限是否为 $expected_perm ..."
echo "----------------------------------------"

need_fix=()

for dir in "${directories[@]}"; do
    if [[ ! -e "$dir" ]]; then
        echo "路径不存在: $dir"
        continue
    fi

    perm=$(stat -c "%a" "$dir" 2>/dev/null)
    if [[ -z "$perm" ]]; then
        echo "无法读取权限: $dir"
        continue
    fi

    if [[ "$perm" -eq "$expected_perm" ]]; then
        echo "权限正确 ($perm): $dir"
    else
        echo "权限错误 (当前=$perm, 期望=$expected_perm): $dir"
        need_fix+=("$dir")
    fi
done

if [[ ${#need_fix[@]} -gt 0 ]]; then
    echo "----------------------------------------"
    echo "开始修复以下目录权限为 $expected_perm ..."
    for dir in "${need_fix[@]}"; do
        if chmod "$expected_perm" "$dir" 2>/dev/null; then
            current_perm=$(stat -c "%a" "$dir" 2>/dev/null)
            if [[ "$current_perm" == "$expected_perm" ]]; then
                echo "修复成功 ($current_perm): $dir"
            else
                echo "设置后权限仍为 $current_perm，可能需要更高权限: $dir"
            fi
        else
            echo "修复失败（权限不足或错误）: $dir"
        fi
    done
    echo "----------------------------------------"
    echo "修复完成。"
else
    echo "----------------------------------------"
    echo "所有目录权限均已正确，无需修复。"
fi

echo "----------------------------------------"
echo "检查完成。"