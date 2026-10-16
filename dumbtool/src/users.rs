use crate::consts;
use crate::dumb;
use crate::dumbutil;
use anyhow::Result;
use anyhow::bail;
use anyhow::ensure;
use core::fmt;
use std::fs;

pub struct DumbUser {
    name: String,
    fancyname: String,
    pubkey: String,
    datapath: String,
}

impl DumbUser {
    pub fn new(directory: &String, username: &str, fancyname: Option<&str>) -> Result<Self> {
        ensure!(
            dumb::verify_chars_username(&username.into()),
            "invalid characters in username"
        );

        let user_path = dumbutil::create_user_path(directory, &username.into());
        let user_blob_file_path = dumbutil::create_user_blob_path(directory, &username.into());
        let user_pubkey_path = dumbutil::create_user_pubkey_path(directory, &username.into());
        let user_fancyname_path = dumbutil::create_user_fancyname_path(directory, &username.into());

        ensure!(
            fs::exists(&user_path)? == false,
            format!("user {username} already exists")
        );

        let fancyname = match fancyname {
            Some(n) => n,
            None => username,
        };

        let keypair = dumb::ed25519_generate_keys()?;
        let public_key = keypair.0;
        let private_key = keypair.1;
        let mut user_blob = dumb::make_user(&username.into(), &private_key)?;
        dumb::user_set_fancy_name(&mut user_blob, &fancyname.into())?;

        fs::create_dir_all(&user_path)?;
        fs::write(&user_pubkey_path, &public_key.clone().into_bytes())?;
        fs::write(&user_fancyname_path, &fancyname)?;
        fs::write(&user_blob_file_path, &user_blob)?;
        Ok(Self {
            name: username.into(),
            fancyname: fancyname.into(),
            pubkey: public_key,
            datapath: user_path,
        })
    }
    pub fn get(directory: &String, username: &str) -> Result<Self> {
        ensure!(
            dumb::verify_chars_username(&username.into()),
            "invalid characters in username"
        );

        let user_path = dumbutil::create_user_path(directory, &username.into());
        let user_pubkey_path = dumbutil::create_user_pubkey_path(directory, &username.into());
        let user_fancyname_path = dumbutil::create_user_fancyname_path(directory, &username.into());

        let fancyname = match fs::read(user_fancyname_path) {
            Ok(v) => String::from_utf8(v)?,
            Err(e) => {
                if e.kind() == std::io::ErrorKind::NotFound {
                    String::from(username)
                } else {
                    bail!(e);
                }
            }
        };

        let pubkey = String::from_utf8(fs::read(user_pubkey_path)?)?;

        Ok(Self {
            name: username.into(),
            fancyname: fancyname,
            pubkey: pubkey,
            datapath: user_path,
        })
    }
    pub fn get_all(directory: &String) -> Result<Vec<Self>> {
        let mut user_list: Vec<DumbUser> = vec![];
        for maybeuser in fs::read_dir(format!("{directory}/{}", consts::USERDATA_PREFIX))? {
            let maybeuser = maybeuser?;
            let keyfile = maybeuser.path().join(consts::SERVER_USER_PUBKEY_FILE);
            if keyfile.is_file() {
                let username = maybeuser.file_name();
                let username = username.to_string_lossy().into_owned();
                if dumb::verify_chars_username(&username) {
                    user_list.push(DumbUser::get(directory, username.as_str())?);
                }
            }
        }
        Ok(user_list)
    }

    pub fn name(&self) -> &String {
        &self.name
    }
    pub fn fancyname(&self) -> &String {
        &self.fancyname
    }
    pub fn pubkey(&self) -> &String {
        &self.pubkey
    }
    pub fn path(&self) -> &String {
        &self.datapath
    }
}

impl fmt::Display for DumbUser {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if self.name == self.fancyname {
            write!(f, "{}", self.name)
        } else {
            write!(f, "{} ({})", self.fancyname, self.name)
        }
    }
}
