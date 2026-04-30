#!/usr/bin/env python3
"""
修复版ZSH配置文件复制脚本
修复了f-string中的反斜杠错误
"""

import os
import sys
import shutil
import stat
import time
from pathlib import Path

def is_termux():
    #"""综合多种特征判断当前环境是否为 Termux"""
    
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
    #    Termux 的数据目录，在 Android 中非常特定
    if os.path.isdir("/data/data/com.termux"):
        return True
    
    # 5. 典型可执行文件是否存在
    #    termux-info 是 Termux 专有命令
    for cmd_path in [
        "/data/data/com.termux/files/usr/bin/termux-info",
        "/data/data/com.termux/files/usr/bin/bash"
    ]:
        if os.path.isfile(cmd_path):
            return True
    
    
    # 6. 环境变量 LD_PRELOAD 经常使用 Termux 的 libtermux-exec.so
    ld_preload = os.environ.get("LD_PRELOAD", "")
    if "libtermux-exec.so" in ld_preload:
        return True
    
    return False

def print_color(text, color_code):
    """打印彩色文本"""
    colors = {
        'red': '\033[91m',
        'green': '\033[92m',
        'yellow': '\033[93m',
        'blue': '\033[94m',
        'purple': '\033[95m',
        'cyan': '\033[96m',
        'white': '\033[97m',
        'reset': '\033[0m',
    }
    print(f"{colors.get(color_code, '')}{text}{colors['reset']}")

def check_file_exists(path, description=""):
    """检查文件是否存在"""
    if os.path.exists(path):
        print_color(f"✓ {description}文件存在: {path}", "green")
        return True
    else:
        print_color(f"✗ {description}文件不存在: {path}", "red")
        return False

def backup_file(file_path, backup_suffix=".backup"):
    """备份文件"""
    if os.path.exists(file_path):
        backup_path = file_path + backup_suffix + "." + time.strftime("%Y%m%d_%H%M%S")
        try:
            shutil.copy2(file_path, backup_path)
            print_color(f"✓ 已创建备份: {backup_path}", "yellow")
            return backup_path
        except Exception as e:
            print_color(f"✗ 备份失败: {e}", "red")
            return None
    return None

def copy_zshrc_with_permissions():
    """复制.zshrc文件并设置权限"""
    
    print("=" * 60)
    print_color("ZSH配置文件复制工具", "cyan")
    print("=" * 60)
    
    # 定义路径
    source_zshrc = "/storage/emulated/0/MITS/data/bin/.zshrc"
    target_zshrc = "/data/data/com.termux/files/home/.zshrc"
    
    # 检查源文件
    print(f"源文件: {source_zshrc}")
    print(f"目标文件: {target_zshrc}")
    print()
    
    if not check_file_exists(source_zshrc, "源.zshrc"):
        print_color("\n请检查以下可能性:", "yellow")
        print("1. 确保MITS项目已正确下载")
        print("2. 检查文件路径是否正确")
        print("3. 确保Termux有存储权限")
        
        # 尝试查找其他可能的.zshrc文件
        print("\n正在搜索可能的.zshrc文件...")
        search_dirs = [
            "/storage/emulated/0/MITS",
            "/storage/emulated/0/MITS/data",
            "/storage/emulated/0/MITS/data/bin",
            "/sdcard/MITS",
            "/sdcard/MITS/data/bin",
        ]
        
        for search_dir in search_dirs:
            if os.path.exists(search_dir):
                for root, dirs, files in os.walk(search_dir):
                    for file in files:
                        if file == ".zshrc" or file.endswith(".zshrc"):
                            found_file = os.path.join(root, file)
                            print_color(f"找到: {found_file}", "green")
                            choice = input(f"是否使用此文件? (y/N): ").lower()
                            if choice == 'y':
                                source_zshrc = found_file
                                break
                    if source_zshrc != "/storage/emulated/0/MITS/data/bin/.zshrc":
                        break
                if source_zshrc != "/storage/emulated/0/MITS/data/bin/.zshrc":
                    break
    
    # 如果还是没找到，提示用户手动输入
    if not os.path.exists(source_zshrc):
        print("\n请手动输入源文件路径:")
        source_zshrc = input("路径: ").strip()
        if not os.path.exists(source_zshrc):
            print_color("❌ 源文件不存在，退出程序", "red")
            return False
    
    # 检查目标目录是否存在
    target_dir = os.path.dirname(target_zshrc)
    if not os.path.exists(target_dir):
        print_color(f"目标目录不存在，正在创建: {target_dir}", "yellow")
        try:
            os.makedirs(target_dir, exist_ok=True)
            print_color("✓ 目录创建成功", "green")
        except Exception as e:
            print_color(f"✗ 目录创建失败: {e}", "red")
            return False
    
    # 检查文件权限
    print("\n检查文件权限...")
    try:
        source_stat = os.stat(source_zshrc)
        print(f"源文件权限: {oct(source_stat.st_mode)[-3:]}")
        
        # 检查是否为Termux用户（uid通常为10000+）
        if source_stat.st_uid > 10000:
            print_color("⚠️  注意：源文件可能属于非Termux用户", "yellow")
    except Exception as e:
        print_color(f"✗ 权限检查失败: {e}", "red")
    
    # 备份现有.zshrc
    if os.path.exists(target_zshrc):
        print("\n检测到现有.zshrc文件")
        print_color("当前.zshrc文件内容预览:", "cyan")
        try:
            with open(target_zshrc, 'r', encoding='utf-8') as f:
                lines = f.readlines()[:10]  # 显示前10行
                for line in lines:
                    print(f"  {line.rstrip()}")
                if len(lines) >= 10:
                    print("  ... (更多内容未显示)")
        except:
            print("  无法读取文件内容")
        
        choice = input("\n是否备份现有.zshrc文件? (Y/n): ").lower()
        if choice != 'n':
            backup_file(target_zshrc)
    
    # 显示源文件内容预览
    print("\n" + "=" * 60)
    print_color("源文件内容预览:", "cyan")
    try:
        with open(source_zshrc, 'r', encoding='utf-8') as f:
            content = f.read()
            # 显示前20行 - 修复了f-string错误
            lines = content.split('\n')[:20]
            for i, line in enumerate(lines, 1):
                print(f"{i:3}: {line}")
            if len(content.split('\n')) > 20:
                remaining_lines = len(content.split('\n')) - 20
                print(f"  ... 还有 {remaining_lines} 行")
    except Exception as e:
        print_color(f"✗ 无法读取源文件: {e}", "red")
        return False
    
    print("\n" + "=" * 60)
    print_color("执行操作:", "purple")
    print(f"1. 复制: {source_zshrc}")
    print(f"   → {target_zshrc}")
    print("2. 设置文件权限 (644)")
    print("3. 验证文件完整性")
    
    confirm = input("\n是否继续? (Y/n): ").lower()
    if confirm == 'n':
        print_color("操作取消", "yellow")
        return False
    
    # 执行复制
    try:
        print("\n开始复制文件...")
        shutil.copy2(source_zshrc, target_zshrc)
        print_color("✓ 文件复制成功", "green")
        
        # 设置权限为644 (rw-r--r--)
        os.chmod(target_zshrc, stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP | stat.S_IROTH)
        print_color("✓ 文件权限设置成功", "green")
        
        # 验证文件大小
        source_size = os.path.getsize(source_zshrc)
        target_size = os.path.getsize(target_zshrc)
        print(f"源文件大小: {source_size} 字节")
        print(f"目标文件大小: {target_size} 字节")
        
        if source_size == target_size:
            print_color("✓ 文件大小验证通过", "green")
        else:
            print_color("⚠️  警告：文件大小不一致", "yellow")
        
        # 验证文件内容
        print("\n验证文件内容...")
        try:
            with open(source_zshrc, 'r', encoding='utf-8') as src, \
                 open(target_zshrc, 'r', encoding='utf-8') as tgt:
                src_content = src.read()
                tgt_content = tgt.read()
                
                if src_content == tgt_content:
                    print_color("✓ 文件内容验证通过", "green")
                else:
                    # 检查差异
                    print_color("⚠️  警告：文件内容不完全一致", "yellow")
                    src_lines = src_content.split('\n')
                    tgt_lines = tgt_content.split('\n')
                    
                    if len(src_lines) != len(tgt_lines):
                        print(f"行数差异: 源文件 {len(src_lines)} 行, 目标文件 {len(tgt_lines)} 行")
                    
                    # 显示前5处差异
                    differences = []
                    for i, (s_line, t_line) in enumerate(zip(src_lines, tgt_lines)):
                        if s_line != t_line:
                            differences.append((i+1, s_line, t_line))
                            if len(differences) >= 5:
                                break
                    
                    if differences:
                        print("前5处差异:")
                        for line_num, s_line, t_line in differences:
                            print(f"  第{line_num}行:")
                            # 修复f-string中的反斜杠问题
                            s_preview = s_line[:80] + ('...' if len(s_line) > 80 else '')
                            t_preview = t_line[:80] + ('...' if len(t_line) > 80 else '')
                            print(f"    源: {s_preview}")
                            print(f"    目标: {t_preview}")
        except Exception as e:
            print_color(f"✗ 内容验证失败: {e}", "red")
        
        # 显示成功信息
        print("\n" + "=" * 60)
        print_color("✅ 配置文件复制完成!", "green")
        print(f"\n文件已复制到: {target_zshrc}")
        print(f"文件权限: {oct(os.stat(target_zshrc).st_mode)[-3:]}")
        
        # 显示下一步操作建议
        print("\n下一步操作:")
        print("1. 重新加载配置:")
        print("   source ~/.zshrc")
        print("2. 重启Termux完全应用配置")
        print("3. 检查配置是否生效:")
        print("   echo $SHELL")
        print("   zsh --version")
        
        # 询问是否立即重新加载
        reload_choice = input("\n是否立即重新加载配置? (y/N): ").lower()
        if reload_choice == 'y':
            print("\n重新加载配置...")
            os.system("source ~/.zshrc")
            print_color("✓ 配置重新加载完成", "green")
        
        return True
        
    except Exception as e:
        print_color(f"✗ 复制失败: {e}", "red")
        return False

def create_auto_reload_script():
    """创建自动重载脚本"""
    script_content = '''#!/data/data/com.termux/files/usr/bin/bash
# 自动重载ZSH配置脚本

echo "重载ZSH配置..."
echo ""

# 检查.zshrc是否存在
if [ ! -f ~/.zshrc ]; then
    echo "错误: ~/.zshrc 不存在"
    exit 1
fi

# 备份当前配置
BACKUP_FILE=~/.zshrc.backup.$(date +%Y%m%d_%H%M%S)
cp ~/.zshrc "$BACKUP_FILE"
echo "已创建备份: $BACKUP_FILE"

# 重载配置
source ~/.zshrc

# 检查是否成功
if [ $? -eq 0 ]; then
    echo "✅ 配置重载成功"
    echo ""
    echo "当前Shell: $SHELL"
    echo "ZSH版本: $(zsh --version 2>/dev/null | head -1)"
else
    echo "❌ 配置重载失败"
    echo "请检查 ~/.zshrc 文件语法"
fi

# 显示最后几行配置（用于调试）
echo ""
echo "配置文件尾部内容:"
tail -10 ~/.zshrc
'''

    script_path = "/data/data/com.termux/files/usr/bin/reload-zsh"
    try:
        with open(script_path, 'w', encoding='utf-8') as f:
            f.write(script_content)
        os.chmod(script_path, 0o755)
        print_color(f"✓ 已创建自动重载脚本: {script_path}", "green")
        print("使用命令 'reload-zsh' 重载配置")
    except Exception as e:
        print_color(f"✗ 创建重载脚本失败: {e}", "red")

def main():
    """主函数"""
    print_color("\n" + "=" * 60, "cyan")
    print_color("Termux ZSH配置复制工具", "cyan")
    print_color("=" * 60, "cyan")
    
    # 检查是否在Termux环境中
    if "com.termux" not in os.getcwd():
        print_color("⚠️  警告：可能不在Termux环境中", "yellow")
        print(f"当前目录: {os.getcwd()}")
        
        confirm = input("\n是否继续? (y/N): ").lower()
        if confirm != 'y':
            return
    
    # 执行复制
    if copy_zshrc_with_permissions():
        # 询问是否创建自动重载脚本
        choice = input("\n是否创建自动重载脚本? (Y/n): ").lower()
        if choice != 'n':
            create_auto_reload_script()
        
        print("\n" + "=" * 60)
        print_color("✅ 所有操作完成!", "green")
        print("\n建议:")
        print("1. 重启Termux应用所有更改")
        print("2. 或输入 'zsh' 启动新配置")
        print("3. 检查Windows风格界面是否正常显示")
    else:
        print_color("❌ 操作失败，请检查错误信息", "red")

if __name__ == "__main__":
    if is_termux():
        print("Running inside Termux")
        main()
    else:
        print("Not a Termux environment")