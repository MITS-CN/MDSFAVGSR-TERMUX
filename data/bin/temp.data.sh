#!/data/data/com.termux/files/usr/bin/bash
#
# test_pkg_install.sh
# 用途：按顺序执行一组 pkg 安装/更新命令，出错时调用 temp.data.check.install.sh
#

# 错误处理函数
error_handler() {
    echo "安装脚本执行出错，调用修复脚本中......"
    temp.data.check.install.sh
}

# 启用遇到错误立即退出，并绑定错误陷阱
set -e
trap 'error_handler' ERR

echo "==================== 开始测试 pkg 相关操作 ===================="

# --- 检查镜像并更新 ---
echo ">>> 1. 检查镜像: pkg --check-mirror update"
pkg --check-mirror update

echo ">>> 2. pkg update"
pkg update

echo ">>> 3. pkg upgrade"
pkg upgrade -y

# --- 安装开发工具 ---
echo ">>> 4. 安装 clang"
pkg install clang -y

echo ">>> 5. 安装 gcc"
pkg install gcc -y

# 注意：list-installed 理论上是一个参数，而非包名，如果出错我们捕获但继续
echo ">>> 6. pkg install list-installed (可能会失败，跳过错误)"
pkg install list-installed -y || true

echo ">>> 7. 安装 openssl-tool"
pkg install openssl-tool -y

echo ">>> 8. 安装 openssl"
pkg install openssl -y

echo ">>> 9. 安装 python"
pkg install python -y

echo ">>> 10. 安装 zsh"
pkg install zsh -y

echo ">>> 11. 批量安装: git pv pulseaudio proot zstd bat termux-api aria2"
pkg install -y git pv pulseaudio proot zstd bat termux-api aria2

echo ">>> 12. 安装 eza fzf wget"
pkg install -y eza fzf wget

echo ">>> 13. 安装 pv (重复)"
pkg install pv -y

echo ">>> 14. 安装 tree"
pkg install tree -y

echo ">>> 15. 安装 rsync"
pkg install rsync -y

echo ">>> 16. 安装 neofetch"
pkg install neofetch -y

echo ">>> 17. 安装 eza (重复)"
pkg install -y eza

echo ">>> 18. 安装 traceroute"
pkg install traceroute -y

echo ">>> 19. 安装 dnsutils"
pkg install dnsutils -y

echo ">>> 20. 安装 nano"
pkg install nano -y

echo ">>> 21. 安装 rush"
pkg install rush -y

echo ">>> 22. 安装 jq"
pkg install jq -y

echo ">>> 23. 安装 sqlite"
pkg install sqlite -y

echo ">>> 24. 安装 zsh (重复)"
pkg install zsh -y

echo ">>> 25. 安装 git (重复)"
pkg install git -y

echo ">>> 26. 安装 bat (重复)"
pkg install bat -y

echo ">>> 27. 安装 fzf (重复)"
pkg install fzf -y

echo ">>> 28. 安装 net-tools"
pkg install net-tools -y

echo ">>> 29. 安装 traceroute (重复)"
pkg install traceroute -y

echo ">>> 30. 安装 dnsutils (重复)"
pkg install dnsutils -y

echo ">>> 31. 安装 ack"
pkg install ack -y

echo ">>> 32. 安装 termux-api (重复)"
pkg install termux-api -y

echo ">>> 33. 安装 termux-elf-cleaner"
pkg install termux-elf-cleaner -y

echo ">>> 34. 安装 vim"
pkg install vim -y

echo ">>> 35. 安装 proot-distro"
pkg install proot-distro -y

echo ">>> 36. 安装 pulseaudio (重复)"
pkg install pulseaudio -y

echo ">>> 37. 安装 ack-grep"
pkg install ack-grep -y

# --- 外部脚本与配置 ---
echo ">>> 38. 安装 zinit"
sh -c "$(curl -fsSL https://raw.githubusercontent.com/zdharma-continuum/zinit/main/scripts/install.sh)"

echo ">>> 39. 运行 zsh 配置脚本"
bash -c "$(curl -L gitee.com/mo2/zsh/raw/2/2)"

echo ">>> 40. 克隆 powerlevel10k"
git clone --depth=1 https://bgithub.xyz/romkatv/powerlevel10k.git ~/powerlevel10k

# --- 存储与文件准备 ---
echo ">>> 41. 设置存储权限"
termux-setup-storage

echo ">>> 42. 创建目录 /storage/emulated/0/MITS/TEMP/"
mkdir -p /storage/emulated/0/MITS/TEMP/

echo ">>> 43. 下载 json.hpp"
wget -O /storage/emulated/0/MITS/TEMP/json.hpp \
    "https://github.com/nlohmann/json/releases/latest/download/json.hpp"

# --- 最终更新 ---
echo ">>> 44. 再次 pkg update"
pkg update

echo ">>> 45. 再次 pkg upgrade"
pkg upgrade -y

echo "==================== 所有操作成功完成 ===================="

# 取消错误陷阱
trap - ERR
set +e