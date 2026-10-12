use axum::http::StatusCode;
use dumbtool::dumb;
use dumbtool::dumbutil;
use dumbtool::events::Event;
use dumbtool::util;

use crate::server;

unsafe extern "C" {
    //defined in c/server_key_private.c
    fn server_internal_get_private_key() -> *const std::os::raw::c_char;
}

fn get_private_key() -> String {
    let k: String;
    unsafe {
        let key = server_internal_get_private_key();
        k = std::ffi::CStr::from_ptr(key).to_string_lossy().into_owned();
    }
    k
}

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
            Event::reject_code(user, code, &msg).dispatch();
            return Err(StatusCode::FORBIDDEN);
        }
    }
    match dumbutil::create_code_filename(directory, user, code) {
        Ok(path) => code_path_user = path,
        Err(msg) => {
            Event::reject_code(user, code, &msg).dispatch();
            return Err(StatusCode::FORBIDDEN);
        }
    }

    match server::dumb_verify_request(&directory, &user, &code, &requestid, &requestsig, time) {
        Ok(v) => match v {
            Ok(_) => {}
            Err(e) => {
                Event::reject_code(user, code, &e).dispatch();
                return Err(StatusCode::FORBIDDEN);
            }
        },
        Err(e) => {
            Event::error(&String::from("verifying code request"), &e).dispatch();
            return Err(StatusCode::INTERNAL_SERVER_ERROR);
        }
    }

    let is_code_personal;
    let dumbpayload = match util::read_file(&code_path_user) {
        Ok(q) => {
            is_code_personal = true;
            q
        }
        Err(e) => {
            if e.kind() == std::io::ErrorKind::NotFound {
                match util::read_file(&code_path_global) {
                    Ok(q) => {
                        is_code_personal = false;
                        q
                    }
                    Err(e) => {
                        if e.kind() == std::io::ErrorKind::NotFound {
                            Event::reject_code(user, code, &String::from("no code found"))
                                .dispatch();
                            return Err(StatusCode::FORBIDDEN);
                        } else {
                            Event::error(
                                &format!("opening file {code_path_global}"),
                                &e.to_string(),
                            )
                            .dispatch();
                            return Err(StatusCode::INTERNAL_SERVER_ERROR);
                        }
                    }
                }
            } else {
                Event::error(&format!("opening file {code_path_user}"), &e.to_string()).dispatch();
                return Err(StatusCode::INTERNAL_SERVER_ERROR);
            }
        }
    };

    let mut dumbpayload = match dumb::u8_to_dumbpayload(&dumbpayload) {
        Ok(q) => q,
        Err(e) => {
            Event::error(&"u8_to_payload()".into(), &e).dispatch();
            return Err(StatusCode::INTERNAL_SERVER_ERROR);
        }
    };

    if dumb::check_expire_and_set(&mut dumbpayload) {
        //code is expired
        println!("code {code} requestedby user {user} is expired");
        Event::reject_code(user, code, &"expired".into()).dispatch();
        return Err(StatusCode::FORBIDDEN);
    }

    match dumb::sign(&mut dumbpayload, &get_private_key()) {
        Ok(_) => {}
        Err(e) => {
            Event::error(&"signing payload".into(), &e).dispatch();
            println!("failed to sign payload:{e}");
            return Err(StatusCode::INTERNAL_SERVER_ERROR);
        }
    }

    let dumbpayload = dumb::dumbpayload_to_u8(&dumbpayload);
    Event::code(user, code, is_code_personal).dispatch();

    return Ok(dumbpayload);
}
