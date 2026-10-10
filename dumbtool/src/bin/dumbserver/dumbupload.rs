use axum::http::StatusCode;

use crate::server;
use dumbtool::dumbutil;
use dumbtool::util;

pub async fn main(
    directory: &String,
    user: &String,
    code: &String,
    requestid: &String,
    requestsig: &String,
    time: u64,
    data: Vec<u8>,
) -> Result<String, StatusCode> {
    match server::dumb_verify_request(&directory, &user, &code, &requestid, &requestsig, time) {
        Ok(v) => match v {
            Ok(_) => {}
            Err(e) => {
                println!("{e}");
                return Err(StatusCode::FORBIDDEN);
            }
        },
        Err(e) => {
            println!("{e}");
            return Err(StatusCode::INTERNAL_SERVER_ERROR);
        }
    }

    let upload_path = match dumbutil::create_upload_filename(directory, user) {
        Ok(v) => v,
        Err(e) => {
            println!("failed constructing upload path:{e}");
            return Err(StatusCode::INTERNAL_SERVER_ERROR);
        }
    };

    //TODO: user quotas to prevent DoS
    match util::write_file(&upload_path, &data) {
        Ok(_) => {}
        Err(e) => {
            println!("failed writing to {upload_path}:{e}");
            return Err(StatusCode::INTERNAL_SERVER_ERROR);
        }
    }

    Ok(String::from(""))
}
