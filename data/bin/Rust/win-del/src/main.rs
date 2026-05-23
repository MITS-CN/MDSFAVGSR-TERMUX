// del.rs - Windows 风格的 del 命令 for Termux (Rust 实现)
//
// 使用方法：
//   cargo build --release
//   ./target/release/del [选项] 文件/模式...

use std::env;
use std::fs;
use std::io::{self, Write};
use std::path::{Path, PathBuf};
use std::process;

use glob::Pattern;
use walkdir::WalkDir;

/// 检查是否在 Termux 环境下运行
fn is_termux() -> bool {
    // 检查特有的环境变量
    if env::var("TERMUX_VERSION").is_ok() {
        return true;
    }
    // 检查 Termux 专属目录
    if Path::new("/data/data/com.termux/files/usr").is_dir() {
        return true;
    }
    // 检查 PREFIX 环境变量
    if let Ok(prefix) = env::var("PREFIX") {
        if prefix == "/data/data/com.termux/files/usr" {
            return true;
        }
    }
    // 检查 termux-info 命令是否存在（通过 which 搜索）
    if which::which("termux-info").is_ok() {
        return true;
    }
    false
}

/// 显示帮助信息（与原始脚本一致）
fn show_help() {
    println!("Deletes one or more files.");
    println!();
    println!("DEL [/P] [/F] [/S] [/Q] [/A:attributes] names");
    println!();
    println!("  names         Specifies a list of one or more files or directories.");
    println!("                Wildcards may be used to delete multiple files.");
    println!("  /P            Prompts for confirmation before deleting each file.");
    println!("  /F            Force deleting of read-only files.");
    println!("  /S            Delete specified files from all subdirectories.");
    println!("  /Q            Quiet mode, do not ask if ok to delete on global wildcard");
    println!();
    println!("  /?            Display this help message");
    println!();
    println!("Examples:");
    println!("  del file.txt           # Delete a single file");
    println!("  del *.tmp /Q           # Delete all .tmp files quietly");
    println!("  del /S *.bak           # Delete .bak files from current and subdirectories");
}

/// 命令行配置
struct Config {
    prompt: bool,
    force: bool,
    recursive: bool,
    quiet: bool,
    targets: Vec<String>,
}

/// 解析命令行参数，返回配置
fn parse_args() -> Result<Config, String> {
    let mut config = Config {
        prompt: false,
        force: false,
        recursive: false,
        quiet: false,
        targets: Vec::new(),
    };

    let mut args = env::args().skip(1); // 跳过程序名

    // 处理第一个参数：无参数或 /? 直接显示帮助并退出
    let first = match args.next() {
        None => {
            show_help();
            process::exit(0);
        }
        Some(ref s) if s == "/?" => {
            show_help();
            process::exit(0);
        }
        Some(first) => first,
    };

    // 将所有参数（包括 first）统一处理
    let all_args: Vec<String> = std::iter::once(first).chain(args).collect();
    for arg in all_args {
        match arg.to_uppercase().as_str() {
            "/P" => config.prompt = true,
            "/F" => config.force = true,
            "/S" => config.recursive = true,
            "/Q" => config.quiet = true,
            // 忽略不支持的 /A 选项，或可以提示错误
            _ => config.targets.push(arg),
        }
    }

    if config.targets.is_empty() {
        return Err("Error: No files specified.".to_string());
    }

    Ok(config)
}

/// 询问用户是否确认删除
fn ask_confirmation(file_path: &Path) -> bool {
    print!("Delete {}? (Y/N) ", file_path.display());
    io::stdout().flush().ok();

    let mut input = String::new();
    if io::stdin().read_line(&mut input).is_err() {
        return false;
    }

    matches!(input.trim().chars().next(), Some('y' | 'Y'))
}

/// 尝试给文件添加写权限（对应 /F 强制删除只读文件）
fn make_writable(file_path: &Path) -> io::Result<()> {
    let metadata = fs::metadata(file_path)?;
    let mut perms = metadata.permissions();
    if perms.readonly() {
        // 添加所有者写权限
        use std::os::unix::fs::PermissionsExt;
        let mode = perms.mode();
        perms.set_mode(mode | 0o200);
        fs::set_permissions(file_path, perms)?;
    }
    Ok(())
}

/// 删除单个文件，应用 /F /P 等设置
fn delete_file(file_path: &Path, config: &Config) -> bool {
    if !file_path.exists() {
        if !config.force {
            eprintln!("Could not find {}", file_path.display());
            return false;
        }
        return true; // /F 模式下跳过不存在的文件
    }

    // 提示确认（/P 优先于 /Q）
    if config.prompt {
        if !ask_confirmation(file_path) {
            return false; // 用户选择不删除
        }
    }

    // /F 处理只读文件
    if config.force {
        if let Err(e) = make_writable(file_path) {
            // 如果无法修改权限，仍尝试删除
            eprintln!("Warning: Cannot change permissions for {}: {}", file_path.display(), e);
        }
    }

    // 执行删除
    match fs::remove_file(file_path) {
        Ok(()) => true,
        Err(e) => {
            eprintln!("Error deleting {}: {}", file_path.display(), e);
            false
        }
    }
}

/// 递归删除子目录中匹配模式的文件（对应 /S）
fn recursive_delete(start_dir: &Path, pattern: &str, config: &Config) -> bool {
    // 解析模式，如果 pattern 包含路径分隔符，拆分为基础路径和文件名模式
    let (base, file_pattern) = if pattern.contains('/') {
        let p = Path::new(pattern);
        if let Some(parent) = p.parent() {
            if parent.as_os_str().is_empty() {
                (PathBuf::from("."), p.file_name().unwrap().to_str().unwrap().to_string())
            } else {
                (parent.to_path_buf(), p.file_name().unwrap().to_str().unwrap().to_string())
            }
        } else {
            (PathBuf::from("."), pattern.to_string())
        }
    } else {
        (start_dir.to_path_buf(), pattern.to_string())
    };

    let search_root = start_dir.join(&base);
    if !search_root.is_dir() {
        eprintln!("Error: Directory not found - {}", search_root.display());
        return false;
    }

    let glob_pattern = match Pattern::new(&file_pattern) {
        Ok(p) => p,
        Err(e) => {
            eprintln!("Invalid pattern: {} ({})", file_pattern, e);
            return false;
        }
    };

    let mut any_failed = false;
    for entry in WalkDir::new(&search_root).follow_links(false) {
        if let Ok(entry) = entry {
            if entry.file_type().is_file() {
                if glob_pattern.matches(entry.file_name().to_str().unwrap_or("")) {
                    if !delete_file(entry.path(), config) {
                        any_failed = true;
                    }
                }
            }
        }
    }

    !any_failed
}

/// 非递归模式：处理普通文件和通配符
fn normal_delete(targets: &[String], config: &Config) -> bool {
    let mut any_failed = false;

    for target in targets {
        // 包含通配符则展开，否则当作具体路径
        if target.contains('*') || target.contains('?') {
            match glob::glob(target) {
                Ok(paths) => {
                    let mut found = false;
                    for entry in paths.flatten() {
                        found = true;
                        if entry.is_file() {
                            if !delete_file(&entry, config) {
                                any_failed = true;
                            }
                        } // 忽略目录
                    }
                    if !found && !config.force {
                        eprintln!("Could not find {}", target);
                        any_failed = true;
                    }
                }
                Err(e) => {
                    eprintln!("Invalid pattern: {} ({})", target, e);
                    any_failed = true;
                }
            }
        } else {
            // 具体路径
            let path = Path::new(target);
            if path.is_dir() {
                eprintln!("Error: '{}' is a directory (del only deletes files).", target);
                any_failed = true;
            } else if !delete_file(path, config) {
                any_failed = true;
            }
        }
    }

    !any_failed
}

fn main() {
    // Termux 环境检查（若需跨平台可移除此段）
    if !is_termux() {
        eprintln!("Error: This command only works in Termux environment.");
        process::exit(1);
    }

    let config = match parse_args() {
        Ok(c) => c,
        Err(e) => {
            eprintln!("{}", e);
            process::exit(1);
        }
    };

    let success = if config.recursive {
        let mut overall = true;
        for target in &config.targets {
            if !recursive_delete(Path::new("."), target, &config) {
                overall = false;
            }
        }
        overall
    } else {
        normal_delete(&config.targets, &config)
    };

    if !success {
        process::exit(1);
    }
}