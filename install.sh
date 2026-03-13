termux-setup-storage
mkdir /storage/emulated/0/MITS
cp -r ./* /storage/emulated/0/MITS
chmod +x /storage/emulated/0/MITS/data/bin/init.rc
bash /storage/emulated/0/MITS/data/bin/init.rc
rm -rf /storage/emulated/0/MITS/