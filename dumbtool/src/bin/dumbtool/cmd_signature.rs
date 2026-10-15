use anyhow::Result;
use dumbtool::dumb;

pub fn sign(filelist: &Vec<String>, privkey: &String) -> Result<()> {
    for file in filelist {
        let mut payload = dumb::read_from_file(&file)?;
        dumb::sign(&mut payload, privkey)?;
        dumb::write_to_file(&payload, file)?;
    }
    Ok(())
}

pub fn verify(filelist: &Vec<String>, pubkey: &String) -> Result<()> {
    for file in filelist {
        let payload = dumb::read_from_file(&file)?;
        match dumb::verify(&payload, pubkey) {
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
