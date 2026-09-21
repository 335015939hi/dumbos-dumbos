use crate::dumb;

pub fn main(filelist: &Vec<String>) -> Result<(), String> {
    if filelist.len() == 0 {
        return Err("One of --code or --file must be specified".to_string());
    }

    for file in filelist {
        let payload = dumb::read_from_file(&file);
        let command = dumb::get_command(&payload);
        println!("{file}:{command}");
    }
    return Ok(());
}
