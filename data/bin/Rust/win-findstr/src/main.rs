use std::env;
use std::fs;
use std::io::{self, BufRead, BufReader, Write};
use std::path::{Path, PathBuf};
use regex::{Regex, RegexBuilder};

// ------------------------------------------------------------
// 选项结构体
// ------------------------------------------------------------
struct Options {
    literal: bool,        // /L
    regex: bool,          // /R (默认)
    ignore_case: bool,    // /I
    line_begin: bool,     // /B
    line_end: bool,       // /E
    exact: bool,          // /X (整行匹配)
    invert: bool,         // /V
    line_number: bool,    // /N
    filename_only: bool,  // /M
    offset: bool,         // /O
    subdirs: bool,        // /S
    skip_binary: bool,    // /P
    // 这些字段用于存储 /F, /G, /D 指定的额外数据
    file_lists: Vec<String>,    // /F:file 读入的文件路径列表
    pattern_files: Vec<String>, // /G:file 读入的模式文件
    dir_lists: Vec<String>,     // /D:dirlist 指定的目录
}

impl Default for Options {
    fn default() -> Self {
        Options {
            literal: false,
            regex: true,
            ignore_case: false,
            line_begin: false,
            line_end: false,
            exact: false,
            invert: false,
            line_number: false,
            filename_only: false,
            offset: false,
            subdirs: false,
            skip_binary: false,
            file_lists: Vec::new(),
            pattern_files: Vec::new(),
            dir_lists: Vec::new(),
        }
    }
}

// ------------------------------------------------------------
// 帮助信息
// ------------------------------------------------------------
fn print_help() {
    println!("Searches for strings in files.\n");
    println!("FINDSTR [/B] [/E] [/L] [/R] [/S] [/I] [/X] [/V] [/N] [/M] [/O] [/P]");
    println!("        [/F:file] [/C:string] [/G:file] [/D:dirlist] [/OFF[LINE]]");
    println!("        strings [[drive:][path]filename[ ...]]\n");
    println!("  /B         Matches pattern if at the beginning of a line.");
    println!("  /E         Matches pattern if at the end of a line.");
    println!("  /L         Uses search strings literally.");
    println!("  /R         Uses search strings as regular expressions. (default)");
    println!("  /S         Searches for matching files in the current directory and all");
    println!("             subdirectories.");
    println!("  /I         Specifies that the search is not to be case-sensitive.");
    println!("  /X         Prints lines that match exactly.");
    println!("  /V         Prints only lines that do not contain a match.");
    println!("  /N         Prints the line number before each line that matches.");
    println!("  /M         Prints only the filename if a file contains a match.");
    println!("  /O         Prints character offset before each matching line.");
    println!("  /P         Skip files with non-printable characters.");
    println!("  /OFF[LINE] Do not skip files with offline attribute set. (ignored)");
    println!("  /F:file    Reads file list from the specified file.");
    println!("  /C:string  Uses specified string as a literal search string.");
    println!("  /G:file    Gets search strings from the specified file.");
    println!("  /D:dirlist Search a semicolon delimited list of directories.");
    println!("  strings    Text to be searched for.");
    println!("  [drive:][path]filename");
    println!("             Specifies a file or files to search.");
}

// ------------------------------------------------------------
// 构建最终的正则表达式，将锚点和大小写选项融入
// ------------------------------------------------------------
fn build_patterns(raw_patterns: &[String], opts: &Options) -> Vec<Regex> {
    let mut regexes = Vec::new();
    for raw in raw_patterns {
        let mut expr = if opts.literal {
            regex::escape(raw)
        } else {
            raw.clone()
        };

        // 添加锚点
        if opts.exact {
            expr = format!("^{}$", expr);
        } else {
            if opts.line_begin {
                expr = format!("^{}", expr);
            }
            if opts.line_end {
                expr = format!("{}$", expr);
            }
        }

        // 编译正则
        let mut builder = RegexBuilder::new(&expr);
        if opts.ignore_case {
            builder.case_insensitive(true);
        }
        // 允许 . 匹配换行吗？findstr 默认 . 不匹配换行，我们保持单行模式
        match builder.build() {
            Ok(re) => regexes.push(re),
            Err(e) => {
                eprintln!("findstr: Invalid regular expression: {}", e);
                // 继续处理其他模式，但可能出错退出
            }
        }
    }
    regexes
}

// ------------------------------------------------------------
// 收集命令行中直接指定的文件和通配符，并结合 /F, /D, /S 生成最终文件列表
// ------------------------------------------------------------
fn collect_files(patterns: &[String], opts: &Options) -> Vec<PathBuf> {
    let mut files: Vec<PathBuf> = Vec::new();

    // 1. 先处理 /F 指定的文件列表
    for file_list_path in &opts.file_lists {
        if let Ok(f) = fs::File::open(file_list_path) {
            for line in BufReader::new(f).lines().flatten() {
                let trimmed = line.trim();
                if !trimmed.is_empty() {
                    files.push(PathBuf::from(trimmed));
                }
            }
        } else {
            eprintln!("findstr: Cannot open file list {}", file_list_path);
        }
    }

    // 2. 处理命令行直接给出的文件 / 通配符
    for pattern in patterns {
        let p = Path::new(pattern);
        // 如果是存在的目录，则搜索该目录下的所有文件（是否递归由 opts.subdirs 控制）
        if p.is_dir() {
            let dir = p;
            // 搜索目录下的文件
            if opts.subdirs {
                // 递归收集
                collect_recursive(dir, &mut files);
            } else {
                // 只收集该目录下的文件（不递归）
                if let Ok(entries) = fs::read_dir(dir) {
                    for entry in entries.flatten() {
                        let path = entry.path();
                        if path.is_file() {
                            files.push(path);
                        }
                    }
                }
            }
        } else if p.exists() {
            files.push(p.to_path_buf());
        } else if pattern.contains('*') || pattern.contains('?') {
            // 可能是通配符，我们需要在当前目录（或指定基础目录）展开
            // 简单处理：只支持 * 和 ? 在文件名部分，不支持完整路径递归（可结合/S）
            let base = p.parent().unwrap_or(Path::new("."));
            let fname = p.file_name().unwrap().to_str().unwrap();
            if opts.subdirs {
                expand_wildcard_recursive(base, fname, &mut files);
            } else {
                expand_wildcard(base, fname, &mut files);
            }
        } else {
            // 文件不存在且不是通配符，当作普通文件名（可能还未创建），加入列表以便后面报错
            files.push(p.to_path_buf());
        }
    }

    // 3. 处理 /D 目录列表
    if !opts.dir_lists.is_empty() {
        // 如果同时给了文件模式，则在 /D 目录下展开，否则使用 "*"
        let file_pattern = if patterns.is_empty() {
            "*".to_string()
        } else {
            // 使用第一个文件模式作为在这些目录下的搜索模式（简化：只取第一个）
            patterns[0].clone()
        };

        for dir_list in &opts.dir_lists {
            for dir in dir_list.split(';') {
                let dir = dir.trim();
                if dir.is_empty() {
                    continue;
                }
                let path = Path::new(dir);
                if path.is_dir() {
                    if opts.subdirs {
                        expand_wildcard_recursive(path, &file_pattern, &mut files);
                    } else {
                        expand_wildcard(path, &file_pattern, &mut files);
                    }
                } else {
                    eprintln!("findstr: Directory not found: {}", dir);
                }
            }
        }
    }

    files
}

// 递归收集目录下所有文件（不区分通配符，全收）
fn collect_recursive(dir: &Path, out: &mut Vec<PathBuf>) {
    if let Ok(entries) = fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                collect_recursive(&path, out);
            } else if path.is_file() {
                out.push(path);
            }
        }
    }
}

// 在指定目录下展开通配符（不递归）
fn expand_wildcard(dir: &Path, pattern: &str, out: &mut Vec<PathBuf>) {
    if let Ok(entries) = fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_file() {
                let fname = path.file_name().unwrap().to_str().unwrap();
                if wildcard_match(pattern, fname) {
                    out.push(path);
                }
            }
        }
    }
}

// 递归展开通配符
fn expand_wildcard_recursive(dir: &Path, pattern: &str, out: &mut Vec<PathBuf>) {
    expand_wildcard(dir, pattern, out);
    if let Ok(entries) = fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                expand_wildcard_recursive(&path, pattern, out);
            }
        }
    }
}

// 简单的通配符匹配 (支持 * 和 ?)
fn wildcard_match(pattern: &str, text: &str) -> bool {
    let p = pattern.as_bytes();
    let t = text.as_bytes();
    let mut pi = 0;
    let mut ti = 0;
    let mut star_idx = None;
    let mut match_idx = 0;

    while ti < t.len() {
        if pi < p.len() && (p[pi] == b'?' || p[pi] == t[ti]) {
            pi += 1;
            ti += 1;
        } else if pi < p.len() && p[pi] == b'*' {
            star_idx = Some(pi);
            match_idx = ti;
            pi += 1;
        } else if let Some(idx) = star_idx {
            pi = idx + 1;
            match_idx += 1;
            ti = match_idx;
        } else {
            return false;
        }
    }
    while pi < p.len() && p[pi] == b'*' {
        pi += 1;
    }
    pi == p.len()
}

// ------------------------------------------------------------
// 检查文件是否为二进制（包含空字节）
// ------------------------------------------------------------
fn is_binary(path: &Path) -> bool {
    if let Ok(data) = fs::read(path) {
        data.iter().any(|&b| b == 0)
    } else {
        false
    }
}

// ------------------------------------------------------------
// 执行搜索并输出结果
// ------------------------------------------------------------
fn search_files(
    files: &[PathBuf],
    patterns: &[Regex],
    opts: &Options,
    multiple_files: bool,
) {
    // 如果没有文件，从 stdin 读取
    if files.is_empty() {
        let stdin = io::stdin();
        let reader = BufReader::new(stdin);
        for (line_no, line_result) in reader.lines().enumerate() {
            if let Ok(line) = line_result {
                process_line(
                    "",
                    &line,
                    line_no + 1, // 行号从 1 开始
                    patterns,
                    opts,
                    multiple_files,
                );
            }
        }
        return;
    }

    for file_path in files {
        // 处理二进制跳过
        if opts.skip_binary && file_path.exists() && is_binary(file_path) {
            if !opts.filename_only {
                eprintln!("{}: binary file", file_path.display());
            }
            continue;
        }

        // /M 模式：只需要知道文件是否包含任何匹配
        if opts.filename_only {
            if file_contains_match(file_path, patterns, opts) {
                println!("{}", file_path.display());
            }
            continue;
        }

        // 打开文件并逐行搜索
        match fs::File::open(file_path) {
            Ok(f) => {
                let reader = BufReader::new(f);
                let display_name = file_path.display();
                for (line_no, line_result) in reader.lines().enumerate() {
                    if let Ok(line) = line_result {
                        process_line(
                            &display_name.to_string(),
                            &line,
                            line_no + 1,
                            patterns,
                            opts,
                            multiple_files,
                        );
                    }
                }
            }
            Err(e) => {
                eprintln!("findstr: Cannot open {}: {}", file_path.display(), e);
            }
        }
    }
}

// 快速检查文件是否包含匹配（用于 /M）
fn file_contains_match(path: &Path, patterns: &[Regex], opts: &Options) -> bool {
    if let Ok(f) = fs::File::open(path) {
        let reader = BufReader::new(f);
        for line_result in reader.lines() {
            if let Ok(line) = line_result {
                let matched = patterns.iter().any(|re| re.is_match(&line));
                // /V 反转：如果反向，则要求没有任何模式匹配
                let is_match = if opts.invert { !matched } else { matched };
                if is_match {
                    return true;
                }
            }
        }
    }
    false
}

// 处理单行：检查匹配并输出
fn process_line(
    filename: &str,
    line: &str,
    line_no: usize,
    patterns: &[Regex],
    opts: &Options,
    multiple_files: bool,
) {
    let matched = patterns.iter().any(|re| re.is_match(line));
    let is_match = if opts.invert { !matched } else { matched };

    if !is_match {
        return;
    }

    // 构造输出前缀
    let mut prefix = String::new();

    if multiple_files && !opts.filename_only {
        prefix.push_str(&format!("{}:", filename));
    }

    if opts.line_number {
        if !prefix.is_empty() {
            prefix.push(':');
        }
        prefix.push_str(&format!("{}", line_no));
    }

    if opts.offset {
        // 找到第一个匹配并取得其起始字节偏移
        if let Some(first_re) = patterns.iter().find(|re| re.is_match(line)) {
            if let Some(mat) = first_re.find(line) {
                if !prefix.is_empty() {
                    prefix.push(':');
                }
                prefix.push_str(&format!("{}", mat.start()));
            }
        }
    }

    // 输出最终行
    if prefix.is_empty() {
        println!("{}", line);
    } else {
        println!("{}:{}", prefix, line);
    }
}

// ------------------------------------------------------------
// 解析命令行参数
// ------------------------------------------------------------
fn parse_args() -> Result<(Options, Vec<String>, Vec<String>), String> {
    let args: Vec<String> = env::args().skip(1).collect();

    if args.is_empty() {
        print_help();
        std::process::exit(0);
    }

    let mut opts = Options::default();
    let mut positional = Vec::new(); // 非选项参数（可能是模式或文件）

    for arg in args {
        if arg == "/?" {
            print_help();
            std::process::exit(0);
        }

        if arg.starts_with('/') && arg.len() > 1 {
            let opt_str = &arg[1..].to_uppercase();
            // 处理组合选项，如 /I /B 等，但 findstr 允许组合，例如 /BI 等同于 /B /I
            let mut chars = opt_str.chars().peekable();
            while let Some(ch) = chars.next() {
                match ch {
                    '?' => { print_help(); std::process::exit(0); }
                    'B' => opts.line_begin = true,
                    'E' => opts.line_end = true,
                    'L' => opts.literal = true,
                    'R' => opts.regex = true,
                    'S' => opts.subdirs = true,
                    'I' => opts.ignore_case = true,
                    'X' => opts.exact = true,
                    'V' => opts.invert = true,
                    'N' => opts.line_number = true,
                    'M' => opts.filename_only = true,
                    'O' => {
                        // 可能是 /OFF 或 /O 单独
                        // 检查后续字符是否为 "FF"
                        let mut off_buf = String::new();
                        off_buf.push('O');
                        while let Some(&nc) = chars.peek() {
                            if nc == 'F' || nc == 'L' || nc == 'I' || nc == 'N' || nc == 'E' {
                                off_buf.push(nc);
                                chars.next();
                            } else {
                                break;
                            }
                        }
                        if off_buf == "OFF" || off_buf == "OFFLINE" {
                            // 忽略离线选项
                        } else {
                            opts.offset = true;
                        }
                    }
                    'P' => opts.skip_binary = true,
                    'F' => {
                        // /F:file
                        if let Some(':') = chars.peek() {
                            chars.next(); // 跳过 ':'
                            let rest: String = chars.collect();
                            if rest.is_empty() {
                                return Err("Missing file name for /F".into());
                            }
                            opts.file_lists.push(rest);
                            break; // 消耗完毕此参数
                        } else {
                            // /F 单独出现？不合法，忽略或报错
                            return Err("/F requires a file name: /F:file".into());
                        }
                    }
                    'C' => {
                        if let Some(':') = chars.peek() {
                            chars.next();
                            let rest: String = chars.collect();
                            if rest.is_empty() {
                                return Err("Missing string for /C".into());
                            }
                            // 直接当作一个模式，放入 positional 也作为模式，后面我们会集中处理模式
                            // 我们选择将 /C 指定的字符串放入 opts.patterns？可临时存起来
                            // 但由于 positional 还可能有其它模式，我们统一把 /C 当作模式收集到临时向量
                            // 为了简单，我们在这里直接添加到 patterns_vec
                            // 但我们需要一个额外的 Vec<String> 收集模式。
                            // 修改：在 options 外维护一个 patterns_raw 向量
                            // 我们把该值返回给调用者，所以需要调整函数返回值。
                            // 这里我们在外部用一个变量 raw_patterns 收集。
                            // 我们将通过引用参数传递。为保持返回结构清晰，我们单独传递一个 &mut Vec<String>。
                            // 由于函数签名限制，我们修改 parse_args 返回 (Options, Vec<String> patterns, Vec<String> files)，
                            // 其中 patterns 也包含从 /C 和 /G 收集的模式。
                            // 所以下面我们使用一个 patterns_raw 变量，在函数内部定义。
                            return Err("Internal: /C processing moved outside".into());
                        } else {
                            return Err("/C requires a string: /C:string".into());
                        }
                    }
                    'G' => {
                        if let Some(':') = chars.peek() {
                            chars.next();
                            let rest: String = chars.collect();
                            if rest.is_empty() {
                                return Err("Missing file name for /G".into());
                            }
                            opts.pattern_files.push(rest);
                            break;
                        } else {
                            return Err("/G requires a file name: /G:file".into());
                        }
                    }
                    'D' => {
                        if let Some(':') = chars.peek() {
                            chars.next();
                            let rest: String = chars.collect();
                            if rest.is_empty() {
                                return Err("Missing directory list for /D".into());
                            }
                            opts.dir_lists.push(rest);
                            break;
                        } else {
                            return Err("/D requires a directory list: /D:dirlist".into());
                        }
                    }
                    _ => {
                        eprintln!("findstr: Invalid option /{}", ch);
                        print_help();
                        std::process::exit(1);
                    }
                }
            }
        } else {
            // 不以 / 开头，收集到 positional
            positional.push(arg);
        }
    }

    // 现在分离 patterns 和 files
    let mut raw_patterns = Vec::new();
    let mut file_args = Vec::new();

    // 1. 处理 /C 指定的模式（之前无法直接在循环中收集，现在重新解析一遍？）
    // 更好的方式：在参数循环时直接将 /C 和 /G 模式加入 raw_patterns，但需要传递可变引用。
    // 我们可以重构循环，使得参数解析时能直接推入 raw_patterns。
    // 简便方法：重新遍历一遍 args 以提取 /C 和 /G。
    for arg in &env::args().skip(1).collect::<Vec<_>>() {
        if arg.starts_with("/C:") {
            raw_patterns.push(arg[3..].to_string());
        } else if arg.starts_with("/c:") {
            raw_patterns.push(arg[3..].to_string());
        }
        // 也可以处理 /G 文件，后面统一读取
    }

    // 2. 读取 /G 文件中的模式
    for pf in &opts.pattern_files {
        match fs::read_to_string(pf) {
            Ok(content) => {
                for line in content.lines() {
                    let trimmed = line.trim();
                    if !trimmed.is_empty() {
                        raw_patterns.push(trimmed.to_string());
                    }
                }
            }
            Err(e) => eprintln!("findstr: Cannot read pattern file {}: {}", pf, e),
        }
    }

    // 3. 从 positional 中区分模式和文件
    if raw_patterns.is_empty() && !positional.is_empty() {
        // 没有通过 /C 或 /G 指定模式，则第一个 positional 作为模式
        raw_patterns.push(positional.remove(0));
        // 剩下的 positional 中，如果是文件或通配符则放入 file_args，否则也作为模式
        for item in positional {
            let p = Path::new(&item);
            if p.exists() || item.contains('*') || item.contains('?') {
                file_args.push(item);
            } else {
                raw_patterns.push(item);
            }
        }
    } else {
        // 已经有模式，剩余 positional 全部当作文件
        file_args = positional;
    }

    if raw_patterns.is_empty() {
        return Err("No search strings specified.".into());
    }

    Ok((opts, raw_patterns, file_args))
}

// ------------------------------------------------------------
// 主函数
// ------------------------------------------------------------
fn main() {
    let (opts, raw_patterns, file_args) = match parse_args() {
        Ok(v) => v,
        Err(e) => {
            eprintln!("findstr: {}", e);
            print_help();
            std::process::exit(1);
        }
    };

    // 构建正则表达式
    let patterns = build_patterns(&raw_patterns, &opts);
    if patterns.is_empty() {
        eprintln!("findstr: No valid patterns.");
        std::process::exit(1);
    }

    // 收集要搜索的文件
    let files = collect_files(&file_args, &opts);
    let multiple_files = files.len() > 1 || (files.is_empty() && !file_args.is_empty());

    // 执行搜索
    search_files(&files, &patterns, &opts, multiple_files);
}