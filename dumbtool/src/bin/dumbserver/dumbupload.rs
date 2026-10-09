use axum::http::StatusCode;

pub async fn main(
    directory: &String,
    user: &String,
    requestid: &String,
    requestsig: &String,
    time: u64,
    data: Vec<u8>,
) -> Result<String, StatusCode> {
    println!("{user}{time}{}", data.len());
    Ok(String::from(""))
}
