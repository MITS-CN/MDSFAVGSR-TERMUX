#像模块一样工作

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


echo "正在安装依赖环境中...."
temp.data.sh

echo "正在检查依赖环境中...."
temp.data.check.install.sh

echo "正在升级中...."
temp.data.up.py

echo "正在修改zsh中...."
temp.data.fix.zsh.sh

echo "正在安装c++应用中...."
temp.data.make.C++.sh

echo "正在安装rust应用中...."
temp.data.make.Rust.sh

echo "正在安装C应用中......"
temp.data.make.C.sh

echo "正在检查配置文件中...."
temp.data.check.config.py

echo "正在检查自定义命令中...."
temp.data.check.cmd.sh

echo "正在修复部分文件权限问题中...."
temp.data.fix.on.sh

echo "正在安装zsh默认配置文件中...."
temp.user.zsh.init.py
echo "源文件已备份在相应目录下"

echo "正在安装termux附属应用中...."
temp.data.install.app.sh

echo "正在清理临时文件中...."
temp.data.clear.sh