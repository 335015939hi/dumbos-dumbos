use crate::dumb;

pub fn main(filelist: &Vec<String>) -> Result<(), String> {
    for file in filelist {
        println!("creating new code {file}");
        let payload = dumb::create_new();
        dumb::write_to_file(&payload, &file);
    }
    return Ok(());
}
