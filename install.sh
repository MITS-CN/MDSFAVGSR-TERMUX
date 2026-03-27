termux-setup-storage

# 获取脚本所在目录的绝对路径（解析符号链接）
script_dir=$(cd "$(dirname "$0")" && pwd -P)

# 获取当前工作目录的绝对路径（解析符号链接）
current_dir=$(pwd -P)

# 判断当前目录是否等于脚本目录
if [ "$current_dir" = "$script_dir" ]; then
    echo "当前目录是脚本所在目录。"
    mkdir /storage/emulated/0/MITS
    cp -r ./* /storage/emulated/0/MITS
    chmod +x /storage/emulated/0/MITS/data/bin/init.rc
    bash /storage/emulated/0/MITS/data/bin/init.rc
    rm -rf /storage/emulated/0/MITS/
# 判断当前目录是否是脚本目录的子目录
elif [[ "$current_dir" == "$script_dir"/* ]]; then
    echo "当前目录是脚本所在目录的子目录,不予执行"
else
    echo "当前目录不在脚本所在目录内,不予执行"
fi