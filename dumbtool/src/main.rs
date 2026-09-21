mod cmd_cmdget;
mod cmd_cmdsetraw;
mod cmd_new;
mod dumb;

use clap::{Parser, Subcommand};
use std::string::String;

#[derive(Parser)]
struct Args {
    #[command(subcommand)]
    command: Commands,
    /// name of code
    #[arg(short, long)]
    code: Option<Vec<String>>,
    /// file path instead of name of code
    #[arg(short, long)]
    file: Option<Vec<String>>,
}

#[derive(Subcommand)]
enum Commands {
    /// create a new payload
    New,
    /// get the command of payload
    CmdGet,
    /// set the command, nothing extra
    CmdSetRaw {
        /// new command string
        command: String,
    },
    /// read the data field
    DataGet {
        /// file to write to, stdout by default
        #[arg(short, long)]
        output: Option<String>,
    },
    /// write to the data field
    DataSetRaw {
        /// file to read from, stdin by default
        #[arg(short, long)]
        input: Option<String>,
    },
}

fn main() -> Result<(), String> {
    let args = Args::parse();
    let code = args.code;
    let file = args.file;
    match args.command {
        Commands::New => {
            return cmd_new::main(code, file);
        }
        Commands::CmdGet => {
            return cmd_cmdget::main(code, file);
        }
        Commands::CmdSetRaw { command } => {
            return cmd_cmdsetraw::main(code, file, &command);
        }
        Commands::DataSetRaw { input } => {
            return Ok(());
        }
        Commands::DataGet { output } => {
            return Ok(());
        }
    }
}
