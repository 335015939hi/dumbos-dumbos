use anyhow::Result;
use dumbtool::dumb::DumbPayload;

pub fn sign(filelist: &Vec<String>, privkey: &String) -> Result<()> {
    for file in filelist {
        let mut payload = DumbPayload::from_file(&file)?;
        payload.sign(privkey)?;
        payload.write_to_file(file)?;
    }
    Ok(())
}

pub fn verify(filelist: &Vec<String>, pubkey: &String) -> Result<()> {
    for file in filelist {
        let payload = DumbPayload::from_file(&file)?;
        match payload.verify(pubkey) {
            Ok(b) => match b {
                true => println!("{file}: passed"),
                false => print!("{file}: failed"),
            },
            Err(e) => {
                println!("{file}: failed: {e:#}");
            }
        }
    }
    Ok(())
}
