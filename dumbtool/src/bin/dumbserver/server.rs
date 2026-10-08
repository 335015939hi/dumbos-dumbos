use axum::http::StatusCode;
use axum::{Router, extract::Query, routing::get};
use serde::Deserialize;
use std::sync::OnceLock;

use crate::dumbcode;

static DIRECTORY: OnceLock<String> = OnceLock::new();

#[derive(Deserialize)]
// see daemon/secret_code.c
struct Params {
    user: String,
    code: String,
    requestid: String,
    requestsig: String,
    time: u64,
}

async fn dumbcode(Query(q): Query<Params>) -> Result<Vec<u8>, StatusCode> {
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

pub async fn main(_allow_output: bool, directory: String, port: u32) {
    DIRECTORY.set(directory).unwrap();
    let server: Router = Router::new()
        .route("/", get(|| async { "hello" }))
        .route("/dumb", get(dumbcode));
    let listener = tokio::net::TcpListener::bind(format!("0.0.0.0:{port}"))
        .await
        .unwrap();

    axum::serve(listener, server).await.unwrap();
}
