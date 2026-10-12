use crate::{consts, dumb};
use chrono::Local;

pub fn create_code_filename(
    directory: &String,
    user: &String,
    code: &String,
) -> Result<String, String> {
    if dumb::verify_chars_username(user) == false {
        return Err(String::from("bad characters in username"));
    }
    if dumb::verify_chars_secretcode(code) == false {
        return Err(String::from("bad characters in code"));
    }
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

pub fn create_upload_filename(directory: &String, user: &String) -> Result<String, String> {
    if dumb::verify_chars_username(user) == false {
        return Err(String::from("bad characters in username"));
    }
    let mut directory = directory.clone();
    if !directory.ends_with('/') {
        directory.push('/');
    }
    let mut user = user.clone();
    if user != "" && !user.ends_with('/') {
        user.push('/');
    }
    let time = Local::now();
    let time = time.format("%Y-%m-%d %H:%M:%S");
    let uniqueid: String = (0..8)
        .map(|_| rand::random_range(b'a'..=b'z') as char)
        .collect();
    return Ok(format!(
        "{directory}{}{user}{}{time}-{uniqueid}",
        consts::USERDATA_PREFIX,
        consts::UPLOAD_FILE_PREFIX,
    ));
}

pub fn create_user_pubkey_path(directory: &String, user: &String) -> String {
    let mut directory = directory.clone();
    if !directory.ends_with('/') {
        directory.push('/');
    }
    let mut user = user.clone();
    if user != "" && !user.ends_with('/') {
        user.push('/');
    }
    return format!(
        "{directory}{}{user}{}",
        consts::USERDATA_PREFIX,
        consts::SERVER_USER_PUBKEY_FILE
    );
}
