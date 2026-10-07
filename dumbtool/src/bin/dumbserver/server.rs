use axum::{Router, routing::get};
use serde::Deserialize;
use std::net::SocketAddr;

use crate::dumbcode;

#[derive(Deserialize)]
struct Params {
    name: String,
}

pub async fn main(allow_output: bool, directory: String, port: u32) {
    let server: Router = Router::new()
        .route("/", get(|| async { "hello" }))
        .route("/dumb", get(dumbcode::main));
    let listener = tokio::net::TcpListener::bind(format!("0.0.0.0:{port}"))
        .await
        .unwrap();

    axum::serve(listener, server).await.unwrap();
}
