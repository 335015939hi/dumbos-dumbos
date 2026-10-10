use axum::body::Bytes;
use axum::http::StatusCode;
use axum::{Router, extract::Query, routing::get, routing::post};
use dumbtool::tables;
use serde::Deserialize;
use std::sync::OnceLock;

use crate::dumbcode;
use crate::dumbupload;
use dumbtool::consts;

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
