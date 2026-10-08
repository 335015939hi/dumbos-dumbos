use dumbtool::dumb;

pub fn sign(filelist: &Vec<String>, privkey: &String) -> Result<(), String> {
    for file in filelist {
        let mut payload = dumb::read_from_file(&file)?;
        dumb::sign(&mut payload, privkey)?;
        dumb::write_to_file(&payload, file)?;
    }
    Ok(())
}

pub fn verify(filelist: &Vec<String>, pubkey: &String) -> Result<(), String> {
    for file in filelist {
        let payload = dumb::read_from_file(&file)?;
        match dumb::verify(&payload, pubkey) {
            Ok(_) => {
                println!("{file}: passed");
            }
            Err(e) => {
                println!("{file}: failed: {e}");
            }
        }
    }
    Ok(())
}
