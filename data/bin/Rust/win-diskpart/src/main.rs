use std::fs;
use std::io::{self, BufRead, Write};
use std::path::Path;
use std::process::Command;
use serde::Deserialize;

// ==================== 配置 ====================

const CONFIG_PATH: &str = "/data/data/com.termux/files/usr/etc/MITS/diskpart/config.json";
const BLOCK_DEV_PATH: &str = "/dev/block";

#[derive(Deserialize, Default)]
struct Config {
    #[serde(rename = "MITS_Diskpart_host")]
    host: Option<String>,
    #[serde(rename = "MITS_Diskpart_copyright")]
    copyright: Option<String>,
    #[serde(rename = "MITS_Diskpart_version")]
    version: Option<String>,
}

// ==================== 数据结构 ====================

#[derive(Debug, Clone)]
struct DiskInfo {
    name: String,
    size_bytes: u64,
    removable: bool,
    model: String,
    serial: String,
    pt_type: String, // "gpt" / "mbr" / "unknown"
}

#[derive(Debug, Clone)]
struct PartInfo {
    name: String,
    start_sector: u64,
    size_sectors: u64, // 扇区数，每扇区 512B
    fs: String,
    uuid: String,
    part_type: String,
}

// ==================== 工具函数 ====================

fn trim(s: &str) -> &str {
    s.trim()
}

/// 执行命令并捕获 stdout 输出。
fn exec_cmd(cmd: &str, args: &[&str]) -> String {
    match Command::new(cmd).args(args).output() {
        Ok(output) => String::from_utf8_lossy(&output.stdout).to_string(),
        Err(_) => String::new(),
    }
}

/// 执行命令并将输出直接打印到终端（用于 parted/wipefs 等需要交互的操作）。
fn exec_cmd_inherit(cmd: &str, args: &[&str]) -> bool {
    Command::new(cmd)
        .args(args)
        .status()
        .map(|s| s.success())
        .unwrap_or(false)
}

fn is_root() -> bool {
    // 在 Termux 中，可以通过检查 id -u 是否为 0 判断
    exec_cmd("id", &["-u"]).trim() == "0"
}

/// 将字节数格式化为人类可读的字符串（DiskPart 风格）。
fn format_size(bytes: u64) -> String {
    if bytes < 1024 {
        format!("{} B", bytes)
    } else if bytes < 1024 * 1024 {
        format!("{} KB", bytes / 1024)
    } else if bytes < 1024 * 1024 * 1024 {
        format!("{} MB", bytes / (1024 * 1024))
    } else {
        format!("{} GB", bytes / (1024 * 1024 * 1024))
    }
}

// ==================== 磁盘信息获取 ====================

/// 解析 /proc/partitions，返回所有块设备条目。
fn read_proc_partitions() -> Vec<(String, u64)> {
    let mut entries = Vec::new();
    if let Ok(content) = fs::read_to_string("/proc/partitions") {
        for line in content.lines().skip(2) {
            let parts: Vec<&str> = line.split_whitespace().collect();
            if parts.len() >= 4 {
                let name = parts[3].to_string();
                if let Ok(blocks) = parts[2].parse::<u64>() {
                    entries.push((name, blocks));
                }
            }
        }
    }
    entries
}

/// 获取所有物理磁盘（过滤 loop/ram/zram/dm）。
fn get_disks() -> Vec<DiskInfo> {
    let all_entries = read_proc_partitions();
    let mut disks = Vec::new();

    for (name, blocks) in &all_entries {
        if name.starts_with("loop")
            || name.starts_with("ram")
            || name.starts_with("zram")
            || name.starts_with("dm-")
        {
            continue;
        }
        // 跳过分区条目（分区名 = 磁盘名 + 数字或 p数字），通过 /sys/block 是否存在判断
        let sys_path = format!("/sys/block/{}", name);
        if !Path::new(&sys_path).exists() {
            continue;
        }

        let size_bytes = blocks * 1024;

        // 可移除标志
        let removable = fs::read_to_string(format!("{}/removable", sys_path))
            .map(|s| s.trim() == "1")
            .unwrap_or(false);

        // 型号
        let model = fs::read_to_string(format!("{}/device/model", sys_path))
            .map(|s| trim(&s).to_string())
            .unwrap_or_else(|_| "Unknown".to_string());

        // 序列号
        let serial = fs::read_to_string(format!("{}/device/serial", sys_path))
            .map(|s| trim(&s).to_string())
            .unwrap_or_else(|_| "Unknown".to_string());

        // 分区表类型
        let wipefs_out = exec_cmd("wipefs", &["--noheadings", &format!("{}/{}", BLOCK_DEV_PATH, name)]);
        let pt_type = if wipefs_out.contains("gpt") {
            "gpt".to_string()
        } else if wipefs_out.contains("dos") {
            "mbr".to_string()
        } else {
            "unknown".to_string()
        };

        disks.push(DiskInfo {
            name: name.clone(),
            size_bytes,
            removable,
            model,
            serial,
            pt_type,
        });
    }
    disks
}

/// 获取指定磁盘上的所有分区。
fn get_partitions(disk_name: &str) -> Vec<PartInfo> {
    let all_entries = read_proc_partitions();
    let mut parts = Vec::new();

    for (name, blocks) in &all_entries {
        if name == disk_name || !name.starts_with(disk_name) {
            continue;
        }
        // 确保是真正的分区（磁盘名后面跟数字或 p数字）
        let suffix = &name[disk_name.len()..];
        if !suffix.starts_with('p') && !suffix.chars().all(|c| c.is_ascii_digit()) {
            continue;
        }

        let size_sectors = blocks * 2; // blocks 是 1024B 单位，扇区 512B

        // 起始扇区
        let start_sector = fs::read_to_string(format!(
            "/sys/block/{}/{}/start",
            disk_name, name
        ))
        .ok()
        .and_then(|s| s.trim().parse::<u64>().ok())
        .unwrap_or(0);

        // 文件系统、UUID、分区类型
        let blkid_out = exec_cmd(
            "blkid",
            &["-o", "export", &format!("{}/{}", BLOCK_DEV_PATH, name)],
        );
        let mut fs_type = "unknown".to_string();
        let mut uuid = String::new();
        let mut part_type = String::new();

        for line in blkid_out.lines() {
            if let Some(v) = line.strip_prefix("TYPE=") {
                fs_type = v.to_string();
            } else if let Some(v) = line.strip_prefix("UUID=") {
                uuid = v.to_string();
            } else if let Some(v) = line.strip_prefix("PART_ENTRY_TYPE=") {
                part_type = v.to_string();
            }
        }

        parts.push(PartInfo {
            name: name.clone(),
            start_sector,
            size_sectors,
            fs: fs_type,
            uuid,
            part_type,
        });
    }
    parts
}

// ==================== 显示函数 ====================

fn list_disks() {
    let disks = get_disks();
    println!();
    println!("  磁盘 ###  状态          大小     可用     Dyn  Gpt");
    println!("  --------  ------------  -------  -------  ---  ---");
    for (i, d) in disks.iter().enumerate() {
        let status = if d.removable { "可移动" } else { "联机  " };
        let size = format_size(d.size_bytes);
        let gpt = if d.pt_type == "gpt" { "*" } else { "" };
        println!(
            "  磁盘 {:<4}  {:<12}  {:<7}  {:<7}       {}",
            i, status, size, "0 B", gpt
        );
    }
    println!();
}

fn detail_disk(disk_name: &str) {
    let disks = get_disks();
    let d = match disks.iter().find(|d| d.name == disk_name) {
        Some(d) => d,
        None => {
            println!("未找到磁盘信息。");
            return;
        }
    };
    println!();
    println!("{}", d.model);
    println!("磁盘 ID: {}", d.serial);
    println!("类型   : {}", if d.removable { "可移动" } else { "固定" });
    println!("状态   : 联机");
    println!("路径   : 0");
    println!("目标   : 0");
    println!("LUN ID : 0");
    println!("位置路径 : /dev/block/{}", d.name);
    println!("当前只读状态 : 否");
    println!("只读   : 否");
    println!("启动磁盘 : 否");
    println!("页面文件磁盘 : 否");
    println!("休眠文件磁盘 : 否");
    println!("故障转储磁盘 : 否");
    println!("群集磁盘 : 否");
    println!();
    println!("卷 ###  Ltr  标签          Fs     类型        大小     状态      信息");
    println!("------- ---  ------------  ----  ---------  -------  --------- ---------");
    let parts = get_partitions(disk_name);
    for (i, p) in parts.iter().enumerate() {
        let size_mb = (p.size_sectors * 512) / (1024 * 1024);
        println!(
            "  卷 {:<3}  {:<4} {:<12} {:<5} {:<10} {:<7} {:<9} {}",
            i, "", "", p.fs, "分区", format!("{} MB", size_mb), "正常", ""
        );
    }
    println!();
}

fn list_partitions(disk_name: &str) {
    if disk_name.is_empty() {
        println!("没有选中磁盘。请先使用 select disk <n>。");
        return;
    }
    let parts = get_partitions(disk_name);
    if parts.is_empty() {
        println!("该磁盘没有分区。");
        return;
    }
    println!();
    println!("  分区 ###  类型              大小     偏移");
    println!("  ---------  ----------------  -------  -------");
    for (i, p) in parts.iter().enumerate() {
        let size_mb = (p.size_sectors * 512) / (1024 * 1024);
        let offset_mb = (p.start_sector * 512) / (1024 * 1024);
        println!(
            "  分区 {:<4}  {:<16}  {:<7}  {}",
            i + 1, p.fs, format!("{} MB", size_mb), format!("{} MB", offset_mb)
        );
    }
    println!();
}

fn detail_partition(p: &PartInfo) {
    println!();
    println!("分区 {}", p.name);
    println!("类型   : {}", if p.part_type.is_empty() { "未知" } else { &p.part_type });
    println!("隐藏   : 否");
    println!("活动   : 否");
    println!("偏移   : {} 扇区 ({} MB)", p.start_sector, (p.start_sector * 512) / (1024 * 1024));
    println!("大小   : {} MB", (p.size_sectors * 512) / (1024 * 1024));
    println!("文件系统 : {}", p.fs);
    println!("UUID   : {}", if p.uuid.is_empty() { "无" } else { &p.uuid });
    println!();
}

// ==================== 操作函数 ====================

fn create_partition(disk_name: &str, size_mb: u64) -> bool {
    if !is_root() {
        eprintln!("错误：创建分区需要 root 权限。");
        return false;
    }
    println!("将使用 parted 创建分区（需要 parted 已安装）。");
    let dev = format!("{}/{}", BLOCK_DEV_PATH, disk_name);
    let size_arg = format!("{}MB", size_mb);
    let args = vec!["-s", &dev, "mkpart", "primary", &size_arg];
    println!("执行: parted {}", args.join(" "));
    let ok = exec_cmd_inherit("parted", &args);
    if ok {
        println!("分区创建成功。");
        // 通知内核重读分区表
        let _ = exec_cmd_inherit("partprobe", &[&dev]);
        true
    } else {
        eprintln!("分区创建失败。");
        false
    }
}

fn delete_partition(disk_name: &str, part_name: &str) -> bool {
    if !is_root() {
        eprintln!("错误：删除分区需要 root 权限。");
        return false;
    }
    // 提取分区号
    let suffix = if part_name.len() > disk_name.len() {
        &part_name[disk_name.len()..]
    } else {
        ""
    };
    let part_num: String = suffix.chars().filter(|c| c.is_ascii_digit()).collect();
    if part_num.is_empty() {
        eprintln!("无法解析分区号。");
        return false;
    }

    print!("确认删除分区 /dev/block/{} ? (输入 yes 继续): ", part_name);
    io::stdout().flush().ok();
    let mut confirm = String::new();
    io::stdin().read_line(&mut confirm).ok();
    if trim(&confirm) != "yes" {
        println!("已取消。");
        return false;
    }

    let dev = format!("{}/{}", BLOCK_DEV_PATH, disk_name);
    let args = vec!["-s", &dev, "rm", &part_num];
    println!("执行: parted {}", args.join(" "));
    let ok = exec_cmd_inherit("parted", &args);
    if ok {
        println!("分区已删除。");
        true
    } else {
        eprintln!("删除失败。");
        false
    }
}

fn clean_disk(disk_name: &str) -> bool {
    if !is_root() {
        eprintln!("错误：清除磁盘需要 root 权限。");
        return false;
    }
    print!(
        "这将清除磁盘 /dev/block/{} 上的所有分区表。\n确认？(输入 yes 继续): ",
        disk_name
    );
    io::stdout().flush().ok();
    let mut confirm = String::new();
    io::stdin().read_line(&mut confirm).ok();
    if trim(&confirm) != "yes" {
        println!("已取消。");
        return false;
    }

    let dev = format!("{}/{}", BLOCK_DEV_PATH, disk_name);
    let ok = exec_cmd_inherit("wipefs", &["--all", &dev]);
    if ok {
        println!("分区表已清除。");
        true
    } else {
        eprintln!("清除失败。");
        false
    }
}

fn format_partition(part_dev: &str, fs_type: &str, quick: bool) -> bool {
    if !is_root() {
        eprintln!("错误：格式化需要 root 权限。");
        return false;
    }
    let (cmd, args): (&str, Vec<&str>) = match fs_type {
        "fat" | "vfat" => ("mkfs.vfat", vec![part_dev]),
        "ext4" => ("mkfs.ext4", vec![part_dev]),
        "ntfs" => ("mkfs.ntfs", vec![part_dev]),
        "exfat" => ("mkfs.exfat", vec![part_dev]),
        "f2fs" => ("mkfs.f2fs", vec![part_dev]),
        _ => {
            eprintln!("不支持的文件系统类型：{}", fs_type);
            return false;
        }
    };

    let mut final_args = args.clone();
    if !quick && fs_type == "ext4" {
        final_args.push("-c");
    }

    println!("执行: {} {}", cmd, final_args.join(" "));
    let ok = exec_cmd_inherit(cmd, &final_args);
    if ok {
        println!("格式化成功。");
        true
    } else {
        eprintln!("格式化失败。");
        false
    }
}

fn mount_partition(part_dev: &str, mount_point: &str) -> bool {
    if !is_root() {
        eprintln!("错误：挂载需要 root 权限。");
        return false;
    }
    if let Err(e) = fs::create_dir_all(mount_point) {
        eprintln!("无法创建挂载点 {}: {}", mount_point, e);
        return false;
    }
    let ok = exec_cmd_inherit("mount", &[part_dev, mount_point]);
    if ok {
        println!("分区已挂载至 {}", mount_point);
        true
    } else {
        eprintln!("挂载失败。");
        false
    }
}

fn unmount_partition(part_dev: &str) -> bool {
    if !is_root() {
        eprintln!("错误：卸载需要 root 权限。");
        return false;
    }
    let ok = exec_cmd_inherit("umount", &[part_dev]);
    if ok {
        println!("分区已卸载。");
        true
    } else {
        eprintln!("卸载失败。");
        false
    }
}

// ==================== 脚本执行 ====================

/// 逐行读取脚本文件并执行其中的 DiskPart 命令。
fn run_script(
    path: &str,
    current_disk: &mut String,
    current_part: &mut String,
) {
    let file = match fs::File::open(path) {
        Ok(f) => f,
        Err(e) => {
            eprintln!("无法打开脚本文件 '{}': {}", path, e);
            return;
        }
    };
    let reader = io::BufReader::new(file);

    for line in reader.lines() {
        let line = match line {
            Ok(l) => l,
            Err(_) => continue,
        };
        let line = trim(&line);
        if line.is_empty() || line.starts_with('#') || line.starts_with("rem ") {
            continue;
        }
        println!("DISKPART> {}", line);
        // 递归执行命令（调用主循环中的命令处理逻辑）
        execute_command(line, current_disk, current_part, false);
    }
}

// ==================== 命令解析与执行 ====================

/// 解析并执行一条 DiskPart 命令。
/// `interactive` 为 true 时输出帮助信息等额外提示。
fn execute_command(
    line: &str,
    current_disk: &mut String,
    current_part: &mut String,
    interactive: bool,
) {
    let tokens: Vec<&str> = line.split_whitespace().collect();
    if tokens.is_empty() {
        return;
    }
    let cmd = tokens[0].to_lowercase();

    match cmd.as_str() {
        "exit" | "quit" => {
            if interactive {
                std::process::exit(0);
            }
        }
        "help" | "?" => {
            print_help();
        }
        "list" if tokens.len() >= 2 => match tokens[1].to_lowercase().as_str() {
            "disk" => list_disks(),
            "partition" => list_partitions(current_disk),
            _ => println!("未知子命令。"),
        },
        "select" if tokens.len() >= 3 => {
            let sub = tokens[1].to_lowercase();
            let idx: usize = match tokens[2].parse() {
                Ok(i) => i,
                Err(_) => {
                    println!("无效的索引。");
                    return;
                }
            };
            match sub.as_str() {
                "disk" => {
                    let disks = get_disks();
                    if idx < disks.len() {
                        *current_disk = disks[idx].name.clone();
                        current_part.clear();
                        println!("磁盘 {} 现在是所选磁盘。", idx);
                    } else {
                        println!("索引超出范围。");
                    }
                }
                "partition" => {
                    if current_disk.is_empty() {
                        println!("请先选择磁盘。");
                        return;
                    }
                    let parts = get_partitions(current_disk);
                    if idx >= 1 && idx <= parts.len() {
                        *current_part = parts[idx - 1].name.clone();
                        println!("分区 {} 现在是所选分区。", idx);
                    } else {
                        println!("分区索引超出范围。");
                    }
                }
                _ => println!("未知子命令。"),
            }
        }
        "detail" => {
            if tokens.len() >= 2 {
                match tokens[1].to_lowercase().as_str() {
                    "disk" => {
                        if current_disk.is_empty() {
                            println!("没有选中磁盘。");
                        } else {
                            detail_disk(current_disk);
                        }
                    }
                    "partition" => {
                        if current_part.is_empty() {
                            println!("没有选中分区。");
                        } else {
                            let parts = get_partitions(current_disk);
                            if let Some(p) = parts.iter().find(|p| p.name == *current_part) {
                                detail_partition(p);
                            } else {
                                println!("未找到分区信息。");
                            }
                        }
                    }
                    _ => println!("用法: detail disk 或 detail partition"),
                }
            }
        }
        "create" if tokens.len() >= 3 && tokens[1].to_lowercase() == "partition"
            && tokens[2].to_lowercase() == "primary" =>
        {
            if current_disk.is_empty() {
                println!("没有选中磁盘。");
                return;
            }
            let size_mb = tokens.iter().find_map(|t| {
                t.strip_prefix("size=").and_then(|v| v.parse::<u64>().ok())
            });
            match size_mb {
                Some(s) => { create_partition(current_disk, s); }
                None => println!("必须指定 size=<MB>。"),
            }
        }
        "delete" if tokens.len() >= 2 && tokens[1].to_lowercase() == "partition" => {
            if current_part.is_empty() {
                println!("没有选中分区。");
                return;
            }
            delete_partition(current_disk, current_part);
            current_part.clear();
        }
        "clean" => {
            if current_disk.is_empty() {
                println!("没有选中磁盘。");
                return;
            }
            clean_disk(current_disk);
            current_part.clear();
        }
        "format" => {
            let mut fs_type = "vfat".to_string();
            let mut quick = false;
            let mut target_dev = if current_part.is_empty() {
                String::new()
            } else {
                format!("{}/{}", BLOCK_DEV_PATH, current_part)
            };
            for tok in &tokens {
                if let Some(v) = tok.strip_prefix("fs=") {
                    fs_type = v.to_string();
                } else if *tok == "quick" {
                    quick = true;
                } else if let Some(v) = tok.strip_prefix("dev=") {
                    target_dev = v.to_string();
                }
            }
            if target_dev.is_empty() {
                println!("没有指定目标分区。请先用 select partition 或指定 dev=/dev/block/xxx");
                return;
            }
            format_partition(&target_dev, &fs_type, quick);
        }
        "assign" => {
            if current_part.is_empty() {
                println!("没有选中分区。");
                return;
            }
            let mount_point = tokens
                .iter()
                .find_map(|t| t.strip_prefix("mount="))
                .unwrap_or("/mnt/diskpart");
            mount_partition(&format!("{}/{}", BLOCK_DEV_PATH, current_part), mount_point);
        }
        "remove" => {
            if current_part.is_empty() {
                println!("没有选中分区。");
                return;
            }
            unmount_partition(&format!("{}/{}", BLOCK_DEV_PATH, current_part));
        }
        "run" => {
            // 支持: run script.txt
            if tokens.len() >= 2 {
                run_script(tokens[1], current_disk, current_part);
            } else {
                println!("用法: run <脚本文件路径>");
            }
        }
        _ => {
            println!("未知命令。输入 help 查看帮助。");
        }
    }
}

fn print_help() {
    println!("支持的命令：");
    println!("  list disk                            - 列出磁盘");
    println!("  select disk <索引>                   - 选择磁盘");
    println!("  detail disk                          - 显示磁盘详细信息");
    println!("  list partition                       - 列出当前磁盘的分区");
    println!("  select partition <索引>              - 选择分区");
    println!("  detail partition                     - 显示当前分区详细信息");
    println!("  create partition primary size=<MB>   - 创建主分区（需 root）");
    println!("  delete partition                     - 删除选中的分区（需 root）");
    println!("  clean                                - 清除磁盘分区表（需 root）");
    println!("  format fs=<类型> [quick] [dev=<设备>] - 格式化（需 root，支持 vfat/ext4/ntfs/exfat/f2fs）");
    println!("  assign [mount=<路径>]                - 挂载当前分区（默认 /mnt/diskpart，需 root）");
    println!("  remove                               - 卸载当前分区（需 root）");
    println!("  run <脚本文件>                        - 执行脚本文件中的 DiskPart 命令");
    println!("  exit                                 - 退出");
}

// ==================== 主程序 ====================

fn main() {
    // 读取配置
    let config: Config = fs::read_to_string(CONFIG_PATH)
        .ok()
        .and_then(|s| serde_json::from_str(&s).ok())
        .unwrap_or_default();

    let host = config.host.as_deref().unwrap_or("ANDROID");
    let copyright = config
        .copyright
        .as_deref()
        .unwrap_or("(c) Microsoft Corporation. MITS Port");
    let version = config
        .version
        .as_deref()
        .unwrap_or("Microsoft DiskPart 版本 10.0.17763.1 (MITS)");

    println!("{}", version);
    println!("{}", copyright);
    println!("在计算机上: {}", host);
    println!();

    let mut current_disk = String::new();
    let mut current_part = String::new();

    // 命令行参数支持直接执行脚本
    let args: Vec<String> = std::env::args().collect();
    if args.len() > 1 {
        // 如果第一个参数是 /s 或 -s，则执行脚本
        let script_path = if args[1] == "/s" || args[1] == "-s" {
            args.get(2).map(|s| s.as_str())
        } else {
            Some(args[1].as_str())
        };
        if let Some(path) = script_path {
            run_script(path, &mut current_disk, &mut current_part);
            return;
        }
    }

    // 交互式主循环
    let stdin = io::stdin();
    loop {
        print!("DISKPART> ");
        io::stdout().flush().ok();

        let mut line = String::new();
        if stdin.lock().read_line(&mut line).is_err() {
            break;
        }
        let line = trim(&line);
        if line.is_empty() {
            continue;
        }

        let cmd_first = line.split_whitespace().next().unwrap_or("").to_lowercase();
        if cmd_first == "exit" || cmd_first == "quit" {
            break;
        }

        execute_command(line, &mut current_disk, &mut current_part, true);
    }
}