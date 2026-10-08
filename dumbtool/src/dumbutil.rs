use crate::{consts, dumb};

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
