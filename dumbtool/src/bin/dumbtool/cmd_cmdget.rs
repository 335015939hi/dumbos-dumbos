use anyhow::Result;
use dumbtool::dumb;

pub fn main(filelist: &Vec<String>) -> Result<()> {
    for file in filelist {
        let payload = dumb::read_from_file(&file)?;
        let command = dumb::get_command(&payload)?;
        println!("{file}:{command}");
    }
    return Ok(());
}
