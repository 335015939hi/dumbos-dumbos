use axum::body::Bytes;
use axum::http::{StatusCode, request};
use axum::{Router, extract::Query, routing::get, routing::post};
use dumbtool::tables;
use serde::Deserialize;
use std::sync::OnceLock;

use crate::dumbcode;
use crate::dumbupload;
use dumbtool::consts;
use dumbtool::dumb;
use dumbtool::dumbutil;
use dumbtool::util;

static DIRECTORY: OnceLock<String> = OnceLock::new();

pub static REQUEST_ID_TABLE: OnceLock<tables::RequestIdTable> = OnceLock::new();

#[derive(Deserialize)]
// see daemon/secret_code.c
struct DumbCodeParams {
    user: String,
    code: String,
    requestid: String,
    requestsig: String,
    time: u64,
}
#[derive(Deserialize)]
struct DumbUploadParams {
    user: String,
    requestid: String,
    requestsig: String,
    time: u64,
}

async fn dumbcode(Query(q): Query<DumbCodeParams>) -> Result<Vec<u8>, StatusCode> {
    dumbcode::main(
        &DIRECTORY.get().unwrap(),
        &q.user,
        &q.code,
        &q.requestid,
        &q.requestsig,
        q.time,
    )
    .await
}
async fn dumbpost(Query(q): Query<DumbUploadParams>, data: Bytes) -> Result<String, StatusCode> {
    dumbupload::main(
        &DIRECTORY.get().unwrap(),
        &q.user,
        &q.requestid,
        &q.requestsig,
        q.time,
        data.to_vec(),
    )
    .await
}

pub fn dumb_verify_request(
    directory: &String,
    user: &String,
    code: &String,
    requestid: &String,
    requestsig: &String,
    time: u64,
) -> Result<Result<(), String>, String> {
    if dumb::verify_chars_username(user) == false {
        return Ok(Err("bad characters found in username".into()));
    }
    if dumb::verify_chars_secretcode(code) == false {
        return Ok(Err("bad characters found in secret code".into()));
    }
    if dumb::verify_chars_requestid(requestid) == false {
        return Ok(Err("bad characters found in requestid".into()));
    }

    let user_requestid = user.clone() + "/" + requestid;

    let user_pubkey_path = dumbutil::create_user_pubkey_path(directory, user);
    let user_pubkey = util::read_file(&user_pubkey_path);
    let user_pubkey = match user_pubkey {
        Ok(key) => key,
        Err(e) => {
            if e.kind() == std::io::ErrorKind::NotFound {
                return Err("user {user} probably doesn't exist".into());
            } else {
                return Err(format!(
                    "error opening {user_pubkey_path}:{}",
                    e.to_string()
                ));
            }
        }
    };

    let request_id_table = REQUEST_ID_TABLE.get().unwrap();
    match request_id_table.exists(user_requestid.as_str()) {
        Ok(v) => {
            if v {
                return Ok(Err(format!(
                    "user {user} tried to make a duplicate request (id={requestid})"
                )));
            }
        }
        Err(e) => {
            return Err(format!("error reading REQUEST_ID_TABLE: {e}"));
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
            return Ok(Err(format!("verify_dumbos_request() failed:{msg}")));
        }
    }

    match request_id_table.insert(user_requestid.as_str()) {
        Ok(_) => return Ok(Ok(())),
        Err(e) => {
            return Err(format!("consuming request id failed:{e}"));
        }
    }
}

pub async fn main(_allow_output: bool, directory: String, port: u32) {
    DIRECTORY.set(directory).unwrap();
    match REQUEST_ID_TABLE.set(
        match tables::RequestIdTable::open(crate::FJALL_DB.get().unwrap()) {
            Ok(v) => v,
            Err(e) => {
                panic!("failed to open requestId table:{e}");
            }
        },
    ) {
        Ok(_) => {}
        Err(_) => {
            panic!("failed to open requestId table: OnceLock");
        }
    }

    let server: Router = Router::new()
        .route("/", get(|| async { "hello" }))
        .route(
            (String::from("/") + consts::DUMB_URL_CODE_PATH).as_str(),
            get(dumbcode),
        )
        .route(
            (String::from("/") + consts::DUMB_URL_UPLOAD_PATH).as_str(),
            post(dumbpost),
        );
    let listener = tokio::net::TcpListener::bind(format!("0.0.0.0:{port}"))
        .await
        .unwrap();

    axum::serve(listener, server).await.unwrap();
}
