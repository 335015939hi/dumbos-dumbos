use crate::{consts, dumb, util};

use std::fs;

pub fn create(directory: &String, users: &Vec<String>) -> Result<(), String> {
    for user in users {
        let keypair = dumb::ed25519_generate_keys();
        let public_key = keypair.0;
        let private_key = keypair.1;
        let user_blob = dumb::make_user(&user, &private_key);
        let mut directory = directory.clone();
        if !directory.ends_with('/') {
            directory.push('/');
        }
        let user_path = format!("{directory}{}{user}/", consts::USERDATA_PREFIX);
        let user_blob_file_path = format!("{user_path}{user}.blob");
        let user_pubkey_path = format!("{user_path}{}", consts::SERVER_USER_PUBKEY_FILE);

        fs::create_dir_all(user_path).unwrap();
        util::write_file(&user_pubkey_path, &public_key.into_bytes()).unwrap();
        util::write_file(&user_blob_file_path, &user_blob).unwrap();
    }
    Ok(())
}
