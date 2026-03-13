import subprocess

print("是否使用 tome 大佬的整合脚本？")
print("输入 1 使用，否则退出")
user_input = input("USER_PD>")

if user_input.strip() == "1":          # 直接比较字符串
    subprocess.run(
        "/data/data/com.termux/files/usr/bin/bash -c \"$(curl -L l.tmoe.me)\"",
        shell=True
    )
else:
    print("退出")