use crate::dumb;

pub fn main(filelist: &Vec<String>, command: &String) -> Result<(), String> {
    for file in filelist {
        let mut payload = dumb::read_from_file(&file);
        dumb::set_command(&mut payload, command);

        dumb::write_to_file(&payload, &file);
    }
    return Ok(());
}
