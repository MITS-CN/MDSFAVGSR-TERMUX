use std::env;
use std::fs;
use std::io::{self, BufRead, Write};
use std::os::unix::fs::MetadataExt; // for uid
use std::path::{Path, PathBuf};
use std::time::UNIX_EPOCH;

use chrono::{DateTime, Local, NaiveDateTime};

// ------------------------------------------------
// 属性过滤器 (对应 /A 选项)
// ------------------------------------------------
#[derive(Debug, Default)]
struct AttrFilter {
    directory: Option<bool>, // D
    readonly: Option<bool>,  // R
    hidden: Option<bool>,    // H
    system: Option<bool>,    // S (Linux 下忽略)
    archive: Option<bool>,   // A (忽略)
    symlink: Option<bool>,   // L
}

// ------------------------------------------------
// 排序键
// ------------------------------------------------
#[derive(Debug, Clone, Copy, PartialEq)]
enum SortKey {
    Name,
    Size,
    Extension,
    DateTime,
    DirFirst, // 目录优先，可与其他键组合
}

// 排序选项
#[derive(Debug)]
struct SortOption {
    key: SortKey,
    reverse: bool,
}

// ------------------------------------------------
// 时间字段选择
// ------------------------------------------------
#[derive(Debug, Clone, Copy)]
enum TimeField {
    Creation,
    LastAccess,
    LastWrite,
}

// ------------------------------------------------
// 命令行选项
// ------------------------------------------------
struct Options {
    attrs: AttrFilter,
    bare: bool,          // /B
    thousand_sep: bool,  // /C
    wide: bool,          // /W
    wide_vertical: bool, // /D (垂直排序)
    lowercase: bool,     // /L
    new_format: bool,    // /N (默认)
    sort: Option<SortOption>,
    pause: bool,         // /P
    show_owner: bool,    // /Q
    recursive: bool,     // /S
    time_field: TimeField, // /T
    // /X /R 忽略
}

impl Default for Options {
    fn default() -> Self {
        Options {
            attrs: AttrFilter {
                // 默认不显示隐藏文件 (点开头)
                hidden: Some(false),
                ..Default::default()
            },
            bare: false,
            thousand_sep: false,
            wide: false,
            wide_vertical: false,
            lowercase: false,
            new_format: true,
            sort: Some(SortOption {
                key: SortKey::DirFirst,
                reverse: false,
            }),
            pause: false,
            show_owner: false,
            recursive: false,
            time_field: TimeField::LastWrite,
        }
    }
}

// ------------------------------------------------
// 帮助信息
// ------------------------------------------------
fn print_help() {
    println!("Displays a list of files and subdirectories in a directory.\n");
    println!("DIR [drive:][path][filename] [/A[[:]attributes]] [/B] [/C] [/D] [/L] [/N]");
    println!("    [/O[[:]sortorder]] [/P] [/Q] [/R] [/S] [/T[[:]timefield]] [/W] [/X] [/4]\n");
    println!("  [drive:][path][filename]  Specifies drive, directory, and/or files to list.");
    println!("  /A          Displays files with specified attributes.");
    println!("              attributes   D  Directories                R  Read-only files");
    println!("                           H  Hidden files               S  System files");
    println!("                           A  Files ready for archiving  L  Symlinks");
    println!("                           -  Prefix meaning not");
    println!("  /B          Uses bare format (no heading information or summary).");
    println!("  /C          Displays the thousand separator in file sizes.");
    println!("  /D          Same as wide but files are list sorted by column.");
    println!("  /L          Uses lowercase.");
    println!("  /N          New long list format where filenames are on the far right.");
    println!("  /O          List by files in sorted order.");
    println!("              sortorder    N  By name (alphabetic)       S  By size (smallest first)");
    println!("                           E  By extension (alphabetic)  D  By date/time (oldest first)");
    println!("                           G  Group directories first    -  Prefix to reverse order");
    println!("  /P          Pauses after each screenful of information.");
    println!("  /Q          Displays the owner of the file.");
    println!("  /R          Display alternate data streams (ignored).");
    println!("  /S          Displays files in specified directory and all subdirectories.");
    println!("  /T          Controls which time field displayed or used for sorting.");
    println!("              timefield   C  Creation");
    println!("                           A  Last Access");
    println!("                           W  Last Written (default)");
    println!("  /W          Uses wide list format.");
    println!("  /X          Displays short names for non-8dot3 file names (ignored).");
    println!("  /4          Displays four-digit years (ignored, always 4-digit).");
}

// ------------------------------------------------
// 解析 /A 属性字符串
// ------------------------------------------------
fn parse_attrs(s: &str) -> AttrFilter {
    let mut filter = AttrFilter::default();
    if s.is_empty() {
        // 没有属性字符串，表示显示所有 (包括隐藏/系统)
        return filter; // 全为 None
    }
    let mut negate = false;
    for ch in s.chars() {
        match ch {
            '-' => negate = true,
            'D' | 'd' => filter.directory = Some(!negate),
            'R' | 'r' => filter.readonly = Some(!negate),
            'H' | 'h' => filter.hidden = Some(!negate),
            'S' | 's' => filter.system = Some(!negate),
            'A' | 'a' => filter.archive = Some(!negate),
            'L' | 'l' => filter.symlink = Some(!negate),
            _ => {}
        }
        if !negate { /* already false */ } else {
            // 只要不是 '-'，属性设置完后应重置
            if ch != '-' {
                negate = false;
            }
        }
    }
    filter
}

// ------------------------------------------------
// 解析 /O 排序字符串
// ------------------------------------------------
fn parse_sort_order(s: &str) -> SortOption {
    let mut reverse = false;
    let mut key = SortKey::DirFirst; // 默认为目录优先? Windows 默认 /ON
    let mut iter = s.chars().peekable();

    // 查找反转前缀
    if let Some(&first) = iter.peek() {
        if first == '-' {
            reverse = true;
            iter.next();
        }
    }

    for ch in iter {
        match ch.to_ascii_uppercase() {
            'N' => key = SortKey::Name,
            'S' => key = SortKey::Size,
            'E' => key = SortKey::Extension,
            'D' => key = SortKey::DateTime,
            'G' => key = SortKey::DirFirst,
            _ => {}
        }
    }
    SortOption { key, reverse }
}

// ------------------------------------------------
// 解析 /T 时间字段
// ------------------------------------------------
fn parse_time_field(s: &str) -> TimeField {
    match s.to_ascii_uppercase().as_str() {
        "C" => TimeField::Creation,
        "A" => TimeField::LastAccess,
        _ => TimeField::LastWrite,
    }
}

// ------------------------------------------------
// 文件条目
// ------------------------------------------------
struct FileEntry {
    path: PathBuf,
    metadata: fs::Metadata,
}

impl FileEntry {
    fn name(&self) -> String {
        self.path
            .file_name()
            .unwrap_or_default()
            .to_string_lossy()
            .to_string()
    }

    fn extension(&self) -> String {
        self.path
            .extension()
            .unwrap_or_default()
            .to_string_lossy()
            .to_string()
    }

    fn is_dir(&self) -> bool {
        self.metadata.is_dir()
    }

    fn is_symlink(&self) -> bool {
        self.metadata.file_type().is_symlink()
    }

    fn is_hidden(&self) -> bool {
        self.name().starts_with('.')
    }

    fn is_readonly(&self) -> bool {
        self.metadata.permissions().readonly()
    }

    fn size(&self) -> u64 {
        self.metadata.len()
    }

    fn modified(&self) -> Option<NaiveDateTime> {
        system_time_to_naive(self.metadata.modified().ok()?)
    }

    fn created(&self) -> Option<NaiveDateTime> {
        system_time_to_naive(self.metadata.created().ok()?)
    }

    fn accessed(&self) -> Option<NaiveDateTime> {
        system_time_to_naive(self.metadata.accessed().ok()?)
    }

    fn time_field(&self, tf: TimeField) -> Option<NaiveDateTime> {
        match tf {
            TimeField::Creation => self.created(),
            TimeField::LastAccess => self.accessed(),
            TimeField::LastWrite => self.modified(),
        }
    }

    fn uid(&self) -> u32 {
        self.metadata.uid()
    }
}

fn system_time_to_naive(st: std::time::SystemTime) -> Option<NaiveDateTime> {
    let dt: DateTime<Local> = st.into();
    Some(dt.naive_local())
}

// ------------------------------------------------
// 过滤器：检查文件是否匹配属性筛选
// ------------------------------------------------
fn attrs_match(entry: &FileEntry, filter: &AttrFilter) -> bool {
    if let Some(v) = filter.directory {
        if entry.is_dir() != v {
            return false;
        }
    }
    if let Some(v) = filter.readonly {
        if entry.is_readonly() != v {
            return false;
        }
    }
    if let Some(v) = filter.hidden {
        if entry.is_hidden() != v {
            return false;
        }
    }
    if let Some(v) = filter.symlink {
        if entry.is_symlink() != v {
            return false;
        }
    }
    // system / archive 忽略
    true
}

// ------------------------------------------------
// 通配符匹配 (支持 * 和 ?)
// ------------------------------------------------
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

// ------------------------------------------------
// 收集文件 (支持路径、通配符、递归)
// ------------------------------------------------
fn collect_files(
    paths: &[String],
    opts: &Options,
) -> Vec<FileEntry> {
    let mut entries = Vec::new();
    let patterns: Vec<&String> = if paths.is_empty() {
        vec![&".".to_string()] // 处理默认当前目录
    } else {
        paths.iter().collect()
    };

    for raw in patterns {
        let p = Path::new(raw);
        // 情况1: 是存在的目录 → 列出内容 (递归可选)
        if p.is_dir() {
            let dir_entries = read_dir_recursive(p, true, opts.recursive, &opts.attrs);
            entries.extend(dir_entries);
        }
        // 情况2: 文件存在 → 直接加入
        else if p.exists() {
            if let Ok(meta) = fs::metadata(p) {
                let entry = FileEntry {
                    path: p.to_path_buf(),
                    metadata: meta,
                };
                if attrs_match(&entry, &opts.attrs) {
                    entries.push(entry);
                }
            }
        }
        // 情况3: 包含通配符 → 在对应的基础目录展开
        else if raw.contains('*') || raw.contains('?') {
            let base = p.parent().unwrap_or(Path::new("."));
            let pattern = p.file_name().unwrap().to_str().unwrap();
            let mut collected = if opts.recursive {
                expand_wildcard_recursive(base, pattern, &opts.attrs)
            } else {
                expand_wildcard(base, pattern, &opts.attrs)
            };
            entries.append(&mut collected);
        }
        // 情况4: 文件不存在且无通配符 → 报错
        else {
            eprintln!("File Not Found: {}", raw);
        }
    }

    // 排序
    sort_entries(&mut entries, &opts.sort);
    entries
}

// 读取目录 (非递归，应用属性过滤)
fn read_dir(dir: &Path, include_dot_dirs: bool, attrs: &AttrFilter) -> Vec<FileEntry> {
    let mut entries = Vec::new();
    if let Ok(dir_entries) = fs::read_dir(dir) {
        for entry in dir_entries.flatten() {
            let path = entry.path();
            // 跳过 . 和 .. (除非需要)
            let fname = path.file_name().unwrap_or_default().to_string_lossy();
            if fname == "." || fname == ".." {
                if include_dot_dirs {
                    if let Ok(meta) = fs::symlink_metadata(&path) {
                        let fe = FileEntry { path, metadata: meta };
                        if attrs_match(&fe, attrs) {
                            entries.push(fe);
                        }
                    }
                }
                continue;
            }
            if let Ok(meta) = fs::symlink_metadata(&path) {
                let fe = FileEntry { path, metadata: meta };
                if attrs_match(&fe, attrs) {
                    entries.push(fe);
                }
            }
        }
    }
    entries
}

fn read_dir_recursive(
    dir: &Path,
    include_dot_dirs: bool,
    recursive: bool,
    attrs: &AttrFilter,
) -> Vec<FileEntry> {
    let mut all = Vec::new();
    if recursive {
    }
    // 非递归或递归：我们先读取当前目录的文件和子目录列表
    let mut entries = read_dir(dir, include_dot_dirs, attrs);
    // 递归时，对于每个子目录，递归收集并合并
    if recursive {
        let subdirs: Vec<PathBuf> = entries.iter().filter(|e| e.is_dir()).map(|e| e.path.clone()).collect();
        for sub in subdirs {
            let child_entries = read_dir_recursive(&sub, false, true, attrs); // 子目录不显示 . ..
            entries.extend(child_entries);
        }
    }
    entries
}

// 在目录下展开通配符（非递归）
fn expand_wildcard(dir: &Path, pattern: &str, attrs: &AttrFilter) -> Vec<FileEntry> {
    let mut results = Vec::new();
    if let Ok(entries) = fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            let fname = path.file_name().unwrap_or_default().to_string_lossy();
            if fname == "." || fname == ".." {
                continue;
            }
            if wildcard_match(pattern, &fname) {
                if let Ok(meta) = fs::symlink_metadata(&path) {
                    let fe = FileEntry { path, metadata: meta };
                    if attrs_match(&fe, attrs) {
                        results.push(fe);
                    }
                }
            }
        }
    }
    results
}

fn expand_wildcard_recursive(dir: &Path, pattern: &str, attrs: &AttrFilter) -> Vec<FileEntry> {
    let mut results = expand_wildcard(dir, pattern, attrs);
    if let Ok(entries) = fs::read_dir(dir) {
        for entry in entries.flatten() {
            let path = entry.path();
            if path.is_dir() {
                let fname = path.file_name().unwrap_or_default().to_string_lossy();
                if fname == "." || fname == ".." {
                    continue;
                }
                results.extend(expand_wildcard_recursive(&path, pattern, attrs));
            }
        }
    }
    results
}

// ------------------------------------------------
// 排序
// ------------------------------------------------
fn sort_entries(entries: &mut Vec<FileEntry>, sort_opt: &Option<SortOption>) {
    if let Some(so) = sort_opt {
        let rev = so.reverse;
        match so.key {
            SortKey::DirFirst => {
                entries.sort_by(|a, b| {
                    match (a.is_dir(), b.is_dir()) {
                        (true, false) => std::cmp::Ordering::Less,
                        (false, true) => std::cmp::Ordering::Greater,
                        _ => a.name().cmp(&b.name()),
                    }
                });
            }
            SortKey::Name => {
                entries.sort_by(|a, b| a.name().cmp(&b.name()));
            }
            SortKey::Size => {
                entries.sort_by(|a, b| a.size().cmp(&b.size()));
            }
            SortKey::Extension => {
                entries.sort_by(|a, b| a.extension().cmp(&b.extension()));
            }
            SortKey::DateTime => {
                entries.sort_by(|a, b| {
                    let ta = a.time_field(TimeField::LastWrite);
                    let tb = b.time_field(TimeField::LastWrite);
                    ta.cmp(&tb)
                });
            }
        }
        if rev {
            entries.reverse();
        }
    }
}

// ------------------------------------------------
// 格式化输出
// ------------------------------------------------
fn display_entries(
    entries: &[FileEntry],
    opts: &Options,
    start_path: &Path,
) -> io::Result<()> {
    if entries.is_empty() {
        return Ok(());
    }

    // 如果 /B 模式，简单输出文件名
    if opts.bare {
        for e in entries {
            let name = if opts.lowercase {
                e.name().to_lowercase()
            } else {
                e.name()
            };
            println!("{}", name);
        }
        return Ok(());
    }

    // 宽列表或垂直宽列表
    if opts.wide || opts.wide_vertical {
        let mut names: Vec<String> = entries.iter().map(|e| {
            let n = if opts.lowercase { e.name().to_lowercase() } else { e.name() };
            if e.is_dir() { format!("[{}]", n) } else { n }
        }).collect();
        if opts.wide {
            print_wide_horizontal(&names);
        } else {
            print_wide_vertical(&names);
        }
        return Ok(());
    }

    // 详细列表格式 (带标题和摘要)
    // 按目录分组输出 (便于 /S 时显示子目录标题)
    let mut groups: Vec<(PathBuf, Vec<&FileEntry>)> = Vec::new();
    for e in entries {
        let parent = e.path.parent().unwrap_or(Path::new(".")).to_path_buf();
        if let Some(last) = groups.last_mut() {
            if last.0 == parent {
                last.1.push(e);
                continue;
            }
        }
        groups.push((parent, vec![e]));
    }

    let mut total_files = 0u64;
    let mut total_dirs = 0u64;
    let mut total_bytes = 0u64;
    let stdout = io::stdout();
    let mut handle = stdout.lock();
    let mut lines_since_pause = 0u32;
    let term_height = term_size::height().unwrap_or(24) as u32;

    // 定义暂停函数
    let mut maybe_pause = |handle: &mut io::StdoutLock, lines: &mut u32| -> io::Result<()> {
        if opts.pause && *lines >= term_height - 2 {
            write!(handle, "Press Enter to continue...")?;
            handle.flush()?;
            let mut input = String::new();
            io::stdin().read_line(&mut input)?;
            *lines = 0;
        }
        Ok(())
    };

    // 对每个目录输出标题和文件列表
    for (dir_path, group) in &groups {
        // 打印目录标题
        writeln!(handle)?;
        writeln!(handle, " Directory of {}", dir_path.display())?;
        writeln!(handle)?;
        lines_since_pause += 2;

        // 打印表头
        let header = if opts.show_owner {
            format!("{:>10} {:>8} {:>12} {:>19} ", "Date", "Time", "Size", "Owner")
        } else {
            format!("{:>10} {:>8} {:>12} ", "Date", "Time", "Size")
        };
        write!(handle, "{}", header)?;
        writeln!(handle, "Name")?;
        lines_since_pause += 1;

        for entry in group {
            maybe_pause(&mut handle, &mut lines_since_pause)?;

            let dt = entry.time_field(opts.time_field).unwrap_or_else(|| {
                NaiveDateTime::from_timestamp_opt(0, 0).unwrap()
            });
            let date_str = dt.format("%Y-%m-%d").to_string();
            let time_str = dt.format("%H:%M").to_string();

            let size_str = if entry.is_dir() {
                "<DIR>".to_string()
            } else {
                if opts.thousand_sep {
                    format_num(entry.size())
                } else {
                    entry.size().to_string()
                }
            };

            let owner_str = if opts.show_owner {
                format!("{:>8}", entry.uid())
            } else {
                String::new()
            };

            let name = if opts.lowercase {
                entry.name().to_lowercase()
            } else {
                entry.name()
            };

            if opts.show_owner {
                writeln!(handle, "{:>10} {:>8} {:>12} {} {}", date_str, time_str, size_str, owner_str, name)?;
            } else {
                writeln!(handle, "{:>10} {:>8} {:>12} {}", date_str, time_str, size_str, name)?;
            }
            lines_since_pause += 1;

            if entry.is_dir() {
                total_dirs += 1;
            } else {
                total_files += 1;
                total_bytes += entry.size();
            }
        }
    }

    // 输出统计信息
    maybe_pause(&mut handle, &mut lines_since_pause)?;
    writeln!(handle)?;
    let bytes_str = if opts.thousand_sep {
        format_num(total_bytes)
    } else {
        total_bytes.to_string()
    };
    writeln!(handle, "               {} File(s)     {} bytes", total_files, bytes_str)?;
    writeln!(handle, "               {} Dir(s)", total_dirs)?;
    // 空闲空间不显示
    lines_since_pause += 2;
    Ok(())
}

fn format_num(n: u64) -> String {
    let s = n.to_string();
    let mut result = String::new();
    for (i, c) in s.chars().rev().enumerate() {
        if i != 0 && i % 3 == 0 {
            result.push(',');
        }
        result.push(c);
    }
    result.chars().rev().collect()
}

// 简单水平多列输出 (类似 ls -x)
fn print_wide_horizontal(names: &[String]) {
    if names.is_empty() {
        return;
    }
    let width = term_size::width().unwrap_or(80) as usize;
    let max_len = names.iter().map(|n| n.len()).max().unwrap_or(1);
    let cols = (width / (max_len + 2)).max(1);
    let rows = (names.len() + cols - 1) / cols;

    for r in 0..rows {
        for c in 0..cols {
            let idx = r + c * rows;
            if idx < names.len() {
                print!("{:<width$}", names[idx], width = max_len + 2);
            }
        }
        println!();
    }
}

// 垂直多列输出 (类似 ls -C)
fn print_wide_vertical(names: &[String]) {
    if names.is_empty() {
        return;
    }
    let width = term_size::width().unwrap_or(80) as usize;
    let max_len = names.iter().map(|n| n.len()).max().unwrap_or(1);
    let cols = (width / (max_len + 2)).max(1);
    let rows = (names.len() + cols - 1) / cols;

    for r in 0..rows {
        for c in 0..cols {
            let idx = c * rows + r;
            if idx < names.len() {
                print!("{:<width$}", names[idx], width = max_len + 2);
            }
        }
        println!();
    }
}

// 终端尺寸模块 (简易，避免额外依赖)
mod term_size {
    pub fn width() -> Option<usize> {
        std::env::var("COLUMNS").ok().and_then(|s| s.parse().ok()).or_else(|| {
            // 尝试通过 ioctl 获取，但不引入 libc，返回 None
            None
        })
    }
    pub fn height() -> Option<usize> {
        std::env::var("LINES").ok().and_then(|s| s.parse().ok())
    }
}

// ------------------------------------------------
// 解析命令行参数
// ------------------------------------------------
fn parse_args() -> (Options, Vec<String>) {
    let args: Vec<String> = env::args().skip(1).collect();
    if args.is_empty() {
        // 无参数时显示当前目录，使用默认选项
        return (Options::default(), vec![".".to_string()]);
    }

    let mut opts = Options::default();
    let mut paths = Vec::new();

    for arg in args {
        if arg == "/?" {
            print_help();
            std::process::exit(0);
        }
        if arg.starts_with('/') && arg.len() > 1 {
            let opt_str = &arg[1..];
            let mut chars = opt_str.char_indices().peekable();
            let mut consumed = false;
            while let Some((i, ch)) = chars.next() {
                match ch.to_ascii_uppercase() {
                    '?' => { print_help(); std::process::exit(0); }
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
                    'B' => opts.bare = true,
                    'C' => opts.thousand_sep = true,
                    'D' => { opts.wide_vertical = true; opts.wide = true; }
                    'L' => opts.lowercase = true,
                    'N' => opts.new_format = true,
                    'O' => {
                        let after_o = &opt_str[i + 1..];
                        let sort_part = if after_o.starts_with(':') {
                            &after_o[1..]
                        } else {
                            after_o
                        };
                        opts.sort = Some(parse_sort_order(sort_part));
                        consumed = true;
                        break;
                    }
                    'P' => opts.pause = true,
                    'Q' => opts.show_owner = true,
                    'S' => opts.recursive = true,
                    'T' => {
                        let after_t = &opt_str[i + 1..];
                        let time_part = if after_t.starts_with(':') {
                            &after_t[1..]
                        } else {
                            after_t
                        };
                        opts.time_field = parse_time_field(time_part);
                        consumed = true;
                        break;
                    }
                    'W' => opts.wide = true,
                    'X' | 'R' | '4' => { /* ignored */ }
                    _ => {
                        eprintln!("Invalid switch - /{}", ch);
                        print_help();
                        std::process::exit(1);
                    }
                }
                consumed = true;
            }
            if consumed {
                continue;
            }
        } else {
            paths.push(arg);
        }
    }

    if paths.is_empty() {
        paths.push(".".to_string());
    }

    // 如果使用了宽格式，则关闭 /N (不再详细)
    if opts.wide {
        opts.bare = false;
    }

    (opts, paths)
}

// ------------------------------------------------
// 主函数
// ------------------------------------------------
fn main() {
    let (opts, paths) = parse_args();
    let entries = collect_files(&paths, &opts);

    // 对于详细输出且非递归，开头显示卷信息 (可省略)
    if !opts.bare && !opts.wide {
        // 模拟卷标 (无实际意义)
        // 不显示卷序列号
    }

    if let Err(e) = display_entries(&entries, &opts, Path::new(".")) {
        eprintln!("Error: {}", e);
    }
}