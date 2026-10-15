use anyhow::Result;
use dumbtool::dumb::DumbPayload;

pub fn main(filelist: &Vec<String>) -> Result<()> {
    for file in filelist {
        let payload = DumbPayload::new()?;
        payload.write_to_file(&file)?;
    }
    return Ok(());
}
