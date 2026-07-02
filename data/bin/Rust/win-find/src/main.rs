use std::env;
use std::fs;
use std::io::{self, Write};
use std::path::{Path, PathBuf};

// ----------------------------------------------
// 数据结构：属性过滤器（对应 /A 选项）
// ----------------------------------------------
#[derive(Debug, Default)]
struct AttrFilter {
    readonly: Option<bool>,
    hidden: Option<bool>,
}

// ----------------------------------------------
// 数据结构：命令行选项
// ----------------------------------------------
struct Options {
    prompt: bool,
    force: bool,
    recursive: bool,
    quiet: bool,
    attrs: Option<AttrFilter>,
}

impl Default for Options {
    fn default() -> Self {
        Options {
            prompt: false,
            force: false,
            recursive: false,
            quiet: false,
            attrs: None,
        }
    }
}

// ----------------------------------------------
// 打印帮助信息
// ----------------------------------------------
fn print_help() {
    println!("Deletes one or more files.\n");
    println!("ERASE [drive:][path]filename [/P] [/F] [/S] [/Q] [/A[[:]attributes]]\n");
    println!("  [drive:][path]filename  Specifies the file(s) to delete.  Wildcards may be used.");
    println!("  /P          Prompts for confirmation before deleting each file.");
    println!("  /F          Force deleting of read-only files.");
    println!("  /S          Delete specified files from all subdirectories.");
    println!("  /Q          Quiet mode, do not ask if ok to delete on global wildcard.");
    println!("  /A          Selects files to delete based on attributes.");
    println!("              attributes  R  Read-only files");
    println!("                          H  Hidden files");
    println!("                          -  Prefix meaning not");
}

// ----------------------------------------------
// 解析 /A:attributes 字符串
// ----------------------------------------------
fn parse_attrs(s: &str) -> Option<AttrFilter> {
    if s.is_empty() {
        return None;
    }
    let mut readonly = None;
    let mut hidden = None;
    let mut negate = false;

    for ch in s.chars() {
        match ch {
            '-' => negate = true,
            'R' | 'r' => readonly = Some(!negate),
            'H' | 'h' => hidden = Some(!negate),
            'S' | 's' | 'A' | 'a' => {}
            _ => {}
        }
    }
    Some(AttrFilter { readonly, hidden })
}

// ----------------------------------------------
// 检查文件属性是否匹配筛选条件
// ----------------------------------------------
fn attrs_match(path: &Path, filter: &AttrFilter) -> bool {
    if let Ok(meta) = fs::metadata(path) {
        if let Some(require_readonly) = filter.readonly {
            let is_readonly = meta.permissions().readonly();
            if is_readonly != require_readonly {
                return false;
            }
        }
        if let Some(require_hidden) = filter.hidden {
            let is_hidden = path
                .file_name()
                .and_then(|n| n.to_str())
                .map(|n| n.starts_with('.'))
                .unwrap_or(false);
            if is_hidden != require_hidden {
                return false;
            }
        }
        true
    } else {
        false
    }
}

// ----------------------------------------------
// 通配符匹配（支持 * 和 ?）
// ----------------------------------------------
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

// ----------------------------------------------
// 搜索文件：支持递归和非递归
// ----------------------------------------------
fn find_files(pattern: &str, recursive: bool, attrs: &Option<AttrFilter>) -> Vec<PathBuf> {
    let mut results = Vec::new();
    let path = Path::new(pattern);

    let (base_dir, file_pattern) = if pattern.contains('/') {
        let parent = path.parent().unwrap_or(Path::new("."));
        let fname = path
            .file_name()
            .map(|f| f.to_string_lossy().to_string())
            .unwrap_or_else(|| "*".to_string());
        (parent.to_path_buf(), fname)
    } else {
        (PathBuf::from("."), pattern.to_string())
    };

    fn walk(dir: &Path, pattern: &str, attrs: &Option<AttrFilter>, results: &mut Vec<PathBuf>) {
        if let Ok(entries) = fs::read_dir(dir) {
            for entry in entries.flatten() {
                let path = entry.path();
                let file_name = path.file_name().unwrap().to_string_lossy();
                if path.is_dir() {
                    walk(&path, pattern, attrs, results);
                } else if wildcard_match(pattern, &file_name) {
                    if let Some(ref a) = attrs {
                        if !attrs_match(&path, a) {
                            continue;
                        }
                    }
                    results.push(path);
                }
            }
        }
    }

    if recursive {
        walk(&base_dir, &file_pattern, attrs, &mut results);
    } else {
        if let Ok(entries) = fs::read_dir(&base_dir) {
            for entry in entries.flatten() {
                let path = entry.path();
                if path.is_file() {
                    let fname = path.file_name().unwrap().to_string_lossy();
                    if wildcard_match(&file_pattern, &fname) {
                        if let Some(ref a) = attrs {
                            if !attrs_match(&path, a) {
                                continue;
                            }
                        }
                        results.push(path);
                    }
                }
            }
        }
    }
    results
}

// ----------------------------------------------
// 删除单个文件，支持强制删除只读文件
// ----------------------------------------------
fn delete_file(path: &Path, force: bool, prompt: bool) -> Result<(), String> {
    if prompt {
        print!("Delete {}? (Y/N) ", path.display());
        io::stdout().flush().unwrap();
        let mut input = String::new();
        if io::stdin().read_line(&mut input).is_err() {
            return Err("failed to read input".into());
        }
        if !input.trim().eq_ignore_ascii_case("y") {
            return Err("cancelled".into());
        }
    }

    match fs::remove_file(path) {
        Ok(()) => Ok(()),
        Err(e) if force && e.kind() == io::ErrorKind::PermissionDenied => {
            if let Ok(meta) = fs::metadata(path) {
                let mut perms = meta.permissions();
                perms.set_readonly(false);
                if fs::set_permissions(path, perms).is_ok() {
                    return fs::remove_file(path).map_err(|e| format!("{}", e));
                }
            }
            Err(format!("Access is denied - {}", e))
        }
        Err(e) => Err(format!("{}", e)),
    }
}

// ----------------------------------------------
// 主函数
// ----------------------------------------------
fn main() {
    let args: Vec<String> = env::args().skip(1).collect();

    // 没有参数 → 显示帮助
    if args.is_empty() {
        print_help();
        return;
    }

    let mut opts = Options::default();
    let mut patterns: Vec<String> = Vec::new();

    // 解析参数
    for arg in args {
        if arg.starts_with('/') && !arg.starts_with("//") {
            let path = Path::new(&arg);
            // 如果这个路径真实存在，则当作文件/目录而非选项
            if path.exists() {
                patterns.push(arg);
                continue;
            }
            let opt_str = &arg[1..];
            if opt_str.is_empty() {
                continue;
            }
            let mut chars = opt_str.char_indices().peekable();
            let mut consumed = false;
            while let Some((i, ch)) = chars.next() {
                match ch.to_ascii_uppercase() {
                    '?' => {
                        print_help();
                        return;
                    }
                    'P' => opts.prompt = true,
                    'F' => opts.force = true,
                    'S' => opts.recursive = true,
                    'Q' => opts.quiet = true,
                    'A' => {
                        let after_a = &opt_str[i + 1..];
                        let attr_part = if after_a.starts_with(':') {
                            &after_a[1..]
                        } else {
                            after_a
                        };
                        opts.attrs = parse_attrs(attr_part);
                        consumed = true;
                        break;
                    }
                    _ => {
                        // 无效开关 → 显示帮助并退出
                        print_help();
                        return;
                    }
                }
                consumed = true;
            }
            if consumed {
                continue;
            }
        } else {
            patterns.push(arg);
        }
    }

    // 没有提供任何文件模式 → 显示帮助
    if patterns.is_empty() {
        print_help();
        return;
    }

    // 全局确认 (当存在通配符且没有 /Q 或 /P 时)
    if !opts.quiet
        && !opts.prompt
        && patterns.iter().any(|p| p.contains('*') || p.contains('?'))
    {
        print!("Are you sure? (Y/N) ");
        io::stdout().flush().unwrap();
        let mut input = String::new();
        if io::stdin().read_line(&mut input).is_err() || !input.trim().eq_ignore_ascii_case("y") {
            return;
        }
    }

    // 处理每一个模式
    for pattern in &patterns {
        if !pattern.contains('*') && !pattern.contains('?') {
            let p = Path::new(pattern);
            if p.is_dir() {
                eprintln!("Could Not Find {}", pattern);
                continue;
            }
        }

        let files = find_files(pattern, opts.recursive, &opts.attrs);
        if files.is_empty() && !pattern.contains('*') && !pattern.contains('?') {
            eprintln!("Could Not Find {}", pattern);
            continue;
        }
        for file in files {
            match delete_file(&file, opts.force, opts.prompt) {
                Ok(()) => {}
                Err(ref e) if e == "cancelled" => {}
                Err(e) => eprintln!("{}: {}", file.display(), e),
            }
        }
    }
}