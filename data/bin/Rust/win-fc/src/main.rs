use std::env;
use std::fs::File;
use std::io::{self, BufRead, BufReader, Read};

#[derive(Debug, Default)]
struct Config {
    a_mode: bool,
    b_mode: bool,
    c_mode: bool,
    l_mode: bool,
    lb_value: usize,
    n_mode: bool,
    offline_mode: bool,
    t_mode: bool,
    u_mode: bool,
    w_mode: bool,
    resync_lines: usize,
}

impl Config {
    fn from_args() -> Result<(Self, String, String), i32> {
        let args: Vec<String> = env::args().collect();
        let mut cfg = Config::default();
        cfg.lb_value = 100;
        cfg.resync_lines = 2;

        let mut file1 = None;
        let mut file2 = None;

        for i in 1..args.len() {
            let arg = &args[i];
            if arg.starts_with('/') {
                if arg.starts_with("/lb") && arg.len() > 3 {
                    if let Ok(n) = arg[3..].parse::<usize>() {
                        cfg.lb_value = n;
                    }
                    continue;
                }
                if arg.len() > 1 && arg[1..].chars().all(|c| c.is_ascii_digit()) {
                    if let Ok(n) = arg[1..].parse::<usize>() {
                        cfg.resync_lines = n;
                    }
                    continue;
                }
                match arg.chars().nth(1).unwrap() {
                    'a' => cfg.a_mode = true,
                    'b' => cfg.b_mode = true,
                    'c' => cfg.c_mode = true,
                    'l' => cfg.l_mode = true,
                    'n' => cfg.n_mode = true,
                    't' => cfg.t_mode = true,
                    'u' => cfg.u_mode = true,
                    'w' => cfg.w_mode = true,
                    'o' => cfg.offline_mode = true,
                    _ => {
                        eprintln!("无效参数: {}", arg);
                        return Err(1);
                    }
                }
            } else {
                if file1.is_none() {
                    file1 = Some(arg.clone());
                } else if file2.is_none() {
                    file2 = Some(arg.clone());
                } else {
                    eprintln!("错误：只能指定两个文件");
                    return Err(1);
                }
            }
        }

        let file1 = match file1 {
            Some(f) => f,
            None => {
                show_help(&args[0]);
                return Err(0);
            }
        };
        let file2 = match file2 {
            Some(f) => f,
            None => {
                show_help(&args[0]);
                return Err(0);
            }
        };

        if !cfg.l_mode && !cfg.b_mode {
            cfg.b_mode = is_binary_file(&file1) || is_binary_file(&file2);
        }

        Ok((cfg, file1, file2))
    }
}

fn show_help(prog: &str) {
    let help = format!(
        "用法: {} [/a] [/b] [/c] [/l] [/lbN] [/n] [/off[line]] [/t] [/u] [/w] [/N] 文件1 文件2
  /a         仅显示每组差异的第一行和最后一行。
  /b         二进制模式。
  /c         忽略大小写。
  /l         ASCII文本模式。
  /lbN       设置内部行缓冲区大小为N行。
  /n         显示行号。
  /off[line] 不跳过脱机文件（仅占位）。
  /t         保留制表符。
  /u         Unicode模式（简单支持）。
  /w         压缩空白字符。
  /N         重新同步所需的连续匹配行数。",
        prog
    );
    eprintln!("{}", help);
}

fn is_binary_file(path: &str) -> bool {
    if let Ok(mut f) = File::open(path) {
        let mut buf = [0; 4096];
        while let Ok(n) = f.read(&mut buf) {
            if n == 0 {
                break;
            }
            if buf[..n].contains(&0) {
                return true;
            }
        }
    }
    false
}

fn normalize_line(line: &str, cfg: &Config) -> String {
    let mut s = line.to_string();
    if cfg.w_mode {
        let trimmed = s.trim();
        let mut result = String::new();
        let mut last_was_space = false;
        for ch in trimmed.chars() {
            if ch.is_whitespace() {
                if !last_was_space {
                    result.push(' ');
                    last_was_space = true;
                }
            } else {
                result.push(ch);
                last_was_space = false;
            }
        }
        s = result;
    } else if !cfg.t_mode {
        s = s.replace('\t', " ");
    }
    if cfg.c_mode {
        s = s.to_lowercase();
    }
    s
}

fn compare_text(cfg: &Config, path1: &str, path2: &str) -> io::Result<bool> {
    let file1 = File::open(path1)?;
    let file2 = File::open(path2)?;
    let mut reader1 = BufReader::new(file1).lines();
    let mut reader2 = BufReader::new(file2).lines();
    let mut line_num = 0;
    let mut diff_found = false;
    let mut in_diff_block = false;
    let mut diff_start_line = 0;
    let mut match_count = 0;

    println!("比较文件 {} 和 {}", path1, path2);

    loop {
        let line1 = reader1.next().transpose()?;
        let line2 = reader2.next().transpose()?;
        line_num += 1;

        let (r1, r2) = match (line1, line2) {
            (None, None) => break,
            (Some(l1), Some(l2)) => (l1, l2),
            (None, Some(l2)) => {
                if !in_diff_block {
                    println!("***** {}", path1);
                    println!("EOF");
                    println!("***** {}", path2);
                    if cfg.n_mode {
                        print!("{}: ", line_num);
                    }
                    println!("{}", l2);
                }
                diff_found = true;
                break;
            }
            (Some(l1), None) => {
                if !in_diff_block {
                    println!("***** {}", path1);
                    if cfg.n_mode {
                        print!("{}: ", line_num);
                    }
                    println!("{}", l1);
                    println!("***** {}", path2);
                    println!("EOF");
                }
                diff_found = true;
                break;
            }
        };

        let norm1 = normalize_line(&r1, cfg);
        let norm2 = normalize_line(&r2, cfg);

        if norm1 != norm2 {
            if !in_diff_block {
                in_diff_block = true;
                diff_start_line = line_num;
                println!("***** {}", path1);
                if cfg.n_mode {
                    print!("{}: ", line_num);
                }
                println!("{}", r1);
                println!("***** {}", path2);
                if cfg.n_mode {
                    print!("{}: ", line_num);
                }
                println!("{}", r2);
            } else {
                if !cfg.a_mode || line_num - diff_start_line <= 1 {
                    if cfg.n_mode {
                        print!("{}: ", line_num);
                    }
                    println!("{}", r1);
                    if cfg.n_mode {
                        print!("{}: ", line_num);
                    }
                    println!("{}", r2);
                }
            }
            match_count = 0;
            diff_found = true;
        } else {
            if in_diff_block {
                match_count += 1;
                if match_count >= cfg.resync_lines {
                    if cfg.a_mode && line_num - diff_start_line > 1 {
                        println!("...");
                    }
                    in_diff_block = false;
                }
            }
        }
    }

    if !diff_found {
        println!("FC: no differences encountered");
    }
    Ok(diff_found)
}

// 修复后的二进制比较函数（兼容旧版 Rust）
fn compare_binary(_cfg: &Config, path1: &str, path2: &str) -> io::Result<bool> {
    let mut file1 = File::open(path1)?;
    let mut file2 = File::open(path2)?;
    let mut offset: usize = 0;
    let mut diff_count = 0;
    let mut diff_found = false;
    println!("比较文件 {} 和 {}", path1, path2);

    loop {
        let mut buf1 = [0u8; 1];
        let mut buf2 = [0u8; 1];
        let r1 = file1.read_exact(&mut buf1);
        let r2 = file2.read_exact(&mut buf2);

        // 使用 match 配合内部 if，避免 guard pattern
        match (r1, r2) {
            (Ok(_), Ok(_)) => {
                if buf1[0] != buf2[0] {
                    println!("{:08X}: {:02X} {:02X}", offset, buf1[0], buf2[0]);
                    diff_found = true;
                    diff_count += 1;
                    if diff_count >= 10 {
                        println!("比较已停止。差异过多。");
                        break;
                    }
                }
            }
            (Err(ref e1), Ok(_)) => {
                if e1.kind() == io::ErrorKind::UnexpectedEof {
                    println!("{:08X}: EOF {:02X}", offset, buf2[0]);
                    diff_found = true;
                    break;
                }
            }
            (Ok(_), Err(ref e2)) => {
                if e2.kind() == io::ErrorKind::UnexpectedEof {
                    println!("{:08X}: {:02X} EOF", offset, buf1[0]);
                    diff_found = true;
                    break;
                }
            }
            (Err(_), Err(_)) => break,
        }
        offset += 1;
    }

    if !diff_found {
        println!("FC: no differences encountered");
    }
    Ok(diff_found)
}

fn main() {
    let (cfg, file1, file2) = match Config::from_args() {
        Ok(v) => v,
        Err(0) => return,
        Err(code) => std::process::exit(code),
    };

    let result = if cfg.b_mode {
        compare_binary(&cfg, &file1, &file2)
    } else {
        compare_text(&cfg, &file1, &file2)
    };

    match result {
        Ok(true) => std::process::exit(1),
        Ok(false) => std::process::exit(0),
        Err(e) => {
            eprintln!("错误: {}", e);
            std::process::exit(2);
        }
    }
}