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

CONFIG_DIR="/data/data/com.termux/files/usr/etc/MITS"

rm -rf /data/data/com.termux/files/usr/etc/MITS

mkdir /data/data/com.termux/files/usr/etc/MITS
mkdir /data/data/com.termux/files/usr/etc/MITS/SL

chmod 755 /data/data/com.termux/files/usr/etc/MITS
chmod 755 /data/data/com.termux/files/usr/etc/MITS/SL

if [ -f "/data/data/com.termux/files/usr/etc/MITS/SL/config.json.example" ]; then
    cp -n "/data/data/com.termux/files/usr/etc/MITS/SL/config.json.example" "/data/data/com.termux/files/usr/etc/MITS/config.json"
    echo "已安装示例配置文件到 /data/data/com.termux/files/usr/etc/MITS/config.json"
elif [ ! -f "/data/data/com.termux/files/usr/etc/MITS/config.json" ]; then
    # 创建默认配置文件
    # 其实应该分开来的这里
    cat > "/data/data/com.termux/files/usr/etc/MITS/config.json" <<EOF
{
    "MITS_version": "Build.IS0060(main:NULL)_fix",
    "MITS_build_by": "Administrator",
    "MITS_Diskpart_copyright": "(c) Microsoft Corporation",
    "MITS_Diskpart_version": "Microsoft DiskPart 版本 10.0.17763.1",
    "MITS_Diskpart_host": "在计算机上: ANDROID"
}
EOF
    echo "已创建默认配置文件 $CONFIG_DIR/config.json"
    chmod 755 /data/data/com.termux/files/usr/etc/MITS/config.json

fi
