use std::env;
use std::fs;
use std::io::{self, Write};
use std::os::unix::fs::MetadataExt;
use std::path::{Path, PathBuf};

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
    DirFirst,
}

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
    bare: bool,
    thousand_sep: bool,
    wide: bool,
    wide_vertical: bool,
    lowercase: bool,
    new_format: bool,
    sort: Option<SortOption>,
    pause: bool,
    show_owner: bool,
    recursive: bool,
    time_field: TimeField,
}

impl Default for Options {
    fn default() -> Self {
        Options {
            attrs: AttrFilter {
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
        // 空属性字符串 -> 不过滤任何属性（显示所有文件，含隐藏、系统等）
        return AttrFilter {
            directory: None,
            readonly: None,
            hidden: None,      // 关键：不按 hidden 过滤
            system: None,
            archive: None,
            symlink: None,
        };
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
        if ch != '-' {
            negate = false;
        }
    }
    filter
}

// ------------------------------------------------
// 解析 /O 排序字符串
// ------------------------------------------------
fn parse_sort_order(s: &str) -> SortOption {
    let mut reverse = false;
    let mut key = SortKey::DirFirst;
    let mut iter = s.chars().peekable();

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
// 过滤器
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
    true
}

// ------------------------------------------------
// 通配符匹配
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
// 收集文件
// ------------------------------------------------
fn collect_files(paths: &[String], opts: &Options) -> Vec<FileEntry> {
    let mut entries = Vec::new();

    let patterns: Vec<String> = if paths.is_empty() {
        vec![".".to_string()]
    } else {
        paths.to_vec()
    };

    for raw in &patterns {
        let p = Path::new(raw.as_str());
        if p.is_dir() {
            let dir_entries = read_dir_recursive(p, true, opts.recursive, &opts.attrs);
            entries.extend(dir_entries);
        } else if p.exists() {
            if let Ok(meta) = fs::metadata(p) {
                let entry = FileEntry {
                    path: p.to_path_buf(),
                    metadata: meta,
                };
                if attrs_match(&entry, &opts.attrs) {
                    entries.push(entry);
                }
            }
        } else if raw.contains('*') || raw.contains('?') {
            let base = p.parent().unwrap_or(Path::new("."));
            let pattern = p.file_name().unwrap().to_str().unwrap();
            let mut collected = if opts.recursive {
                expand_wildcard_recursive(base, pattern, &opts.attrs)
            } else {
                expand_wildcard(base, pattern, &opts.attrs)
            };
            entries.append(&mut collected);
        } else {
            eprintln!("File Not Found: {}", raw);
        }
    }

    sort_entries(&mut entries, &opts.sort);
    entries
}

fn read_dir(dir: &Path, include_dot_dirs: bool, attrs: &AttrFilter) -> Vec<FileEntry> {
    let mut entries = Vec::new();
    if let Ok(dir_entries) = fs::read_dir(dir) {
        for entry in dir_entries.flatten() {
            let path = entry.path();
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
    let mut entries = read_dir(dir, include_dot_dirs, attrs);
    if recursive {
        let subdirs: Vec<PathBuf> = entries
            .iter()
            .filter(|e| e.is_dir())
            .map(|e| e.path.clone())
            .collect();
        for sub in subdirs {
            let child_entries = read_dir_recursive(&sub, false, true, attrs);
            entries.extend(child_entries);
        }
    }
    entries
}

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
// 输出
// ------------------------------------------------
fn display_entries(
    entries: &[FileEntry],
    opts: &Options,
    _start_path: &Path,
) -> io::Result<()> {
    if entries.is_empty() {
        return Ok(());
    }

    // ---- 修改点：/S /B 时输出完整路径 ----
    if opts.bare {
        for e in entries {
            // 如果开启了递归，使用完整路径；否则只使用文件名
            let display = if opts.recursive {
                e.path.display().to_string()
            } else {
                e.name()
            };
            let name = if opts.lowercase {
                display.to_lowercase()
            } else {
                display
            };
            println!("{}", name);
        }
        return Ok(());
    }
    // ---- 修改结束 ----

    if opts.wide || opts.wide_vertical {
        let names: Vec<String> = entries.iter().map(|e| {
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

    let maybe_pause = |handle: &mut io::StdoutLock, lines: &mut u32| -> io::Result<()> {
        if opts.pause && *lines >= term_height - 2 {
            write!(handle, "Press Enter to continue...")?;
            handle.flush()?;
            let mut input = String::new();
            io::stdin().read_line(&mut input)?;
            *lines = 0;
        }
        Ok(())
    };

    for (dir_path, group) in &groups {
        writeln!(handle)?;
        writeln!(handle, " Directory of {}", dir_path.display())?;
        writeln!(handle)?;
        lines_since_pause += 2;

        let header = if opts.show_owner {
            format!("{:>10} {:>8} {:>12} {:>8} ", "Date", "Time", "Size", "Owner")
        } else {
            format!("{:>10} {:>8} {:>12} ", "Date", "Time", "Size")
        };
        write!(handle, "{}", header)?;
        writeln!(handle, "Name")?;
        lines_since_pause += 1;

        for entry in group {
            maybe_pause(&mut handle, &mut lines_since_pause)?;

            let dt = entry.time_field(opts.time_field).unwrap_or_else(|| {
                DateTime::from_timestamp(0, 0)
                    .map(|dt| dt.naive_local())
                    .unwrap_or_else(|| NaiveDateTime::UNIX_EPOCH)
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

    maybe_pause(&mut handle, &mut lines_since_pause)?;
    writeln!(handle)?;
    let bytes_str = if opts.thousand_sep {
        format_num(total_bytes)
    } else {
        total_bytes.to_string()
    };
    writeln!(handle, "               {} File(s)     {} bytes", total_files, bytes_str)?;
    writeln!(handle, "               {} Dir(s)", total_dirs)?;
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

mod term_size {
    pub fn width() -> Option<usize> {
        std::env::var("COLUMNS").ok().and_then(|s| s.parse().ok())
    }
    pub fn height() -> Option<usize> {
        std::env::var("LINES").ok().and_then(|s| s.parse().ok())
    }
}

// ------------------------------------------------
// 解析参数
// ------------------------------------------------
fn parse_args() -> (Options, Vec<String>) {
    let args: Vec<String> = env::args().skip(1).collect();
    if args.is_empty() {
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

    (opts, paths)
}

// ------------------------------------------------
// 主函数
// ------------------------------------------------
fn main() {
    let (opts, paths) = parse_args();
    let entries = collect_files(&paths, &opts);
    if let Err(e) = display_entries(&entries, &opts, Path::new(".")) {
        eprintln!("Error: {}", e);
    }
}