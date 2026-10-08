use axum::http::StatusCode;
use dumbtool::dumb;
use dumbtool::dumbutil;
use dumbtool::util;

pub async fn main(
    directory: &String,
    user: &String,
    code: &String,
    requestid: &String,
    requestsig: &String,
    time: u64,
) -> Result<Vec<u8>, StatusCode> {
    let code_path_global;
    let code_path_user;
    match dumbutil::create_code_filename(directory, &String::from(""), code) {
        Ok(path) => code_path_global = path,
        Err(msg) => {
            println!("{msg}");
            return Err(StatusCode::FORBIDDEN);
        }
    }
    match dumbutil::create_code_filename(directory, user, code) {
        Ok(path) => code_path_user = path,
        Err(msg) => {
            println!("{msg}");
            return Err(StatusCode::FORBIDDEN);
        }
    }
    if dumb::verify_chars_requestid(requestid) == false {
        println!("bad characters found in requestid");
        return Err(StatusCode::FORBIDDEN);
    }
    let user_pubkey = dumbutil::create_user_pubkey_path(directory, user);
    let user_pubkey = util::read_file(&user_pubkey);
    let user_pubkey = match user_pubkey {
        Ok(key) => key,
        Err(e) => {
            if e.kind() == std::io::ErrorKind::NotFound {
                println!("user {user} probably doesn't exist");
                return Err(StatusCode::FORBIDDEN);
            } else {
                println!("{}", e.to_string());
                return Err(StatusCode::INTERNAL_SERVER_ERROR);
            }
        }
    };

    Err(StatusCode::NOT_FOUND)
}
