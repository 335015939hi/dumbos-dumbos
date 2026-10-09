mod dumbcode;
mod dumbupload;
mod server;

use dumbtool::consts;

use clap::Parser;
use std::sync::OnceLock;

#[derive(Parser)]
struct Args {
    /// run in headless mode
    #[arg(long)]
    headless: bool,
    /// directory for DumbOS stuff
    #[arg(short,long,default_value=consts::CODE_FILE_PATH)]
    directory: String,
    ///port to bind to
    #[arg(short, long, default_value = "8080")]
    port: u32,
}

pub static FJALL_DB: OnceLock<fjall::Database> = OnceLock::new();

#[tokio::main]
async fn main() {
    let args = Args::parse();
    let directory = args.directory;
    let headless = args.headless;
    let port = args.port;
    if !headless {
        panic!("TUI not yet supported");
    }
    match FJALL_DB.set(
        match fjall::Database::builder(consts::FJALL_DB_PATH).open() {
            Ok(v) => v,
            Err(e) => {
                panic!("{e}");
            }
        },
    ) {
        Ok(_) => {}
        Err(_) => {
            panic!("set on FJALL_DB failed");
        }
    };
    server::main(headless, directory, port).await;
}
