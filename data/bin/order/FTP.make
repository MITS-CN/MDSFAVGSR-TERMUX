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

# CloudinatorFTP 自动安装脚本 for Termux
# 用法: bash install_cloudinator.sh

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}   CloudinatorFTP 自动安装脚本 (Termux)  ${NC}"
echo -e "${GREEN}========================================${NC}"

# 1. 更新包列表并安装基础软件
echo -e "${YELLOW}[1/9] 更新软件包列表...${NC}"
pkg update -y && pkg upgrade -y

echo -e "${YELLOW}[2/9] 安装必要软件包 (python, git, cloudflared, rust)...${NC}"
pkg install rush -y
pkg install cloudflared -y
pkg install -y python git cloudflared rust  # rust 用于编译 bcrypt（如果需要）

# 2. 克隆项目仓库
echo -e "${YELLOW}[3/9] 克隆 CloudinatorFTP 仓库...${NC}"
if [ -d "CloudinatorFTP" ]; then
    echo -e "${YELLOW}目录已存在，跳过克隆。${NC}"
else
    git clone https://bgithub.xyz/NeoMatrix14241/CloudinatorFTP.git
fi
cd CloudinatorFTP

# 3. 升级 pip 并安装 Python 依赖
echo -e "${YELLOW}[4/9] 升级 pip...${NC}"
pip install --upgrade pip

echo -e "${YELLOW}[5/9] 安装 Python 依赖...${NC}"
# 尝试安装所有依赖，如果 bcrypt 失败则特殊处理
if pip install flask flask_cors bcrypt werkzeug zipstream-new watchdog; then
    echo -e "${GREEN}Python 依赖安装成功。${NC}"
else
    echo -e "${RED}bcrypt 安装失败，尝试安装 Rust 后重试...${NC}"
    pkg install -y rust  # 确保 Rust 已安装
    pip install bcrypt
    # 重新安装全部（确保其他依赖完整）
    pip install flask flask_cors werkzeug zipstream-new watchdog
fi

# 4. 授权存储访问
echo -e "${YELLOW}[6/9] 请求存储权限 (termux-setup-storage)...${NC}"
termux-setup-storage
echo -e "${GREEN}请在弹出的系统中允许存储权限。${NC}"
sleep 3

# 5. 配置存储路径
echo -e "${YELLOW}[7/9] 配置共享存储路径...${NC}"
if [ -f "setup_storage.py" ]; then
    echo -e "${GREEN}运行 setup_storage.py 进行路径配置...${NC}"
    python setup_storage.py
else
    echo -e "${RED}未找到 setup_storage.py，将手动创建配置。${NC}"
    # 简单引导：让用户输入路径
    echo "请输入你想共享的完整路径 (例如 /storage/emulated/0/Download):"
    read -r storage_path
    mkdir -p "$storage_path"
    # 写入 server_config.json 或 storage_config.json（根据项目实际）
    # 这里假设使用 server_config.json 存储路径字段
    cat > server_config.json <<EOF
{
  "storage_path": "$storage_path"
}
EOF
    echo -e "${GREEN}路径已保存到 server_config.json${NC}"
fi

# 6. 设置管理员密码
echo -e "${YELLOW}[8/9] 设置管理员账户...${NC}"
if [ -f "create_user.py" ]; then
    python create_user.py
else
    echo -e "${RED}未找到 create_user.py，请手动设置密码。${NC}"
    echo "默认用户名: admin，密码: password123（强烈建议登录后修改）"
fi

# 7. 完成安装，提示启动方式
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN} 安装完成！${NC}"
echo -e "${YELLOW}接下来请按以下步骤启动服务：${NC}"
echo -e "1. 在 当前终端 运行: ${GREEN}python dev_server.py${NC}"
echo -e "2. 另开一个 Termux 会话，运行: ${GREEN}cloudflared tunnel --url http://localhost:5000${NC}"
echo -e "3. 访问 cloudflared 输出的公网地址（例如 https://xxx.trycloudflare.com）"
echo -e "4. 使用你设置的用户名密码登录，并立即修改密码。"
echo -e ""
echo -e "${YELLOW}注意：保持 Termux 在后台运行，不要关闭。${NC}"
echo -e "${GREEN}========================================${NC}"