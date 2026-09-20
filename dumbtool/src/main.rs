mod cmd_new;
mod cmd_cmdget;
mod dumb;

use clap::{Parser, Subcommand};
use std::string::String;

#[derive(Parser)]
struct Args {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// create a new payload
    New {
        /// name of code
        #[arg(short, long)]
        code: Option<Vec<String>>,
        /// file path instead of name of code
        #[arg(short, long)]
        file: Option<Vec<String>>,
    },
    CmdGet {
        /// name of code
        #[arg(short, long)]
        code: Option<Vec<String>>,
        /// file path instead of name of code
        #[arg(short, long)]
        file: Option<Vec<String>>,
    },
}

fn main() -> Result<(), String> {
    let args = Args::parse();
    match args.command {
        Commands::New { code, file } => {
            return cmd_new::main(code, file);
        }
        Commands::CmdGet { code, file } => {
            return cmd_cmdget::main(code, file);
        }
    }
}
