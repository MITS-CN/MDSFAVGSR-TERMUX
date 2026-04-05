#!/data/data/com.termux/files/usr/bin/python3
# -*- coding: utf-8 -*-
"""
修复 MITS 配置目录及配置文件
"""

import os
import stat
import sys

# 颜色定义
GREEN = '\033[0;32m'
RED = '\033[0;31m'
YELLOW = '\033[1;33m'
BLUE = '\033[0;34m'
NC = '\033[0m'  # No Color

# 路径配置
CONFIG_DIR = "/data/data/com.termux/files/usr/etc/MITS"
SL_DIR = os.path.join(CONFIG_DIR, "SL")
CONFIG_FILE = os.path.join(CONFIG_DIR, "config.json")

# 期望权限（八进制）
EXPECTED_PERMS = 0o755

# 默认配置文件内容
DEFAULT_CONFIG = '''{
    "MITS_version": "Build.IS0057(main:FIX)_online",
    "MITS_build_by": "Administrator",
    "MITS_Diskpart_copyright": "(c) Microsoft Corporation",
    "MITS_Diskpart_version": "Microsoft DiskPart 版本 10.0.17763.1",
    "MITS_Diskpart_host": "在计算机上: ANDROID"
}
'''


def ensure_dir(path, name):
    """确保目录存在且权限正确"""
    created = False
    perm_fixed = False

    if not os.path.exists(path):
        try:
            os.makedirs(path, mode=EXPECTED_PERMS, exist_ok=True)
            print(f"{GREEN}[✓]{NC} 创建目录: {name} ({path})")
            created = True
        except OSError as e:
            print(f"{RED}[✗]{NC} 创建目录失败 {name}: {e}")
            return False
    else:
        print(f"{BLUE}[i]{NC} 目录已存在: {name} ({path})")

    # 检查并修正权限
    try:
        mode = os.stat(path).st_mode
        perms = stat.S_IMODE(mode)
        if perms != EXPECTED_PERMS:
            os.chmod(path, EXPECTED_PERMS)
            print(f"{GREEN}[✓]{NC} 修正权限 {name}: {oct(perms)[2:]} → {oct(EXPECTED_PERMS)[2:]}")
            perm_fixed = True
        else:
            print(f"{GREEN}[✓]{NC} 权限正确: {name} ({oct(perms)[2:]})")
    except OSError as e:
        print(f"{RED}[✗]{NC} 无法检查/修正权限 {name}: {e}")
        return False

    return True


def ensure_config_file():
    """确保配置文件存在且权限正确"""
    if not os.path.exists(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, 'w', encoding='utf-8') as f:
                f.write(DEFAULT_CONFIG)
            os.chmod(CONFIG_FILE, EXPECTED_PERMS)
            print(f"{GREEN}[✓]{NC} 创建配置文件: {CONFIG_FILE}")
            return True
        except OSError as e:
            print(f"{RED}[✗]{NC} 创建配置文件失败: {e}")
            return False
    else:
        print(f"{BLUE}[i]{NC} 配置文件已存在: {CONFIG_FILE}")
        # 修正权限（如果不对）
        try:
            mode = os.stat(CONFIG_FILE).st_mode
            perms = stat.S_IMODE(mode)
            if perms != EXPECTED_PERMS:
                os.chmod(CONFIG_FILE, EXPECTED_PERMS)
                print(f"{GREEN}[✓]{NC} 修正配置文件权限: {oct(perms)[2:]} → {oct(EXPECTED_PERMS)[2:]}")
            else:
                print(f"{GREEN}[✓]{NC} 配置文件权限正确: {oct(perms)[2:]}")
        except OSError as e:
            print(f"{RED}[✗]{NC} 无法修正配置文件权限: {e}")
            return False

        # 可选：检查内容是否非空（但保留现有内容）
        if os.path.getsize(CONFIG_FILE) == 0:
            print(f"{YELLOW}[!]{NC} 配置文件为空，但保留现有文件（未覆盖）")
        return True


def main():
    print("===== MITS 配置修复工具 =====")
    print()

    # 1. 确保主目录存在且权限正确
    print(">>> 处理主目录")
    if not ensure_dir(CONFIG_DIR, "主目录"):
        print(f"{RED}[✗]{NC} 主目录处理失败，无法继续")
        sys.exit(1)

    # 2. 确保子目录存在且权限正确
    print("\n>>> 处理子目录 SL")
    if not ensure_dir(SL_DIR, "子目录 SL"):
        print(f"{RED}[✗]{NC} 子目录处理失败")
        sys.exit(1)

    # 3. 确保配置文件存在且权限正确
    print("\n>>> 处理配置文件")
    if not ensure_config_file():
        print(f"{RED}[✗]{NC} 配置文件处理失败")
        sys.exit(1)

    print("\n===== 修复完成 =====")
    print("所有检查项已处理，配置应该已就绪。")


if __name__ == "__main__":
    main()