#!/data/data/com.termux/files/usr/bin/python3

import os
import stat
import sys
import subprocess

# 颜色定义（ANSI）
GREEN = '\033[0;32m'
RED = '\033[0;31m'
YELLOW = '\033[1;33m'
NC = '\033[0m'  # No Color

# 目标路径
CONFIG_DIR = "/data/data/com.termux/files/usr/etc/MITS"
SL_DIR = os.path.join(CONFIG_DIR, "SL")
CONFIG_FILE = os.path.join(CONFIG_DIR, "config.json")

# 期望的权限（八进制）
EXPECTED_PERMS = 0o755

# 统计变量
total = 0
success = 0
fail = 0


def check_dir(path, name):
    """检查目录存在性，如存在则检查权限（可选）"""
    global total, success, fail
    total += 1
    if not os.path.isdir(path):
        print(f"{RED}[✗]{NC} {name} 不存在: {path}")
        fail += 1
        return False
    else:
        print(f"{GREEN}[✓]{NC} {name} 存在: {path}")
        success += 1

        # 检查权限
        total += 1
        try:
            mode = os.stat(path).st_mode
            perms = stat.S_IMODE(mode)
            if perms == EXPECTED_PERMS:
                print(f"{GREEN}[✓]{NC} {name} 权限正确: {oct(perms)[2:]}")
                success += 1
            else:
                print(f"{RED}[✗]{NC} {name} 权限不正确: {oct(perms)[2:]} (应为 {oct(EXPECTED_PERMS)[2:]})")
                fail += 1
        except OSError as e:
            print(f"{RED}[✗]{NC} 无法获取 {name} 权限: {e}")
            fail += 1
        return True


def check_file(path, name):
    """检查文件存在性，如存在则检查权限、非空和内容"""
    global total, success, fail
    total += 1
    if not os.path.isfile(path):
        print(f"{RED}[✗]{NC} {name} 不存在: {path}")
        fail += 1
        return False
    else:
        print(f"{GREEN}[✓]{NC} {name} 存在: {path}")
        success += 1

        # 检查权限
        total += 1
        try:
            mode = os.stat(path).st_mode
            perms = stat.S_IMODE(mode)
            if perms == EXPECTED_PERMS:
                print(f"{GREEN}[✓]{NC} {name} 权限正确: {oct(perms)[2:]}")
                success += 1
            else:
                print(f"{RED}[✗]{NC} {name} 权限不正确: {oct(perms)[2:]} (应为 {oct(EXPECTED_PERMS)[2:]})")
                fail += 1
        except OSError as e:
            print(f"{RED}[✗]{NC} 无法获取 {name} 权限: {e}")
            fail += 1

        # 检查文件非空
        total += 1
        if os.path.getsize(path) > 0:
            print(f"{GREEN}[✓]{NC} {name} 非空")
            success += 1
        else:
            print(f"{RED}[✗]{NC} {name} 为空")
            fail += 1

        # 可选：检查关键字段
        try:
            with open(path, 'r', encoding='utf-8') as f:
                content = f.read()
                if "MITS_version" in content:
                    print(f"{GREEN}[✓]{NC} {name} 包含预期字段 (MITS_version)")
                else:
                    print(f"{YELLOW}[!]{NC} {name} 可能缺少预期字段 (MITS_version)")
        except Exception as e:
            print(f"{YELLOW}[!]{NC} 无法读取 {name} 内容: {e}")

        return True


def main():
    print("===== 检查 MITS 配置目录及文件 =====")
    print()

    # 1. 检查主目录
    check_dir(CONFIG_DIR, "主目录")

    # 2. 检查子目录 SL
    check_dir(SL_DIR, "子目录 SL")

    # 3. 检查配置文件
    check_file(CONFIG_FILE, "配置文件")

    print()
    print("===== 检查完成 =====")
    print(f"总计: {total}   {GREEN}成功: {success}{NC}   {RED}失败: {fail}{NC}")

    if fail > 0:
        print("部分检查失败，请检查配置安装过程。")
        result = subprocess.run(['python', '/data/data/com.termux/files/usr/bin/temp.data.fix.config.py'], capture_output=True, text=True)
        
        print(result.stdout)
        print(result.returncode)
        
        sys.exit(1)
    else:
        print("所有检查通过，MITS 配置已正确安装。")
        sys.exit(0)


if __name__ == "__main__":
    main()