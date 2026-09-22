use crate::dumb;

use std::fs;
use std::io::{self, Read, Write};

fn read_file(path: &String) -> io::Result<Vec<u8>> {
    if path == "-" {
        let mut data = Vec::new();
        io::stdin().read_to_end(&mut data)?;
        return Ok(data);
    } else {
        return fs::read(path);
    }
}
fn write_file(path: &String, data: &Vec<u8>) -> io::Result<()> {
    if path == "-" {
        io::stdout().write_all(data)?;
        return Ok(());
    } else {
        return fs::write(path, data);
    }
}

pub fn getdata(filelist: &Vec<String>, output: &String) -> Result<(), String> {
    if filelist.len() == 0 {
        return Err("One of --code or --file must be specified".to_string());
    }
    let mut data: Vec<u8> = Vec::new();
    for file in filelist {
        let payload = dumb::read_from_file(&file);
        let mut new_data = dumb::get_data(&payload);
        data.append(&mut new_data);
    }
    return Ok(write_file(&output, &data).unwrap());
}
pub fn setdata(filelist: &Vec<String>, output: &String) -> Result<(), String> {
    if filelist.len() == 0 {
        return Err("One of --code or --file must be specified".to_string());
    }
    let data = read_file(&output).unwrap();

    for file in filelist {
        let mut payload = dumb::read_from_file(&file);
        payload = dumb::set_data(payload, &data);
        dumb::write_to_file(&payload, &file);
    }
    return Ok(());
}
