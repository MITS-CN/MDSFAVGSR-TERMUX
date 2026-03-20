apt list --upgradable
pkg update

pkg install clang -y
pkg install gcc -y

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

curl -s -o x https://raw.githubusercontent.com/olegos2/termux-box/main/install && chmod +x x && ./x

sh -c "$(curl -fsSL https://raw.githubusercontent.com/zdharma-continuum/zinit/main/scripts/install.sh)"

bash -c "$(curl -L gitee.com/mo2/zsh/raw/2/2)"

git clone --depth=1 https://bgithub.xyz/romkatv/powerlevel10k.git ~/powerlevel10k

wget https://github.com/nlohmann/json/releases/latest/download/json.hpp

pkg update
