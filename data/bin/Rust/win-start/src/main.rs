use std::env;
use std::path::Path;
use std::process::{Command, Stdio};
use std::thread;
use std::time::Duration;

use anyhow::{bail, Context, Result};
use which::which;

/// 打印帮助信息
fn print_help() {
    println!(
        r#"Usage: start-rs [options] <file|URL|program> [arguments...]

Options:
  /wait          Wait for the launched program to exit (only for executables).
  /?             Show this help.

Examples:
  start-rs image.jpg                  # 打开图片
  start-rs https://example.com        # 打开 URL
  start-rs /bin/bash                  # 启动 bash（后台）
  start-rs /wait /bin/bash            # 启动 bash 并等待退出
"#
    );
}

/// 判断是否为可执行文件（路径存在且可执行）
fn is_executable(path: &Path) -> bool {
    if let Ok(meta) = std::fs::metadata(path) {
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            let mode = meta.permissions().mode();
            mode & 0o111 != 0
        }
        #[cfg(not(unix))]
        {
            meta.is_file()
        }
    } else {
        false
    }
}

/// 尝试用 termux-open 打开
fn open_with_termux(target: &str) -> Result<()> {
    // 如果是 URL，尝试 termux-open-url
    let is_url = target.starts_with("http://") || target.starts_with("https://");
    let cmd = if is_url { "termux-open-url" } else { "termux-open" };
    
    // 检查命令是否存在
    if which(cmd).is_err() {
        bail!("{} not found, please install termux-api", cmd);
    }
    
    let status = Command::new(cmd)
        .arg(target)
        .status()
        .context(format!("Failed to run {}", cmd))?;
    
    if status.success() {
        Ok(())
    } else {
        bail!("{} returned non-zero exit code", cmd)
    }
}

/// 尝试用 xdg-open（通用 Linux）
fn open_with_xdg(target: &str) -> Result<()> {
    if which("xdg-open").is_err() {
        bail!("xdg-open not found");
    }
    let status = Command::new("xdg-open")
        .arg(target)
        .status()
        .context("Failed to run xdg-open")?;
    if status.success() {
        Ok(())
    } else {
        bail!("xdg-open returned non-zero")
    }
}

fn main() -> Result<()> {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        print_help();
        return Ok(());
    }

    // 解析 /wait 和 /?
    let mut wait = false;
    let mut remaining_args = Vec::new();
    for arg in &args[1..] {
        if arg == "/?" || arg == "-?" || arg == "--help" {
            print_help();
            return Ok(());
        } else if arg == "/wait" || arg == "-wait" {
            wait = true;
        } else {
            remaining_args.push(arg.clone());
        }
    }

    if remaining_args.is_empty() {
        bail!("No target specified");
    }

    let target = &remaining_args[0];
    let target_path = Path::new(target);
    let target_args = &remaining_args[1..];

    // 判断目标类型
    // 1. 如果存在且是可执行文件，则直接运行
    if target_path.exists() && is_executable(target_path) {
        let mut cmd = Command::new(target_path);
        cmd.args(target_args);
        cmd.stdin(Stdio::inherit()).stdout(Stdio::inherit()).stderr(Stdio::inherit());

        if wait {
            // 等待执行完毕
            let status = cmd.status().context("Failed to run executable")?;
            if !status.success() {
                bail!("Executable exited with non-zero status");
            }
        } else {
            // 后台运行：spawn 且不等待
            cmd.spawn().context("Failed to spawn executable")?;
            println!("Started {} in background", target);
        }
        return Ok(());
    }

    // 2. 如果是 URL 或文件，尝试用 termux-open 或 xdg-open
    // 先尝试 termux（Termux 专用）
    if let Ok(()) = open_with_termux(target) {
        return Ok(());
    }

    // 再尝试 xdg-open
    if let Ok(()) = open_with_xdg(target) {
        return Ok(());
    }

    // 都不行，报错
    bail!("Cannot open '{}': no suitable handler found", target);
}