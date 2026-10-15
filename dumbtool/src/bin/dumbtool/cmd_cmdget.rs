use anyhow::Result;
use dumbtool::dumb::DumbPayload;

pub fn main(filelist: &Vec<String>) -> Result<()> {
    for file in filelist {
        let payload = DumbPayload::from_file(&file)?;
        let command = payload.command()?;
        println!("{file}:{command}");
    }
    return Ok(());
}
