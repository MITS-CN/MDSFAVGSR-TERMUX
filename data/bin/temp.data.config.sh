#!/data/data/com.termux/files/usr/bin/bash
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
    "MITS_version": "Build.IS0057(main:NULL)_fix",
    "MITS_build_by": "Administrator",
    "MITS_Diskpart_copyright": "(c) Microsoft Corporation",
    "MITS_Diskpart_version": "Microsoft DiskPart 版本 10.0.17763.1",
    "MITS_Diskpart_host": "在计算机上: ANDROID"
}
EOF
    echo "已创建默认配置文件 $CONFIG_DIR/config.json"
    chmod 755 /data/data/com.termux/files/usr/etc/MITS/config.json

fi
