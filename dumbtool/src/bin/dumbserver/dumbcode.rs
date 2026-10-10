use axum::http::StatusCode;
use dumbtool::dumb;
use dumbtool::dumbutil;
use dumbtool::tables;
use dumbtool::util;

use crate::FJALL_DB;

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
    let request_id_table = crate::server::REQUEST_ID_TABLE.get().unwrap();
    match request_id_table.exists(requestid.as_str()) {
        Ok(v) => {
            if v {
                println!("user {user} tried to make a duplicate request (id={requestid})");
                return Err(StatusCode::FORBIDDEN);
            }
        }
        Err(e) => {
            println!("{e}");
            return Err(StatusCode::INTERNAL_SERVER_ERROR);
        }
    }
    match dumb::verify_dumbos_request(
        &user,
        &requestid,
        time,
        &requestsig,
        &String::from_utf8(user_pubkey).unwrap(),
    ) {
        Ok(_) => {}
        Err(msg) => {
            println!("verify_dumbos_request() failed:{msg}");
            return Err(StatusCode::FORBIDDEN);
        }
    }

    match request_id_table.insert(requestid.as_str()) {
        Ok(_) => {}
        Err(e) => {
            println!("consuming request id failed:{e}");
            return Err(StatusCode::INTERNAL_SERVER_ERROR);
        }
    }

    let dumbpayload = match util::read_file(&code_path_user) {
        Ok(q) => q,
        Err(e) => {
            if e.kind() == std::io::ErrorKind::NotFound {
                match util::read_file(&code_path_global) {
                    Ok(q) => q,
                    Err(e) => {
                        if e.kind() == std::io::ErrorKind::NotFound {
                            println!("no code {code} found for user {user}");
                            return Err(StatusCode::FORBIDDEN);
                        } else {
                            println!("failed to open {code_path_global}:{}", e.to_string());
                            return Err(StatusCode::INTERNAL_SERVER_ERROR);
                        }
                    }
                }
            } else {
                println!("failed to open {code_path_user}:{}", e.to_string());
                return Err(StatusCode::INTERNAL_SERVER_ERROR);
            }
        }
    };

    let mut dumbpayload = match dumb::u8_to_dumbpayload(&dumbpayload) {
        Ok(q) => q,
        Err(e) => {
            println!("{e}");
            return Err(StatusCode::INTERNAL_SERVER_ERROR);
        }
    };

    if dumb::check_expire_and_set(&mut dumbpayload) {
        //code is expired
        println!("code {code} requestedby user {user} is expired");
        return Err(StatusCode::FORBIDDEN);
    }

    match dumb::sign(&mut dumbpayload, &get_private_key()) {
        Ok(_) => {}
        Err(e) => {
            println!("failed to sign payload:{e}");
            return Err(StatusCode::INTERNAL_SERVER_ERROR);
        }
    }

    let dumbpayload = dumb::dumbpayload_to_u8(&dumbpayload);
    println!("done");

    return Ok(dumbpayload);
}
