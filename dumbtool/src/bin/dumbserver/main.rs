mod dumbcode;
mod dumbupload;
mod server;
mod ui;
use clap::Parser;
use dumbtool::consts;
use dumbtool::log;
use std::sync::OnceLock;
use std::thread;

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
pub static DIRECTORY: OnceLock<String> = OnceLock::new();

#[tokio::main]
async fn main() {
    let args = Args::parse();
    let directory = args.directory;
    let headless = args.headless;
    let port = args.port;

    match FJALL_DB.set(
        match fjall::Database::builder(format!("{directory}/{}", consts::FJALL_DB_FILENAME)).open()
        {
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
    DIRECTORY.set(directory.clone()).unwrap();

    let shutdown_rx = if !headless {
        log::set_verbosity("none").unwrap();
        let (shutdown_tx, shutdown_rx) = tokio::sync::oneshot::channel::<()>();
        thread::spawn(move || {
            ui::main::main();
            let _ = shutdown_tx.send(());
        });
        Some(shutdown_rx)
    } else {
        None
    };

    log::debug("starting server");
    server::main(headless, directory, port, shutdown_rx).await;
}
