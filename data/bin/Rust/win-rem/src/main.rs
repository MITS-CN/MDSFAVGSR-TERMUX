use std::env;

fn print_help() {
    println!("Records comments (remarks) in a batch file or CONFIG.SYS.");
    println!();
    println!("REM [comment]");
    println!();
    println!("  comment   Any string of characters to be treated as a comment.");
    println!();
    println!("If no comment is given, REM does nothing.");
    println!("Use REM /? to display this help.");
}

fn main() {
    let args: Vec<String> = env::args().skip(1).collect();

    // 处理帮助请求
    if args.iter().any(|a| a == "/?") {
        print_help();
        return;
    }

    // 否则什么都不做（即使有其他参数也忽略）
}