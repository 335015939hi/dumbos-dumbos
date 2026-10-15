use axum::http::StatusCode;

use crate::server;
use dumbtool::dumbutil;
use dumbtool::events::Event;
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
                let e = format!("{e}");
                Event::reject_upload(user, &e).dispatch();
                return Err(StatusCode::FORBIDDEN);
            }
        },
        Err(e) => {
            Event::error(&"verifying request".into(), &e).dispatch();
            return Err(StatusCode::INTERNAL_SERVER_ERROR);
        }
    }

    let upload_path = match dumbutil::create_upload_filename(directory, user) {
        Ok(v) => v,
        Err(e) => {
            let e = format!("{e}");
            Event::error(&"creating upload path".into(), &e).dispatch();
            return Err(StatusCode::INTERNAL_SERVER_ERROR);
        }
    };

    let file_size = data.len();

    //TODO: user quotas to prevent DoS
    match util::write_file(&upload_path, &data) {
        Ok(_) => {}
        Err(e) => {
            let e = format!("{e}");
            Event::error(&format!("writing to {upload_path}"), &e).dispatch();
            return Err(StatusCode::INTERNAL_SERVER_ERROR);
        }
    }

    Event::upload(user, &upload_path, file_size).dispatch();
    Ok(String::from(""))
}
