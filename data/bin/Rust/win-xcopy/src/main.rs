use std::env;
use std::fs;
use std::io::{self, Write};
use std::os::unix::fs::PermissionsExt;
use std::path::{Path, PathBuf};
use std::process;

use chrono::NaiveDate;
use glob::glob;

macro_rules! verbose {
    ($quiet:expr, $($arg:tt)*) => {
        if !$quiet {
            eprintln!($($arg)*);
        }
    };
}

fn main() {
    let args: Vec<String> = env::args().skip(1).collect();
    if args.is_empty() || args.iter().any(|a| a == "/?" || a == "-?" || a == "--help") {
        print_usage();
        process::exit(0);
    }

    let mut params = Params::default();
    let (source, destination) = parse_args(&args, &mut params);

    if let Err(e) = run(source, destination, &params) {
        eprintln!("Error: {}", e);
        process::exit(1);
    }
}

#[derive(Debug, Default)]
struct Params {
    subdirs: bool,          // /S
    include_empty: bool,    // /E
    assume_dir: Option<bool>, // Some(true) for /I, Some(false) for /-I
    yes: Option<bool>,      // Some(true) for /Y, Some(false) for /-Y
    overwrite_readonly: bool, // /R
    include_hidden: bool,   // /H
    preserve_attrs: bool,   // /K
    date_filter: Option<DateFilter>,
    prompt_each: bool,      // /P
    wait_key: bool,         // /W
    continue_on_error: bool,// /C
    verify: bool,           // /V
    full_display: bool,     // /F
    quiet: bool,            // /Q
    list_only: bool,        // /L
    dirs_only: bool,        // /T
    update_only: bool,      // /U
    exclude_files: Vec<String>,
}

#[derive(Debug)]
enum DateFilter {
    Newer,                          // /D without date
    OnOrAfter(NaiveDate),           // /D:date
}

fn parse_args(args: &[String], params: &mut Params) -> (String, Option<String>) {
    let mut source = None;
    let mut destination = None;
    let mut i = 0;

    while i < args.len() {
        let arg = &args[i];
        if arg.starts_with('/') || arg.starts_with('-') {
            let mut switch = arg[1..].to_uppercase();
            let value = if let Some(pos) = switch.find(':') {
                let val = switch[pos + 1..].to_string();
                switch = switch[..pos].to_string();
                Some(val)
            } else {
                None
            };

            match switch.as_str() {
                "S" => params.subdirs = true,
                "E" => {
                    params.subdirs = true;
                    params.include_empty = true;
                }
                "I" => params.assume_dir = Some(true),
                "-I" => params.assume_dir = Some(false),
                "Y" => params.yes = Some(true),
                "-Y" => params.yes = Some(false),
                "R" => params.overwrite_readonly = true,
                "H" => params.include_hidden = true,
                "K" => params.preserve_attrs = true,
                "D" => {
                    if let Some(date_str) = value {
                        // Parse m-d-y or m/d/y
                        let date = parse_date(&date_str)
                            .expect("Invalid date format for /D. Use m-d-y or m/d/y");
                        params.date_filter = Some(DateFilter::OnOrAfter(date));
                    } else {
                        params.date_filter = Some(DateFilter::Newer);
                    }
                }
                "P" => params.prompt_each = true,
                "W" => params.wait_key = true,
                "C" => params.continue_on_error = true,
                "V" => params.verify = true,
                "F" => params.full_display = true,
                "Q" => params.quiet = true,
                "L" => params.list_only = true,
                "T" => params.dirs_only = true,
                "U" => params.update_only = true,
                "EXCLUDE" => {
                    if let Some(files) = value {
                        params.exclude_files = files.split('+').map(|s| s.to_string()).collect();
                    } else {
                        eprintln!("Warning: /EXCLUDE requires file list.");
                    }
                }
                "A" | "M" => verbose!(params.quiet, "Warning: /A and /M are not supported on this platform."),
                "G" | "O" | "X" | "B" | "N" | "Z" | "J" | "COMPRESS" | "SPARSE" | "NOCLONE" => {
                    verbose!(params.quiet, "Warning: /{} is not supported on this platform.", switch);
                }
                _ => eprintln!("Warning: unknown switch /{}", switch),
            }
        } else {
            if source.is_none() {
                source = Some(arg.clone());
            } else if destination.is_none() {
                destination = Some(arg.clone());
            } else {
                eprintln!("Warning: extra argument ignored: {}", arg);
            }
        }
        i += 1;
    }

    let source = source.expect("Source must be specified");
    (source, destination)
}

fn parse_date(s: &str) -> Result<NaiveDate, chrono::ParseError> {
    // Try m-d-y, m/d/y, m-d-yyyy, etc.
    let normalized = s.replace('/', "-");
    let formats = ["%m-%d-%y", "%m-%d-%Y", "%m-%d-%y", "%m/%d/%Y", "%m/%d/%y"];
    for fmt in &formats {
        if let Ok(d) = NaiveDate::parse_from_str(&normalized, fmt) {
            return Ok(d);
        }
    }
    // try chrono's default
    NaiveDate::parse_from_str(&normalized, "%m-%d-%Y")
        .or_else(|_| NaiveDate::parse_from_str(&normalized, "%m-%d-%y"))
}

fn run(source: String, destination: Option<String>, params: &Params) -> io::Result<()> {
    if params.wait_key {
        eprint!("Press any key to continue . . . ");
        io::stdout().flush().unwrap();
        let _ = io::stdin().read_line(&mut String::new());
    }

    // Load exclude strings
    let mut exclude_patterns = Vec::new();
    for f in &params.exclude_files {
        let content = fs::read_to_string(f)?;
        for line in content.lines() {
            let trimmed = line.trim();
            if !trimmed.is_empty() {
                exclude_patterns.push(trimmed.to_string());
            }
        }
    }

    // Determine if source has wildcards
    let has_wildcards = source.contains('*') || source.contains('?');

    // Collect entries from source glob
    let mut entries: Vec<PathBuf> = Vec::new();
    if has_wildcards {
        for entry in glob(&source).unwrap() {
            match entry {
                Ok(path) => entries.push(path),
                Err(e) => {
                    if params.continue_on_error {
                        eprintln!("Error globbing: {}", e);
                    } else {
                        return Err(io::Error::new(io::ErrorKind::Other, e.to_string()));
                    }
                }
            }
        }
    } else {
        let p = PathBuf::from(&source);
        if p.exists() {
            entries.push(p);
        } else {
            eprintln!("Source not found: {}", source);
            process::exit(1);
        }
    }

    if entries.is_empty() {
        println!("No files found.");
        return Ok(());
    }

    // Determine destination: if None, use current directory
    let dest_base = match destination {
        Some(d) => PathBuf::from(d),
        None => PathBuf::from("."),
    };

    // Determine if target is directory
    let multiple_sources = entries.len() > 1 || has_wildcards || params.subdirs;
    let dest_is_dir = if dest_base.exists() {
        dest_base.is_dir()
    } else {
        // Not exist
        if let Some(assume) = params.assume_dir {
            assume
        } else if multiple_sources {
            // If multiple sources, assume directory
            true
        } else {
            // Prompt user
            prompt_dest_type(&dest_base)?
        }
    };

    let target_dir: PathBuf;
    if dest_is_dir {
        target_dir = dest_base.clone();
        // Ensure target directory exists
        fs::create_dir_all(&target_dir)?;
    } else {
        // Single file target
        if let Some(parent) = dest_base.parent() {
            fs::create_dir_all(parent)?;
        }
        target_dir = dest_base.parent().unwrap_or(Path::new(".")).to_path_buf();
    }

    // Expand directories if /S or /E
    let mut all_files: Vec<(PathBuf, PathBuf)> = Vec::new(); // (source, relative path for directory copy)
    for entry in &entries {
        if entry.is_dir() {
            if !params.subdirs {
                // Without /S, skip directories
                continue;
            }
            // Walk dir
            let base = entry.clone();
            let walker = walk_dir(&base, params.include_hidden);
            for path in walker {
                let rel = path.strip_prefix(&base).unwrap().to_path_buf();
                all_files.push((path, rel));
            }
            // If /E and base is empty, it will still be listed by walk_dir? We need to handle empty dir
            if params.include_empty {
                // Ensure directory itself is added if empty? The dir will be created via /T or when copying files.
                // We'll just push the base with rel "." to ensure it is created.
                if base.read_dir().map(|mut d| d.next().is_none()).unwrap_or(false) {
                    all_files.push((base.clone(), PathBuf::from(".")));
                }
            }
        } else {
            all_files.push((entry.clone(), entry.file_name().unwrap().into()));
        }
    }

    // Apply filters: exclude, date, update-only
    all_files.retain(|(src_path, rel)| {
        // Exclude
        let abs = src_path.canonicalize().unwrap_or_else(|_| src_path.clone());
        let abs_str = abs.to_string_lossy();
        if exclude_patterns.iter().any(|p| abs_str.contains(p.as_str())) {
            return false;
        }
        // Hidden filter
        if !params.include_hidden {
            if let Some(name) = src_path.file_name() {
                if name.to_string_lossy().starts_with('.') {
                    return false;
                }
            }
        }
        // Date filter
        if let Some(date_filter) = &params.date_filter {
            let metadata = match src_path.metadata() {
                Ok(m) => m,
                Err(_) => return false,
            };
            let src_time = metadata.modified().ok();
            match date_filter {
                DateFilter::Newer => {
                    // Need destination path to compare
                    // We'll apply later when we have full target path
                    return true; // keep for now, will filter later
                }
                DateFilter::OnOrAfter(date) => {
                    if let Some(src_time) = src_time {
                        let src_date = chrono::DateTime::<chrono::Utc>::from(src_time).date_naive();
                        return src_date >= *date;
                    } else {
                        return false;
                    }
                }
            }
        }
        // Update-only: keep only if dest exists
        if params.update_only {
            let dest_path = target_dir.join(rel);
            if !dest_path.exists() {
                return false;
            }
        }
        true
    });

    // Apply Newer filter later during copy
    // Process each file/dir
    for (src_path, rel) in &all_files {
        let dest_path = target_dir.join(rel);

        if params.dirs_only {
            // /T only create directories
            if src_path.is_dir() {
                if !dest_path.exists() {
                    if params.list_only {
                        println!("{}", dest_path.display());
                    } else {
                        if let Err(e) = fs::create_dir_all(&dest_path) {
                            eprintln!("Error creating directory {}: {}", dest_path.display(), e);
                            if !params.continue_on_error { return Err(e); }
                        }
                    }
                }
            }
            continue;
        }

        // If src is a directory, just ensure dest directory exists (files inside will handle subdirs)
        if src_path.is_dir() {
            if !params.list_only {
                if let Err(e) = fs::create_dir_all(&dest_path) {
                    eprintln!("Error creating directory {}: {}", dest_path.display(), e);
                    if !params.continue_on_error { return Err(e); }
                }
            } else {
                println!("{}", dest_path.display());
            }
            continue;
        }

        // Apply Newer date filter now that we have dest_path
        if let Some(DateFilter::Newer) = &params.date_filter {
            if dest_path.exists() {
                let src_meta = src_path.metadata().ok();
                let dst_meta = dest_path.metadata().ok();
                if let (Some(s), Some(d)) = (src_meta, dst_meta) {
                    if let (Ok(st), Ok(dt)) = (s.modified(), d.modified()) {
                        if st <= dt {
                            continue; // skip, dest is newer or same
                        }
                    }
                }
            }
        }

        // Prompt for each file if /P
        if params.prompt_each {
            print!("Overwrite {} (Yes/No/All)? ", dest_path.display());
            io::stdout().flush().unwrap();
            let mut answer = String::new();
            io::stdin().read_line(&mut answer).unwrap();
            if !answer.trim().eq_ignore_ascii_case("y") && !answer.trim().eq_ignore_ascii_case("yes") {
                continue;
            }
        }

        // Handle overwrite prompt if /-Y and dest exists, or no /Y and dest exists and readonly
        let dest_exists = dest_path.exists();
        if dest_exists {
            let mut skip = false;
            // If /-Y or not /Y, ask unless /Y is set
            if params.yes != Some(true) {
                // If /-Y specified or no /Y and dest is readonly, ask
                if params.yes == Some(false) || (!params.overwrite_readonly && dest_path.metadata().map(|m| m.permissions().readonly()).unwrap_or(false)) {
                    print!("Overwrite {}? (Yes/No/All): ", dest_path.display());
                    io::stdout().flush().unwrap();
                    let mut answer = String::new();
                    io::stdin().read_line(&mut answer).unwrap();
                    if !answer.trim().eq_ignore_ascii_case("y") && !answer.trim().eq_ignore_ascii_case("yes") {
                        skip = true;
                    }
                }
            }
            if skip {
                continue;
            }
            // Remove read-only if /R
            if params.overwrite_readonly {
                let _ = fs::set_permissions(&dest_path, fs::Permissions::from_mode(0o644));
            }
        }

        // List only
        if params.list_only {
            if params.full_display {
                println!("{} -> {}", src_path.display(), dest_path.display());
            } else {
                println!("{}", src_path.display());
            }
            continue;
        }

        // Perform copy
        if let Err(e) = copy_file(src_path, &dest_path, params) {
            eprintln!("Error copying {}: {}", src_path.display(), e);
            if !params.continue_on_error {
                return Err(e);
            }
        } else if params.verify {
            // Verify size
            if let (Ok(s_meta), Ok(d_meta)) = (src_path.metadata(), dest_path.metadata()) {
                if s_meta.len() != d_meta.len() {
                    eprintln!("Verification failed: size mismatch for {}", src_path.display());
                    if !params.continue_on_error {
                        return Err(io::Error::new(io::ErrorKind::Other, "Verification failed"));
                    }
                }
            }
        }
    }

    Ok(())
}

fn prompt_dest_type(dest: &Path) -> io::Result<bool> {
    loop {
        eprint!("Does {} specify a file name or directory name on the target (F = file, D = directory)? ", dest.display());
        io::stdout().flush().unwrap();
        let mut input = String::new();
        io::stdin().read_line(&mut input).unwrap();
        match input.trim().to_uppercase().as_str() {
            "D" => return Ok(true),
            "F" => return Ok(false),
            _ => continue,
        }
    }
}

fn walk_dir(dir: &Path, include_hidden: bool) -> Vec<PathBuf> {
    let mut result = Vec::new();
    if let Ok(entries) = dir.read_dir() {
        for entry in entries.flatten() {
            let path = entry.path();
            let name = path.file_name().unwrap().to_string_lossy();
            if !include_hidden && name.starts_with('.') {
                continue;
            }
            if path.is_dir() {
                result.push(path.clone());
                result.extend(walk_dir(&path, include_hidden));
            } else {
                result.push(path);
            }
        }
    }
    result
}

fn copy_file(src: &Path, dest: &Path, params: &Params) -> io::Result<()> {
    // Create parent directory
    if let Some(parent) = dest.parent() {
        fs::create_dir_all(parent)?;
    }

    // Copy content
    let _bytes = fs::copy(src, dest)?;
    if !params.quiet {
        if params.full_display {
            println!("{} -> {}", src.display(), dest.display());
        } else {
            println!("{}", src.display());
        }
    }

    // Preserve attributes /K
    if params.preserve_attrs {
        // Permissions
        let src_meta = src.metadata()?;
        let src_perm = src_meta.permissions();
        // On Unix, set mode (but not ownership, which requires root)
        fs::set_permissions(dest, src_perm)?;

        // Modification time
        let modified = src_meta.modified()?;
        // set_file_times requires nightly or filetime crate. Use filetime? We can use utimes via libc, but to keep deps simple,
        // we'll use `filetime` crate. Add to Cargo.toml if needed. Alternatively, use std::fs::File::set_modified (stabilized Rust 1.75).
        // Let's assume Rust >= 1.75 on Termux.
        let dest_file = fs::File::open(dest)?;
        dest_file.set_modified(modified)?;
    }

    Ok(())
}

fn print_usage() {
    println!("XCOPY-like tool for Termux (Rust)");
    println!("Usage: xcopy source [destination] [options]");
    println!("Options: /S /E /I /-I /Y /-Y /R /H /K /D[:date] /P /W /C /V /F /Q /L /T /U /EXCLUDE:files");
    println!("See Windows XCOPY /? for details.");
}