#!/data/data/com.termux/files/usr/bin/bash

# 定义路径
target_file="/data/data/com.termux/files/home/.zinit"
source_dir="/storage/emulated/0/MITS/TEMP/backup/"
dest_dir="/data/data/com.termux/"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
PURPLE='\033[0;35m'
NC='\033[0m' # 无颜色

# 检查脚本执行权限
check_execution_permission() {
    if [[ ! -x "$0" ]]; then
        echo -e "${RED}错误：脚本没有执行权限${NC}"
        echo -e "${YELLOW}请运行以下命令：${NC}"
        echo -e "${BLUE}chmod +x \"$0\"${NC}"
        exit 1
    fi
}

# 检查存储权限
check_storage_permission() {
    if [[ ! -d "/storage/emulated/0" ]]; then
        echo -e "${YELLOW}正在请求存储权限...${NC}"
        termux-setup-storage
        sleep 3
    fi
}

# 创建临时目录
create_temp_dirs() {
    mkdir -p "/storage/emulated/0/MITS/TEMP/list" 2>/dev/null
    mkdir -p "$dest_dir" 2>/dev/null
}

# 进度条函数（简化版，更快）
show_progress_bar() {
    local current=$1
    local total=$2
    local percent=$((current * 100 / total))
    
    # 每处理100个文件才更新一次显示，减少开销
    if [[ $((current % 100)) -eq 0 ]] || [[ $current -eq $total ]]; then
        printf "\r${CYAN}处理中: %3d%% (%d/%d)${NC}" "$percent" "$current" "$total"
    fi
}

# 快速文件比较函数（使用大小和时间戳，比MD5快）
should_copy_file_fast() {
    local src_file=$1
    local dst_file=$2
    
    # 如果目标文件不存在，需要复制
    if [[ ! -f "$dst_file" ]]; then
        echo "new"
        return 0
    fi
    
    # 获取文件大小
    local src_size=$(stat -c%s "$src_file" 2>/dev/null || ls -l "$src_file" | awk '{print $5}')
    local dst_size=$(stat -c%s "$dst_file" 2>/dev/null || ls -l "$dst_file" | awk '{print $5}')
    
    # 如果大小不同，需要复制
    if [[ "$src_size" != "$dst_size" ]]; then
        echo "size_diff"
        return 0
    fi
    
    # 获取修改时间（只取到秒，避免毫秒差异）
    local src_mtime=$(stat -c%Y "$src_file" 2>/dev/null || date -r "$src_file" +%s 2>/dev/null)
    local dst_mtime=$(stat -c%Y "$dst_file" 2>/dev/null || date -r "$dst_file" +%s 2>/dev/null)
    
    # 如果源文件更新，需要复制
    if [[ "$src_mtime" -gt "$dst_mtime" ]]; then
        echo "newer"
        return 0
    fi
    
    # 文件相同，不需要复制
    echo "same"
    return 1
}

# 批量复制文件（更高效的方法）
batch_copy_files() {
    local src=$1
    local dst=$2
    
    echo -e "${CYAN}正在快速扫描文件...${NC}"
    
    # 使用find快速获取文件列表
    echo -e "${YELLOW}正在生成文件列表...${NC}"
    find "$src" -type f > /storage/emulated/0/MITS/TEMP/list/src_files.list 2>/dev/null
    local total_files=$(wc -l < /storage/emulated/0/MITS/TEMP/list/src_files.list)
    
    if [[ $total_files -eq 0 ]]; then
        echo -e "${YELLOW}没有找到需要复制的文件${NC}"
        return 0
    fi
    
    echo -e "${GREEN}发现 ${total_files} 个文件${NC}"
    echo -e "${CYAN}开始批量处理...${NC}"
    
    # 清空之前的进度条
    echo ""
    
    local processed=0
    local new_count=0
    local updated_count=0
    
    # 使用while循环处理文件，避免内存问题
    while IFS= read -r src_file; do
        # 计算相对路径
        local rel_path="${src_file#$src}"
        local dst_file="$dst$rel_path"
        
        # 检查是否需要复制
        copy_status=$(should_copy_file_fast "$src_file" "$dst_file")
        
        if [[ "$copy_status" == "new" ]]; then
            # 创建目标目录
            mkdir -p "$(dirname "$dst_file")" 2>/dev/null
            # 复制文件（使用简单的cp）
            if cp "$src_file" "$dst_file" 2>/dev/null; then
                new_count=$((new_count + 1))
                # 设置合理的权限，不要使用 777
                chmod 755 "$dst_file" 2>/dev/null || true
            fi
        elif [[ "$copy_status" == "size_diff" ]] || [[ "$copy_status" == "newer" ]]; then
            # 创建目标目录
            mkdir -p "$(dirname "$dst_file")" 2>/dev/null
            # 复制文件
            if cp "$src_file" "$dst_file" 2>/dev/null; then
                updated_count=$((updated_count + 1))
                # 设置合理的权限，不要使用 777
                chmod 755 "$dst_file" 2>/dev/null || true
            fi
        fi
        
        processed=$((processed + 1))
        
        # 显示进度（减少更新频率，提高性能）
        if [[ $((processed % 50)) -eq 0 ]] || [[ $processed -eq $total_files ]]; then
            show_progress_bar "$processed" "$total_files"
        fi
    done < /storage/emulated/0/MITS/TEMP/list/src_files.list
    
    echo "" # 换行
    
    # 清理临时文件
    rm -f /storage/emulated/0/MITS/TEMP/list/src_files.list 2>/dev/null
    
    # 显示统计
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${GREEN}批量复制完成：${NC}"
    echo -e "  新增文件: ${GREEN}$new_count 个${NC}"
    echo -e "  更新文件: ${YELLOW}$updated_count 个${NC}"
    echo -e "  跳过文件: ${BLUE}$((total_files - new_count - updated_count)) 个${NC}"
    echo -e "  总文件数: $total_files 个${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    
    return 0
}

# 使用rsync进行高效复制（如果可用）
use_rsync_if_available() {
    local src=$1
    local dst=$2
    
    # 检查rsync是否可用
    if command -v rsync >/dev/null 2>&1; then
        echo -e "${GREEN}检测到rsync，使用高效复制模式...${NC}"
        
        # 计算需要复制的文件数量（预估）
        echo -e "${YELLOW}正在计算需要复制的文件...${NC}"
        
        # 使用rsync的-n（dry-run）选项来查看将要复制的文件
        local changes=$(rsync -avn "$src" "$dst" 2>/dev/null | grep -E "^[^.]" | wc -l)
        
        if [[ $changes -eq 0 ]]; then
            echo -e "${YELLOW}所有文件都是最新的，无需复制${NC}"
            return 0
        fi
        
        echo -e "${CYAN}发现大约 $changes 个文件需要更新${NC}"
        echo -e "${GREEN}开始使用rsync复制...${NC}"
        
        # 实际复制
        rsync -av --progress "$src" "$dst"
        
        if [[ $? -eq 0 ]]; then
            echo -e "${GREEN} rsync复制成功${NC}"
            # 设置合理的权限（注意：不要对整个目录设置777）
            find "$dst" -type f -exec chmod 755 {} \; 2>/dev/null || true
            return 0
        else
            echo -e "${RED} rsync复制失败，回退到标准复制${NC}"
            return 1
        fi
    else
        echo -e "${YELLOW}未找到rsync，使用标准复制模式${NC}"
        return 1
    fi
}

# 设置关键文件的权限
set_critical_file_permissions() {
    echo -e "${YELLOW}设置关键文件权限...${NC}"
    
    local critical_files=(
        "/data/data/com.termux/files/home/.zshrc"
        "/data/data/com.termux/files/home/.zinit"
        "/data/data/com.termux/files/home/.termux/shell"
    )
    
    for file in "${critical_files[@]}"; do
        if [[ -f "$file" ]]; then
            chmod 755 "$file" 2>/dev/null && echo -e "${GREEN}[✓] 设置权限: $(basename "$file")${NC}" || true
        fi
    done
}

# 主流程
main() {
    # 检查执行权限
    check_execution_permission
    
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${GREEN}智能文件复制系统（优化版）${NC}"
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo ""
    
    # 检查存储权限
    check_storage_permission
    
    # 创建临时目录
    create_temp_dirs
    
    # 1. 检查目标文件是否存在
    if [[ -e "$target_file" ]]; then
        echo -e "${GREEN}[SYSTEM] 检测到 $target_file 已存在${NC}"
    else
        echo -e "${YELLOW}[SYSTEN] 未检测到 .zinit，将进行初始化${NC}"
    fi
    
    # 2. 检查源目录
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${YELLOW}检查源目录...${NC}"
    
    if [[ ! -d "$source_dir" ]]; then
        echo -e "${RED}[SYSTEM] 错误：源目录 $source_dir 不存在${NC}"
        echo -e "${YELLOW}[SYSTEM] 重新安装中..."
        exit 1
    fi
    
    # 3. 检查源目录是否为空
    if [[ -z "$(ls -A "$source_dir" 2>/dev/null)" ]]; then
        echo -e "${YELLOW}[SYSTEM] 源目录为空，跳过复制${NC}"
        echo -e "${YELLOW}[SYSTEM] 重新安装中..."
        exit 0
    fi
    
    # 4. 统计文件信息
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${YELLOW}快速统计文件...${NC}"
    
    # 快速统计
    local file_count=$(find "$source_dir" -type f | wc -l)
    local dir_count=$(find "$source_dir" -type d | wc -l)
    
    echo -e "${GREEN}[SYSTEM] 发现大量文件：${NC}"
    echo -e "  文件数: $file_count"
    echo -e "  目录数: $((dir_count - 1))"
    
    # 5. 先创建必要的目录结构
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${YELLOW}创建目录结构...${NC}"
    
    # 只创建不存在的目录
    find "$source_dir" -type d -exec mkdir -p "$dest_dir{}" \; 2>/dev/null | wc -l > /storage/emulated/0/MITS/TEMP/list/dirs_created.tmp
    local dirs_created=$(cat /storage/emulated/0/MITS/TEMP/list/dirs_created.tmp 2>/dev/null || echo "0")
    rm -f /storage/emulated/0/MITS/TEMP/list/dirs_created.tmp 2>/dev/null
    
    echo -e "${GREEN}目录结构准备完成${NC}"
    
    # 6. 使用更高效的复制方法
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${GREEN}开始文件复制...${NC}"
    
    # 尝试使用rsync，如果不可用则使用批量复制
    if use_rsync_if_available "$source_dir" "$dest_dir"; then
        echo -e "${GREEN}使用rsync完成复制${NC}"
    else
        echo -e "${YELLOW}使用批量复制方法...${NC}"
        batch_copy_files "$source_dir" "$dest_dir"
    fi
    
    # 7. 设置关键文件权限
    set_critical_file_permissions
    
    # 8. 验证关键文件
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    echo -e "${YELLOW}验证关键文件...${NC}"
    
    local critical_files=(
        "/data/data/com.termux/files/home/.zinit"
        "/data/data/com.termux/files/home/.zshrc"
        "/data/data/com.termux/files/home/.termux/shell"
    )
    
    local missing_count=0
    
    for file in "${critical_files[@]}"; do
        if [[ -e "$file" ]]; then
            echo -e "${GREEN}[SYSTEM] $(basename "$file") 存在${NC}"
            
            # 如果是shell脚本，设置执行权限
            if [[ "$file" == *"/.termux/shell" ]] && [[ -f "$file" ]]; then
                chmod +x "$file" 2>/dev/null
                echo -e "${BLUE}[→] 已设置执行权限${NC}"
            fi
        else
            echo -e "${YELLOW}[!] $(basename "$file") 不存在${NC}"
            missing_count=$((missing_count + 1))
        fi
    done
    
    # 9. 总结
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    if [[ $missing_count -eq 0 ]]; then
        echo -e "${GREEN}[SYSTEM] 文件复制完成！${NC}"
        
        # 设置zsh为默认shell
        if [[ -f "/data/data/com.termux/files/home/.zshrc" ]]; then
            chsh -s zsh > /dev/null 2>&1
            echo -e "${GREEN}[SYSTEN] 已设置zsh为默认shell${NC}"
            echo -e "${YELLOW}[SYSTEM] 建议重新启动Termux以应用更改${NC}"
        fi
    else
        echo -e "${YELLOW}[SYSTEM] 复制完成，但缺少 $missing_count 个关键文件${NC}"
        echo -e "${YELLOW}[SYSTEM] 重新安装中..."
    fi
    
    echo -e "${CYAN}════════════════════════════════════════${NC}"
    
    # 显示最终统计
    echo -e "${BLUE}处理摘要：${NC}"
    echo -e "  总文件数: $file_count"
    echo -e "  总目录数: $((dir_count - 1))"
    echo -e "  新目录创建: $dirs_created"
    echo -e "  处理时间: $(date '+%Y-%m-%d %H:%M:%S')"
    echo -e "  脚本权限: $(ls -l "$0" | awk '{print $1}')"
    
    echo -e "${CYAN}════════════════════════════════════════${NC}"
}

# 运行主函数
main