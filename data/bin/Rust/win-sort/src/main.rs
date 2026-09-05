use std::env;
use std::fs::File;
use std::io::{self, BufRead, BufReader, Write, BufWriter};
use std::process;

fn usage() {
    println!(
        "SORT [/R] [/+n] [/O output] [/C] [/U] [/I] [/?] [file ...]\n\n\
         Description:\n\
         \tSorts text read from files or standard input.\n\n\
         \t/R        Reverse sort order (descending).\n\
         \t/+n       Begin comparison at character n (1-based).\n\
         \t/O output Write output to the specified file.\n\
         \t/C        Check if the input is already sorted.\n\
         \t/U        Output only unique lines (remove duplicates).\n\
         \t/I        Ignore case when comparing.\n\
         \t/?        Display this help.\n\n\
         Examples:\n\
         \tsort file.txt\n\
         \tsort /R /+2 data.txt\n\
         \tsort /U /I input.txt /O output.txt\n\
         \tsort /C /U file.txt"
    );
}

fn get_key(s: &str, key_start: usize) -> &str {
    if key_start == 0 {
        return s;
    }
    // 查找第 key_start 个字符的字节偏移（key_start 为 0 基字符索引）
    let mut char_count = 0;
    for (idx, _) in s.char_indices() {
        if char_count == key_start {
            return &s[idx..];
        }
        char_count += 1;
    }
    // 若 key_start 超出字符串长度，返回空串
    ""
}

fn main() {
    let args: Vec<String> = env::args().skip(1).collect();

    if args.is_empty() || args.iter().any(|a| a == "/?" || a == "--help") {
        usage();
        return;
    }

    let mut reverse = false;
    let mut unique = false;
    let mut check = false;
    let mut ignore_case = false;
    let mut key_start: usize = 0; // 0 基字符索引
    let mut output_file: Option<String> = None;
    let mut input_files: Vec<String> = Vec::new();

    let mut i = 0;
    while i < args.len() {
        let arg = &args[i];
        if arg.starts_with('/') || arg.starts_with('-') {
            // 处理 /O 或 -O，可能紧跟文件名
            if arg.len() >= 2 && (arg.as_bytes()[1] as char).eq_ignore_ascii_case(&'O') {
                if arg.len() > 2 {
                    output_file = Some(arg[2..].to_string());
                } else {
                    if i + 1 < args.len() {
                        i += 1;
                        output_file = Some(args[i].clone());
                    } else {
                        eprintln!("Error: /O requires a filename.");
                        process::exit(1);
                    }
                }
                i += 1;
                continue;
            }
            // 处理其他选项字符
            let mut j = 1;
            while j < arg.len() {
                let c = arg.as_bytes()[j] as char;
                let c_upper = c.to_ascii_uppercase();
                match c_upper {
                    'R' => reverse = true,
                    'U' => unique = true,
                    'C' => check = true,
                    'I' => ignore_case = true,
                    '+' => {
                        // 解析 /+n 中的数字
                        if j + 1 < arg.len() {
                            let num_str = &arg[j + 1..];
                            if let Ok(n) = num_str.parse::<usize>() {
                                key_start = n.saturating_sub(1); // 1 基转 0 基
                            } else {
                                key_start = 0;
                            }
                            j = arg.len(); // 跳过剩余字符
                            continue;
                        } else {
                            // "+" 后无数字，按 0 处理
                            key_start = 0;
                            j += 1;
                            continue;
                        }
                    }
                    '?' => {
                        usage();
                        return;
                    }
                    _ => {
                        eprintln!("Unknown option: /{}", c);
                        process::exit(1);
                    }
                }
                j += 1;
            }
        } else {
            input_files.push(arg.clone());
        }
        i += 1;
    }

    // 读取所有行
    let mut lines: Vec<String> = Vec::new();
    if input_files.is_empty() {
        let stdin = io::stdin();
        for line in stdin.lock().lines() {
            match line {
                Ok(l) => lines.push(l),
                Err(e) => {
                    eprintln!("Error reading stdin: {}", e);
                    process::exit(1);
                }
            }
        }
    } else {
        for fname in &input_files {
            match File::open(fname) {
                Ok(file) => {
                    let reader = BufReader::new(file);
                    for line in reader.lines() {
                        match line {
                            Ok(l) => lines.push(l),
                            Err(e) => {
                                eprintln!("Error reading {}: {}", fname, e);
                                process::exit(1);
                            }
                        }
                    }
                }
                Err(e) => {
                    eprintln!("Error: Cannot open {}: {}", fname, e);
                    process::exit(1);
                }
            }
        }
    }

    // 定义比较闭包
    let cmp = |a: &String, b: &String| -> std::cmp::Ordering {
        let key_a = get_key(a, key_start);
        let key_b = get_key(b, key_start);
        let res = if ignore_case {
            key_a.to_lowercase().cmp(&key_b.to_lowercase())
        } else {
            key_a.cmp(&key_b)
        };
        if reverse {
            res.reverse()
        } else {
            res
        }
    };

    // 检查模式
    if check {
        let mut sorted = true;
        for i in 1..lines.len() {
            if cmp(&lines[i], &lines[i - 1]) == std::cmp::Ordering::Less {
                sorted = false;
                break;
            }
            if unique {
                let equal = if ignore_case {
                    lines[i].to_lowercase() == lines[i - 1].to_lowercase()
                } else {
                    lines[i] == lines[i - 1]
                };
                if equal {
                    sorted = false;
                    break;
                }
            }
        }
        if sorted {
            process::exit(0);
        } else {
            process::exit(1);
        }
    }

    // 排序
    lines.sort_by(cmp);

    // 去重
    if unique {
        let mut unique_lines: Vec<String> = Vec::new();
        for line in lines {
            if unique_lines.is_empty() {
                unique_lines.push(line);
            } else {
                let last = unique_lines.last().unwrap();
                let equal = if ignore_case {
                    line.to_lowercase() == last.to_lowercase()
                } else {
                    line == *last
                };
                if !equal {
                    unique_lines.push(line);
                }
            }
        }
        lines = unique_lines;
    }

    // 输出结果
    if let Some(out_file) = output_file {
        match File::create(&out_file) {
            Ok(file) => {
                let mut writer = BufWriter::new(file);
                for line in &lines {
                    if let Err(e) = writeln!(writer, "{}", line) {
                        eprintln!("Error writing to {}: {}", out_file, e);
                        process::exit(1);
                    }
                }
                if let Err(e) = writer.flush() {
                    eprintln!("Error flushing output file: {}", e);
                    process::exit(1);
                }
            }
            Err(e) => {
                eprintln!("Error: Cannot open output file {}: {}", out_file, e);
                process::exit(1);
            }
        }
    } else {
        let stdout = io::stdout();
        let mut writer = BufWriter::new(stdout.lock());
        for line in &lines {
            if let Err(e) = writeln!(writer, "{}", line) {
                eprintln!("Error writing to stdout: {}", e);
                process::exit(1);
            }
        }
        if let Err(e) = writer.flush() {
            eprintln!("Error flushing stdout: {}", e);
            process::exit(1);
        }
    }
}