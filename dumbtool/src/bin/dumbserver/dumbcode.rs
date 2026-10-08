use axum::http::StatusCode;
use dumbtool::dumbutil;

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
    match (dumbutil::create_code_filename(directory, user, code)) {
        Ok(path) => code_path_user = path,
        Err(msg) => {
            println!("{msg}");
            return Err(StatusCode::FORBIDDEN);
        }
    }
    println!("{code_path_global} {code_path_user}");
    Err(StatusCode::FORBIDDEN)
}
