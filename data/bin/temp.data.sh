apt list --upgradable
pkg list-all
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

pkg install nano
pkg install rush
pkg install jq
pkg install x11-repo
pkg install sqlite
pkg install -y zsh git bat fzf net-tools traceroute dnsutils ack termux-api
pkg install termux-elf-cleaner

pkg update

pkg install x11-repo
pkg install xfdesktop
pkg install vim -y

pkg install proot-distro -y
pkg install pulseaudio -y



#other

sh -c "$(curl -fsSL https://raw.githubusercontent.com/zdharma-continuum/zinit/main/scripts/install.sh)"

bash -c "$(curl -L gitee.com/mo2/zsh/raw/2/2)"

git clone --depth=1 https://bgithub.xyz/romkatv/powerlevel10k.git ~/powerlevel10k

wget -O /storage/emulated/0/MITS/TEMP/json.hpp "https://github.com/nlohmann/json/releases/latest/download/json.hpp"

pkg update
pkg upgrade