use crate::dumb;

pub fn main(filelist: &Vec<String>) -> Result<(), String> {
    if filelist.len() == 0 {
        return Err("One of --code or --file must be specified".to_string());
    }

    for file in filelist {
        println!("creating new code {file}");
        let payload = dumb::create_new();
        dumb::write_to_file(&payload, &file);
    }
    return Ok(());
}
