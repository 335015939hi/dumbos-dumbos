use crate::{consts, dumb, util};

use std::fs;

pub fn create(directory: &String, users: &Vec<String>) -> Result<(), String> {
    for user in users {
        let username;
        let fancy_name;
        if let Some((first_part, second_part)) = user.split_once('/') {
            username = String::from(first_part);
            fancy_name = String::from(second_part);
        } else {
            fancy_name = user.clone();
            username = user.clone();
        }

        let keypair = dumb::ed25519_generate_keys();
        let public_key = keypair.0;
        let private_key = keypair.1;
        let mut user_blob = dumb::make_user(&username, &private_key);
        dumb::user_set_fancy_name(&mut user_blob, &fancy_name);
        let mut directory = directory.clone();
        if !directory.ends_with('/') {
            directory.push('/');
        }
        let user_path = format!("{directory}{}{username}/", consts::USERDATA_PREFIX);
        let user_blob_file_path = format!("{user_path}{username}.blob");
        let user_pubkey_path = format!("{user_path}{}", consts::SERVER_USER_PUBKEY_FILE);

        fs::create_dir_all(user_path).unwrap();
        util::write_file(&user_pubkey_path, &public_key.into_bytes()).unwrap();
        util::write_file(&user_blob_file_path, &user_blob).unwrap();
    }
    Ok(())
}
