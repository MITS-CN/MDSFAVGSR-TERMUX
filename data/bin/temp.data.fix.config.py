#!/data/data/com.termux/files/usr/bin/python3
# -*- coding: utf-8 -*-
"""
MITS 配置初始化脚本（按照 Bash 版本逻辑重写）
"""

import os
import stat
import sys
import shutil

def is_termux():
    """综合多种特征判断当前环境是否为 Termux"""
    # 1. 环境变量 TERMUX_VERSION（Termux 特有，最权威）
    if os.environ.get("TERMUX_VERSION"):
        return True

    # 2. PREFIX 路径特征
    prefix = os.environ.get("PREFIX", "")
    if prefix.startswith("/data/data/com.termux/files"):
        return True

    # 3. HOME 路径特征
    home = os.environ.get("HOME", "")
    if home.startswith("/data/data/com.termux/files/home"):
        return True

    # 4. 典型目录是否存在
    if os.path.isdir("/data/data/com.termux"):
        return True

    # 5. 典型可执行文件是否存在
    for cmd_path in [
        "/data/data/com.termux/files/usr/bin/termux-info",
        "/data/data/com.termux/files/usr/bin/bash"
    ]:
        if os.path.isfile(cmd_path):
            return True

    # 6. 环境变量 LD_PRELOAD
    ld_preload = os.environ.get("LD_PRELOAD", "")
    if "libtermux-exec.so" in ld_preload:
        return True

    return False

# 颜色定义
GREEN = '\033[0;32m'
RED = '\033[0;31m'
YELLOW = '\033[1;33m'
BLUE = '\033[0;34m'
NC = '\033[0m'  # No Color

# 路径配置
PREFIX = "/data/data/com.termux/files/usr"
CONFIG_DIR = os.path.join(PREFIX, "etc", "MITS")
SL_DIR = os.path.join(CONFIG_DIR, "SL")
DISKPART_DIR = os.path.join(CONFIG_DIR, "diskpart")
CONFIG_FILE = os.path.join(CONFIG_DIR, "config.json")
DISKPART_CONFIG_FILE = os.path.join(DISKPART_DIR, "config.json")
EXAMPLE_CONFIG_FILE = os.path.join(SL_DIR, "config.json.example")

# 期望权限
DIR_PERMS = 0o755
FILE_PERMS = 0o755

# 默认配置文件内容
DEFAULT_MAIN_CONFIG = '''{
    "MITS_version": "Build.IS0064(main:NULL)",
    "MITS_build_by": "Administrator"
}
'''

DEFAULT_DISKPART_CONFIG = '''{
    "MITS_Diskpart_copyright": "(c) Microsoft Corporation",
    "MITS_Diskpart_version": "Microsoft DiskPart 版本 10.0.17763.1",
    "MITS_Diskpart_host": "在计算机上: ANDROID"
}
'''

def set_perms(path, perms):
    """设置文件/目录权限，忽略错误"""
    try:
        os.chmod(path, perms)
        return True
    except OSError:
        return False

def main():
    print("===== MITS 配置初始化脚本 =====")
    print()

    # 1. 检查是否为 Termux 环境
    if not is_termux():
        print(f"{RED}[✗]{NC} 当前环境不是 Termux。")
        sys.exit(1)
    print(f"{GREEN}[✓]{NC} 当前环境是 Termux。")

    # 2. 删除旧的配置目录（如果存在）
    if os.path.exists(CONFIG_DIR):
        try:
            shutil.rmtree(CONFIG_DIR)
            print(f"{YELLOW}[!]{NC} 已删除旧目录: {CONFIG_DIR}")
        except OSError as e:
            print(f"{RED}[✗]{NC} 无法删除目录 {CONFIG_DIR}: {e}")
            sys.exit(1)

    # 3. 创建新目录结构
    dirs_to_create = [CONFIG_DIR, SL_DIR, DISKPART_DIR]
    for d in dirs_to_create:
        try:
            os.makedirs(d, mode=DIR_PERMS, exist_ok=True)
            # 确保权限正确
            set_perms(d, DIR_PERMS)
            print(f"{GREEN}[✓]{NC} 创建目录并设置权限: {d}")
        except OSError as e:
            print(f"{RED}[✗]{NC} 创建目录失败 {d}: {e}")
            sys.exit(1)

    # 4. 处理配置文件
    # 4a. 如果存在示例配置文件，复制到主配置文件（不覆盖，但此时目录刚创建，目标肯定不存在）
    if os.path.isfile(EXAMPLE_CONFIG_FILE):
        try:
            shutil.copy2(EXAMPLE_CONFIG_FILE, CONFIG_FILE)
            set_perms(CONFIG_FILE, FILE_PERMS)
            print(f"{GREEN}[✓]{NC} 已从示例文件复制配置文件: {CONFIG_FILE}")
        except OSError as e:
            print(f"{RED}[✗]{NC} 复制示例配置文件失败: {e}")
            sys.exit(1)
    # 4b. 否则，如果主配置文件不存在，则创建默认配置
    elif not os.path.exists(CONFIG_FILE):
        try:
            # 创建主配置文件
            with open(CONFIG_FILE, 'w', encoding='utf-8') as f:
                f.write(DEFAULT_MAIN_CONFIG)
            set_perms(CONFIG_FILE, FILE_PERMS)
            print(f"{GREEN}[✓]{NC} 已创建默认主配置文件: {CONFIG_FILE}")

            # 创建 diskpart 配置文件
            with open(DISKPART_CONFIG_FILE, 'w', encoding='utf-8') as f:
                f.write(DEFAULT_DISKPART_CONFIG)
            set_perms(DISKPART_CONFIG_FILE, FILE_PERMS)
            print(f"{GREEN}[✓]{NC} 已创建 diskpart 配置文件: {DISKPART_CONFIG_FILE}")
        except OSError as e:
            print(f"{RED}[✗]{NC} 创建默认配置文件失败: {e}")
            sys.exit(1)
    else:
        # 理论上不会走到这里（因为目录已删除），但保留分支
        print(f"{BLUE}[i]{NC} 配置文件已存在: {CONFIG_FILE}")
        set_perms(CONFIG_FILE, FILE_PERMS)

    # 如果 diskpart 配置文件尚未创建（例如从示例复制时没有生成它），这里可以不处理，
    # 但为了完整性，我们可以检查一下它的存在并设置权限。
    if not os.path.exists(DISKPART_CONFIG_FILE):
        try:
            with open(DISKPART_CONFIG_FILE, 'w', encoding='utf-8') as f:
                f.write(DEFAULT_DISKPART_CONFIG)
            set_perms(DISKPART_CONFIG_FILE, FILE_PERMS)
            print(f"{GREEN}[✓]{NC} 已补建 diskpart 配置文件: {DISKPART_CONFIG_FILE}")
        except OSError as e:
            print(f"{RED}[✗]{NC} 无法创建 diskpart 配置文件: {e}")

    print("\n===== 初始化完成 =====")
    print("所有目录和配置文件已就绪。")

if __name__ == "__main__":
    main()