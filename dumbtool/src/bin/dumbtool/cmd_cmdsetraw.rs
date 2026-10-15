use anyhow::Result;
use dumbtool::dumb::DumbPayload;

pub fn main(filelist: &Vec<String>, command: &String) -> Result<()> {
    for file in filelist {
        let mut payload = DumbPayload::from_file(&file)?;
        payload.set_command(command)?;

        payload.write_to_file(&file)?;
    }
    return Ok(());
}
