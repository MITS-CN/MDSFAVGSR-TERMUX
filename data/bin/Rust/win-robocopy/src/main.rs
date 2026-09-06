use std::collections::HashSet;
use std::fs;
use std::path::{Path, PathBuf};
use std::thread;
use std::time::Duration;

use anyhow::{anyhow, Context, Result};
use walkdir::WalkDir;

// ---------- 命令行参数结构 ----------
struct Config {
    source: PathBuf,
    dest: PathBuf,
    mirror: bool,        // /MIR
    recursive: bool,     // /E (copy subdirs, including empty)
    retry: u32,          // /R:n
    wait_secs: u64,      // /W:n
}

// ---------- 帮助信息 ----------
fn print_help() {
    println!(
        r#"Usage: robocopy-rs <source> <destination> [options]

Options:
  /E          Copy subdirectories, including empty ones.
  /MIR        Mirror a directory tree (equivalent to /E plus purging).
  /R:n        Number of retries on failed copies (default: 1).
  /W:n        Wait time in seconds between retries (default: 1).
  /?          Show this help.

Examples:
  robocopy-rs /sdcard/Photos /sdcard/Backup /E
  robocopy-rs /data/data/com.termux/files/home/project /sdcard/project_backup /MIR /R:3 /W:2
"#
    );
}

// ---------- 解析参数 ----------
fn parse_args() -> Result<Config> {
    let args: Vec<String> = std::env::args().collect();
    if args.len() < 3 {
        anyhow::bail!("Not enough arguments");
    }

    let mut source: Option<PathBuf> = None;
    let mut dest: Option<PathBuf> = None;
    let mut mirror = false;
    let mut recursive = false;
    let mut retry = 1;
    let mut wait_secs = 1;

    let mut args_iter = args.iter().skip(1);
    while let Some(arg) = args_iter.next() {
        if arg.starts_with('/') || arg.starts_with('-') {
            let opt = arg.trim_start_matches('/').trim_start_matches('-');
            match opt {
                "E" => recursive = true,
                "MIR" => {
                    mirror = true;
                    recursive = true; // MIR implies /E
                }
                "?" => {
                    print_help();
                    std::process::exit(0);
                }
                _ if opt.starts_with('R') => {
                    let n_str = &opt[1..];
                    retry = n_str
                        .parse()
                        .with_context(|| format!("Invalid /R value: {}", n_str))?;
                }
                _ if opt.starts_with('W') => {
                    let n_str = &opt[1..];
                    wait_secs = n_str
                        .parse()
                        .with_context(|| format!("Invalid /W value: {}", n_str))?;
                }
                _ => anyhow::bail!("Unknown option: {}", arg),
            }
        } else {
            // 路径参数
            if source.is_none() {
                source = Some(PathBuf::from(arg));
            } else if dest.is_none() {
                dest = Some(PathBuf::from(arg));
            } else {
                anyhow::bail!("Too many path arguments (only source and destination allowed)");
            }
        }
    }

    let source = source.ok_or_else(|| anyhow!("Missing source path"))?;
    let dest = dest.ok_or_else(|| anyhow!("Missing destination path"))?;

    // 检查源是否存在
    if !source.exists() {
        anyhow::bail!("Source path does not exist: {}", source.display());
    }
    // 检查源和目标是否相同（避免自我复制）
    if source == dest {
        anyhow::bail!("Source and destination paths are the same");
    }

    Ok(Config {
        source,
        dest,
        mirror,
        recursive,
        retry,
        wait_secs,
    })
}

// ---------- 复制单个文件，带重试 ----------
fn copy_file_with_retry(
    src: &Path,
    dst: &Path,
    retry: u32,
    wait_secs: u64,
) -> Result<()> {
    for attempt in 0..=retry {
        if attempt > 0 {
            eprintln!(
                "Retry {} for {} (wait {}s)",
                attempt,
                src.display(),
                wait_secs
            );
            thread::sleep(Duration::from_secs(wait_secs));
        }

        match fs::copy(src, dst) {
            Ok(_) => {
                // 保留源文件的权限（Unix）
                if let Ok(meta) = fs::metadata(src) {
                    let _ = fs::set_permissions(dst, meta.permissions());
                }
                return Ok(());
            }
            Err(e) => {
                if attempt == retry {
                    return Err(anyhow!("Failed to copy {}: {}", src.display(), e));
                }
                eprintln!("Copy error: {}, retrying...", e);
            }
        }
    }
    unreachable!()
}

// ---------- 主复制逻辑 ----------
fn run_robocopy(config: &Config) -> Result<()> {
    let src = &config.source;
    let dst = &config.dest;
    let mirror = config.mirror;
    let recursive = config.recursive;
    let retry = config.retry;
    let wait_secs = config.wait_secs;

    // 如果目标路径不存在且是目录，则创建
    if !dst.exists() {
        fs::create_dir_all(dst)
            .with_context(|| format!("Cannot create destination directory: {}", dst.display()))?;
    }

    // 如果目标是文件，报错
    if dst.is_file() {
        anyhow::bail!("Destination path is a file, not a directory: {}", dst.display());
    }

    // 收集源的所有条目（用于镜像删除）
    let src_entries: Option<HashSet<PathBuf>> = if mirror {
        let mut set = HashSet::new();
        for entry in WalkDir::new(src).follow_links(false) {
            let entry = match entry {
                Ok(e) => e,
                Err(e) => {
                    eprintln!("Warning: skipping source entry due to error: {}", e);
                    continue;
                }
            };
            let path = entry.path();
            let rel = path
                .strip_prefix(src)
                .expect("Path should start with source root")
                .to_path_buf();
            set.insert(rel);
        }
        Some(set)
    } else {
        None
    };

    // 遍历源目录进行复制
    let mut copied_count = 0;
    let mut failed_count = 0;
    let mut skipped_count = 0;
    let mut dir_created_count = 0;

    let walker = WalkDir::new(src).follow_links(false);
    for entry in walker {
        let entry = match entry {
            Ok(e) => e,
            Err(e) => {
                eprintln!("Warning: skipping source entry due to error: {}", e);
                continue;
            }
        };
        let src_path = entry.path();
        let rel_path = src_path
            .strip_prefix(src)
            .expect("Path should start with source root");
        let dst_path = dst.join(rel_path);

        let meta = match fs::symlink_metadata(src_path) {
            Ok(m) => m,
            Err(e) => {
                eprintln!("Warning: cannot read metadata for {}: {}", src_path.display(), e);
                continue;
            }
        };

        if meta.is_dir() {
            // 目录处理
            if rel_path.as_os_str().is_empty() || recursive {
                // 根目录总是创建，递归模式创建所有目录
                if !dst_path.exists() {
                    fs::create_dir_all(&dst_path)?;
                    dir_created_count += 1;
                }
            }
            // 非递归模式且非根目录，跳过
            continue;
        } else if meta.is_file() {
            // 文件处理
            if !recursive && rel_path.parent().is_some() {
                // 非递归模式跳过子目录中的文件
                continue;
            }
            // 确保目标父目录存在
            if let Some(parent) = dst_path.parent() {
                if !parent.exists() {
                    fs::create_dir_all(parent)?;
                    dir_created_count += 1;
                }
            }

            // 判断是否需要复制：目标不存在，或源修改时间更新，或大小不同
            let need_copy = if dst_path.exists() {
                let src_mtime = meta.modified().ok();
                let dst_meta = fs::metadata(&dst_path).ok();
                match (src_mtime, dst_meta) {
                    (Some(sm), Some(dm)) => {
                        let src_size = meta.len();
                        let dst_size = dm.len();
                        // 如果源更新或大小不同，需要复制
                        sm > dm.modified().ok().unwrap_or(std::time::SystemTime::UNIX_EPOCH)
                            || src_size != dst_size
                    }
                    _ => true, // 无法比较则复制
                }
            } else {
                true
            };

            if need_copy {
                eprint!(
                    "Copying {} -> {} ... ",
                    src_path.display(),
                    dst_path.display()
                );
                match copy_file_with_retry(src_path, &dst_path, retry, wait_secs) {
                    Ok(_) => {
                        eprintln!("OK");
                        copied_count += 1;
                    }
                    Err(e) => {
                        eprintln!("FAILED: {}", e);
                        failed_count += 1;
                    }
                }
            } else {
                skipped_count += 1;
            }
        } else {
            // 其他类型（符号链接等）跳过
            eprintln!("Skipping non-file/dir: {}", src_path.display());
        }
    }

    // ---------- 镜像模式：删除目标中多余的文件/目录 ----------
    if mirror {
        if let Some(src_set) = src_entries {
            // 第一遍：删除源中不存在的目录（直接整个删除，避免遍历其内部）
            let mut extra_dirs = Vec::new();
            for entry in WalkDir::new(dst).follow_links(false).min_depth(1) {
                let entry = match entry {
                    Ok(e) => e,
                    Err(e) => {
                        eprintln!("Warning: cannot access target entry: {}", e);
                        continue;
                    }
                };
                let dst_path = entry.path();
                let rel = dst_path
                    .strip_prefix(dst)
                    .expect("Path should start with destination root");
                if dst_path.is_dir() && !src_set.contains(rel) {
                    extra_dirs.push(dst_path.to_path_buf());
                }
            }
            // 按路径长度降序排序，确保先删除子目录（虽然remove_dir_all会自动递归，但排序可减少冲突）
            extra_dirs.sort_by_key(|p| std::cmp::Reverse(p.components().count()));
            for dir in extra_dirs {
                eprintln!("Removing extra directory: {}", dir.display());
                if let Err(e) = fs::remove_dir_all(&dir) {
                    eprintln!("Warning: failed to remove directory {}: {}", dir.display(), e);
                }
            }

            // 第二遍：删除源中不存在的文件（目录已经处理过，现在只会遇到存在于源的目录中的文件）
            for entry in WalkDir::new(dst).follow_links(false).min_depth(1) {
                let entry = match entry {
                    Ok(e) => e,
                    Err(e) => {
                        eprintln!("Warning: cannot access target entry: {}", e);
                        continue;
                    }
                };
                let dst_path = entry.path();
                let rel = dst_path
                    .strip_prefix(dst)
                    .expect("Path should start with destination root");
                if dst_path.is_file() && !src_set.contains(rel) {
                    eprintln!("Removing extra file: {}", dst_path.display());
                    if let Err(e) = fs::remove_file(&dst_path) {
                        eprintln!("Warning: failed to remove file {}: {}", dst_path.display(), e);
                    }
                }
            }
        }
    }

    // 输出汇总
    eprintln!(
        "\nSummary:\n  Files copied: {}\n  Files skipped: {}\n  Files failed: {}\n  Directories created: {}",
        copied_count, skipped_count, failed_count, dir_created_count
    );

    // 如果有失败，返回错误退出码
    if failed_count > 0 {
        Err(anyhow!("{} file(s) failed to copy", failed_count))
    } else {
        Ok(())
    }
}

// ---------- 入口 ----------
fn main() -> Result<()> {
    let config = match parse_args() {
        Ok(c) => c,
        Err(e) => {
            eprintln!("Error parsing arguments: {}", e);
            print_help();
            std::process::exit(1);
        }
    };

    if let Err(e) = run_robocopy(&config) {
        eprintln!("Error during copy: {}", e);
        std::process::exit(1);
    }

    Ok(())
}