mod cmd_new;
mod dumb;

use clap::{Parser, Subcommand};
use std::string::String;

#[derive(Parser)]
struct Args {
    //command
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
}

fn main() -> Result<(), String> {
    let args = Args::parse();
    match args.command {
        Commands::New { code, file } => {
            return cmd_new::cmd_new(code, file);
        }
    }
}
