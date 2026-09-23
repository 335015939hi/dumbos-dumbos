use crate::dumb;

pub fn main(filelist: &Vec<String>) -> Result<(), String> {
    for file in filelist {
        let payload = dumb::read_from_file(&file);
        let command = dumb::get_command(&payload);
        println!("{file}:{command}");
    }
    return Ok(());
}
