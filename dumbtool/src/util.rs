use std::fs;
use std::io::{self, Read, Write};

pub fn read_file(path: &String) -> io::Result<Vec<u8>> {
    if path == "-" {
        let mut data = Vec::new();
        io::stdin().read_to_end(&mut data)?;
        return Ok(data);
    } else {
        return fs::read(path);
    }
}
pub fn write_file(path: &String, data: &Vec<u8>) -> io::Result<()> {
    if path == "-" {
        io::stdout().write_all(data)?;
        return Ok(());
    } else {
        return fs::write(path, data);
    }
}
