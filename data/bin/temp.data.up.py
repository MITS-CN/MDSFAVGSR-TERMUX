#!/usr/bin/env python3
"""
全局检查工具：
1. 检查 $HOME/storage/shared/MITS/TEMP/json.hpp 是否存在且为空
2. 若为空（或用户强制），则升级所有可升级的 pip 包
"""

import os
import sys
import subprocess
import argparse

TARGET_FILE = "$HOME/storage/shared/MITS/TEMP/json.hpp"

def is_file_empty(path: str) -> bool:
    """检查文件是否存在且大小为 0"""
    if not os.path.exists(path):
        return False
    return os.path.getsize(path) == 0

def get_outdated_packages() -> list:
    """获取所有可升级的包列表 (name, current, latest)"""
    try:
        result = subprocess.run(
            [sys.executable, "-m", "pip", "list", "--outdated", "--format=json"],
            capture_output=True,
            text=True,
            check=True
        )
        import json
        outdated = json.loads(result.stdout)
        return [(pkg['name'], pkg['version'], pkg['latest_version']) for pkg in outdated]
    except Exception as e:
        print(f"获取可升级包列表失败: {e}", file=sys.stderr)
        return []

def upgrade_packages(packages: list, dry_run: bool = False) -> bool:
    """升级指定的包列表，返回是否全部成功"""
    if not packages:
        print("没有发现可升级的包。")
        return True

    print(f"发现 {len(packages)} 个可升级的包:")
    for name, cur, latest in packages:
        print(f"  {name}: {cur} -> {latest}")

    if dry_run:
        print("(试运行模式，未实际执行升级)")
        return True

    # 逐个升级，避免因某个包失败导致全部停止
    success = True
    for name, cur, latest in packages:
        print(f"正在升级 {name}...")
        try:
            subprocess.run(
                [sys.executable, "-m", "pip", "install", "--upgrade", name],
                check=True
            )
            print(f"  ✓ {name} 升级成功")
        except subprocess.CalledProcessError as e:
            print(f"  ✗ {name} 升级失败: {e}", file=sys.stderr)
            success = False
    return success

def main():
    parser = argparse.ArgumentParser(description="检查文件是否为空，并升级可升级的 pip 包")
    parser.add_argument("--force", action="store_true", help="即使目标文件不存在或非空，也执行升级")
    parser.add_argument("--dry-run", action="store_true", help="只显示将要升级的包，不实际执行")
    args = parser.parse_args()

    # 1. 检查文件
    empty = is_file_empty(TARGET_FILE)
    if empty:
        print(f"文件 {TARGET_FILE} 存在且为空。")
    else:
        if os.path.exists(TARGET_FILE):
            print(f"文件 {TARGET_FILE} 存在但不为空 (大小 {os.path.getsize(TARGET_FILE)} 字节)。")
        else:
            print(f"文件 {TARGET_FILE} 不存在。")

    # 2. 决定是否执行升级
    if not args.force and not empty:
        print("由于目标文件不为空且未使用 --force 参数，跳过升级。")
        return

    print("开始检查可升级的 pip 包...")
    outdated = get_outdated_packages()
    if not upgrade_packages(outdated, dry_run=args.dry_run):
        sys.exit(1)

if __name__ == "__main__":
    main()