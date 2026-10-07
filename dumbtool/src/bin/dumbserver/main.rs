mod server;

use dumbtool::consts;

use clap::Parser;

#[derive(Parser)]
struct Args {
    /// run in headless mode
    #[arg(long)]
    headless: bool,
    /// directory for DumbOS stuff
    #[arg(short,long,default_value=consts::CODE_FILE_PATH)]
    directory: String,
}

pub fn main() {
    let args = Args::parse();
    let directory = args.directory;
    let headless = args.headless;
    if !headless {
        panic!("TUI not yet supported");
    }
    server::main(headless, directory);
}
