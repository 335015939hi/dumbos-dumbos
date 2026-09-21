use crate::dumb;

pub fn main(filelist: &Vec<String>, command: &String) -> Result<(), String> {
    if filelist.len() == 0 {
        return Err("One of --code or --file must be specified".to_string());
    }

    for file in filelist {
        let mut payload = dumb::read_from_file(&file);
        dumb::set_command(&mut payload, command);

        dumb::write_to_file(&payload, &file);
    }
    return Ok(());
}
