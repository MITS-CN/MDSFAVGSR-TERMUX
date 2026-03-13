#!/data/data/com.termux/files/usr/bin/bash

# 定义目标目录
target_dir="/data/data/com.termux/files/usr/bin/"

# 检查目标目录是否存在
if [ ! -d "${target_dir}" ]; then
    echo "错误：目标目录 ${target_dir} 不存在"
    exit 1
fi

# 批量删除以 temp.data 开头的文件
echo "正在清理 ${target_dir} 下以 temp.data 与 temp.user 开头的文件..."
rm -fv "${target_dir}temp.data"*
rm -fv "${target_dir}temp.user"*
rm -fv "${target_dir}#temp.user"*
rm -fv "${target_dir}#temp.data"*
rm -fv "${target_dir}#del."*
# 清理完成提示
echo -e "\n清理完成！"