use crate::consts;
use crate::dumb;
use crate::dumbutil;
use crate::util;
use anyhow::Result;
use anyhow::ensure;
use chrono::Local;
use std::fs;

pub fn create_code_filename(directory: &String, user: &String, code: &String) -> Result<String> {
    ensure!(
        dumb::verify_chars_username(user) == true,
        "bad characters in username"
    );
    ensure!(
        dumb::verify_chars_secretcode(code) == true,
        "bad characters in code"
    );

    let mut directory = directory.clone();
    if !directory.ends_with('/') {
        directory.push('/');
    }
    let mut user = user.clone();
    if user != "" && !user.ends_with('/') {
        user.push('/');
    }
    return Ok(format!(
        "{directory}{user}{}{code}",
        consts::CODE_FILE_PREFIX
    ));
}

pub fn create_upload_filename(directory: &String, user: &String) -> Result<String> {
    ensure!(
        dumb::verify_chars_username(user) == true,
        "bad characters in username"
    );
    let directory = create_user_path(directory, user);
    let time = Local::now();
    let time = time.format("%Y-%m-%d %H:%M:%S");
    let uniqueid: String = (0..8)
        .map(|_| rand::random_range(b'a'..=b'z') as char)
        .collect();
    return Ok(format!(
        "{directory}{}{time}-{uniqueid}",
        consts::UPLOAD_FILE_PREFIX,
    ));
}

pub fn create_user_path(directory: &String, user: &String) -> String {
    let mut directory = directory.clone();
    if !directory.ends_with('/') {
        directory.push('/');
    }
    let mut user = user.clone();
    if user != "" && !user.ends_with('/') {
        user.push('/');
    }
    return format!("{directory}{}{user}", consts::USERDATA_PREFIX);
}

pub fn create_user_pubkey_path(directory: &String, user: &String) -> String {
    let directory = create_user_path(directory, user);
    return format!("{directory}{}", consts::SERVER_USER_PUBKEY_FILE);
}

pub fn create_user_blob_path(directory: &String, user: &String) -> String {
    let directory = create_user_path(directory, user);
    return format!("{directory}{}", consts::SERVER_USER_BLOB_FILE);
}

pub fn create_user_fancyname_path(directory: &String, user: &String) -> String {
    let directory = create_user_path(directory, user);
    return format!("{directory}{}", consts::SERVER_USER_FANCYNAME_FILE);
}

pub fn create_user(directory: &String, user: &String) -> Result<()> {
    let username;
    let fancy_name;
    if let Some((first_part, second_part)) = user.split_once('/') {
        username = String::from(first_part);
        fancy_name = String::from(second_part);
    } else {
        fancy_name = user.clone();
        username = user.clone();
    }

    let keypair = dumb::ed25519_generate_keys()?;
    let public_key = keypair.0;
    let private_key = keypair.1;
    let mut user_blob = dumb::make_user(&username, &private_key)?;
    dumb::user_set_fancy_name(&mut user_blob, &fancy_name)?;

    let user_path = dumbutil::create_user_path(directory, user);
    let user_blob_file_path = dumbutil::create_user_blob_path(directory, user);
    let user_pubkey_path = dumbutil::create_user_pubkey_path(directory, user);
    let user_fancyname_path = dumbutil::create_user_fancyname_path(directory, user);

    ensure!(
        fs::exists(&user_path)? == false,
        format!("user {user} already exists")
    );

    fs::create_dir_all(&user_path)?;
    util::write_file(&user_pubkey_path, &public_key.into_bytes())?;
    util::write_file(&user_fancyname_path, &fancy_name.into_bytes())?;
    util::write_file(&user_blob_file_path, &user_blob)?;
    Ok(())
}
