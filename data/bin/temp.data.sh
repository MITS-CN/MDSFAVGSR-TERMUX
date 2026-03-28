pkg --check-mirror update
pkg update
pkg upgrade

pkg install clang -y
pkg install gcc -y
pkg install list-installed

pkg install openssl-tool -y
pkg install openssl -y
pkg install python -y
pkg install zsh -y
pkg install -y git pv pulseaudio proot zstd bat termux-api aria2
pkg install -y eza fzf wget
pkg install pv -y
pkg install tree -y
pkg install rsync -y
pkg install neofetch -y
pkg install -y eza 

# pkg install exa
# Checking availability of current mirror:
# [*] https://mirrors.cqupt.edu.cn/termux/termux-main: ok
# Hit:1 https://mirrors.cqupt.edu.cn/termux/termux-main stable InRelease
# Hit:2 https://mirrors.cqupt.edu.cn/termux/termux-root root InRelease
# Hit:3 https://mirrors.cqupt.edu.cn/termux/termux-x11 x11 InRelease
# Reading package lists... Done
# Building dependency tree... Done
# Reading state information... Done
# 30 packages can be upgraded. Run 'apt list --upgradable' to see them.
# Reading package lists... Done
# Building dependency tree... Done
# Reading state information... Done
# Package exa is not available, but is referred to by another package.
# This may mean that the package is missing, has been obsoleted, or
# is only available from another source
# However the following packages replace it:
  # eza

# E: Package 'exa' has no installation candidate

#综上所述，exa直接安装已被删除，接下来是eza的天下
pkg install traceroute -y
pkg install dnsutils -y

pkg install nano -y
pkg install rush -y
pkg install jq -y
pkg install sqlite -y
pkg install zsh -y
pkg install git -y
pkg install bat -y
pkg install fzf -y
pkg install net-tools -y
pkg install traceroute -y
pkg install dnsutils -y
pkg install ack -y
pkg install termux-api -y

pkg install termux-elf-cleaner

pkg install vim -y

pkg install proot-distro -y
pkg install pulseaudio -y
pkg install ack-grep -y


#other

sh -c "$(curl -fsSL https://raw.githubusercontent.com/zdharma-continuum/zinit/main/scripts/install.sh)"

bash -c "$(curl -L gitee.com/mo2/zsh/raw/2/2)"

git clone --depth=1 https://bgithub.xyz/romkatv/powerlevel10k.git ~/powerlevel10k

#后面c++程序使用
termux-setup-storage

mkdir -p /storage/emulated/0/MITS/TEMP/

wget -O /storage/emulated/0/MITS/TEMP/json.hpp "https://github.com/nlohmann/json/releases/latest/download/json.hpp" 

pkg update
pkg upgrade