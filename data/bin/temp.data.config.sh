#!/data/data/com.termux/files/usr/bin/bash

rm -rf /data/data/com.termux/files/usr/etc/MITS

mkdir /data/data/com.termux/files/usr/etc/MITS
mkdir /data/data/com.termux/files/usr/etc/MITS/SL

if [ -f "/data/data/com.termux/files/usr/etc/MITS/SL/config.json.example" ]; then
    cp -n "/data/data/com.termux/files/usr/etc/MITS/SL/config.json.example" "/data/data/com.termux/files/usr/etc/MITS/config.json"
    echo "已安装示例配置文件到 /data/data/com.termux/files/usr/etc/MITS/config.json"
elif [ ! -f "/data/data/com.termux/files/usr/etc/MITS/config.json" ]; then
    # 创建默认配置文件
    cat > "/data/data/com.termux/files/usr/etc/MITS/config.json" <<EOF
{
    "MITS_build_version": "0049_A",
    "MITS_build_by": "Administrator"
}
EOF
    echo "已创建默认配置文件 $CONFIG_DIR/config.json"
fi
