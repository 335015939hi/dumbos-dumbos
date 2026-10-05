mod cmd_cmdget;
mod cmd_cmdsetraw;
mod cmd_createheaders;
mod cmd_datagetset;
mod cmd_expire;
mod cmd_new;
mod cmd_users;
mod cmdset;
mod consts;
mod dumb;
mod util;

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
    /// directory for DumbOS stuff, WD by default
    #[arg(short, long, default_value = consts::CODE_FILE_PATH)]
    directory: String,
    /// user to use, global by default
    #[arg(short, long)]
    user: Option<Vec<String>>,
}

#[derive(Subcommand)]
enum Commands {
    /// create a new payload
    New,
    /// get the command of payload
    CmdGet,
    /// set the command as a raw string
    CmdSetRaw {
        /// new command string
        command: String,
    },
    /// read the data field
    /// will concatenate output if multiple files specified
    DataGet {
        /// file to write to, stdout by default
        #[arg(short, long, default_value = "-")]
        output: String,
    },
    /// write to the data field
    DataSetRaw {
        /// file to read from, stdin by default
        #[arg(short, long, default_value = "-")]
        input: String,
    },
    /// get expire date as raw string
    ExpireGetRaw,
    /// set expire date as raw string
    ExpireSetRaw {
        /// the new expire time, as raw string
        expire: String,
    },
    /// get expire time
    ExpireGet,
    /// set expire time
    ExpireSet {
        /// raw string
        #[arg(long,conflicts_with_all=&["timestamp","relative"])]
        raw: Option<String>,
        /// UNIX timestamp
        #[arg(short, long,conflicts_with_all=&["raw","relative"])]
        timestamp: Option<u64>,
        /// relative expire (time after first use), in seconds
        #[arg(short, long,conflicts_with_all=&["timestamp","raw"])]
        relative: Option<i32>,
    },

    /// set the command of the payload
    CommandSet {
        #[command(subcommand)]
        command: PayloadCommand,
    },
    /// create a new user.
    /// specify usernames by --user. set the display name with text after a '/' (allow special
    /// characters, chinese, etc.)
    CreateUser,
    /// create the public/private key header files required to compile the DumbOS server and client.
    /// copy the generated key_*.h headers into /repo/root/keys/ (or just use --directory /repo/root/keys/)
    CreateKeyHeaders,
}

#[derive(Subcommand)]
enum PayloadCommand {
    // subcommands for high level payload commands
    /// no-op, doesn't do anything. useful for testing, or disabling
    Ok,
    /// install a apk file
    InstallThis {
        /// path of apk file
        apk: String,
    },
    /// export local storage to external USB (format as exfat)
    FileExport,
    /// import files from external USB (formate as exfat) to local storage
    FileImport,
    /// allow an app, by package name, to access network. note that this is UID-based and certain
    /// apps (especially system apps) may share UIDs, allowing one app will allow all apps sharing
    /// the same UID
    FirewallAllow {
        /// package name of app, specify multiple times for multiple apps
        app: Vec<String>,
    },
    /// revoke permission of app, by package name, from accessing network. note that this is UID-based and certain
    /// apps (especially system apps) may share UIDs. make sure to remove all of them sharing the
    /// same UID.
    FirewallDeny {
        /// package name of app, specify multiple times for multiple apps
        app: Vec<String>,
    },
    /// reset the firewall permissions
    FirewallFlush,
}

fn main() -> Result<(), String> {
    let args = Args::parse();
    let code = args.code;
    let file = args.file;
    let mut dumb_dir = args.directory;
    let users = if args.user != None {
        args.user.unwrap()
    } else {
        Vec::new()
    };
    if !dumb_dir.ends_with('/') {
        dumb_dir.push('/');
    }
    let dumb_dir = dumb_dir;
    let mut filelist: Vec<String> = Vec::new();
    if code != None {
        for codename in code.unwrap() {
            if users.len() == 0 {
                filelist.push(format!(
                    "{}{}{}",
                    dumb_dir,
                    consts::CODE_FILE_PREFIX,
                    codename
                ));
            } else {
                for user in &users {
                    filelist.push(format!(
                        "{}{}/{}{}",
                        dumb_dir,
                        user,
                        consts::CODE_FILE_PREFIX,
                        codename
                    ));
                }
            }
        }
    }
    if file != None {
        for filename in file.unwrap() {
            filelist.push(filename);
        }
    }

    match args.command {
        Commands::New => {
            return cmd_new::main(&filelist);
        }
        Commands::CmdGet => {
            return cmd_cmdget::main(&filelist);
        }
        Commands::CmdSetRaw { command } => {
            return cmd_cmdsetraw::main(&filelist, &command);
        }
        Commands::DataSetRaw { input } => {
            return cmd_datagetset::setdata(&filelist, &input);
        }
        Commands::DataGet { output } => {
            return cmd_datagetset::getdata(&filelist, &output);
        }
        Commands::ExpireSetRaw { expire } => {
            return cmd_expire::set_raw(&filelist, &expire);
        }
        Commands::ExpireGetRaw => {
            return cmd_expire::get_raw(&filelist);
        }
        Commands::ExpireSet {
            raw,
            timestamp,
            relative,
        } => {
            return cmd_expire::set(&filelist, &raw, &timestamp, &relative);
        }
        Commands::ExpireGet => {
            return cmd_expire::get(&filelist);
        }
        Commands::CreateUser => {
            return cmd_users::create(&dumb_dir, &users);
        }
        Commands::CreateKeyHeaders => {
            return cmd_createheaders::main(&dumb_dir);
        }
        Commands::CommandSet { command } => match command {
            PayloadCommand::Ok => {
                return cmdset::ok(&filelist);
            }
            PayloadCommand::InstallThis { apk } => {
                return cmdset::install_this(&filelist, &apk);
            }
            PayloadCommand::FileExport => {
                return cmdset::file_export(&filelist);
            }
            PayloadCommand::FileImport => {
                return cmdset::file_import(&filelist);
            }
            PayloadCommand::FirewallAllow { app } => {
                return cmdset::firewall_allow(&filelist, &app);
            }
            PayloadCommand::FirewallDeny { app } => {
                return cmdset::firewall_deny(&filelist, &app);
            }
            PayloadCommand::FirewallFlush => {
                return cmdset::firewall_flush(&filelist);
            }
        },
    }
}
