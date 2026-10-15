use anyhow::Result;
use dumbtool::dumb;
use dumbtool::util;

pub fn getdata(filelist: &Vec<String>, output: &String) -> Result<()> {
    let mut data: Vec<u8> = Vec::new();
    for file in filelist {
        let payload = dumb::read_from_file(&file)?;
        let mut new_data = dumb::get_data(&payload)?;
        data.append(&mut new_data);
    }
    return Ok(util::write_file(&output, &data).unwrap());
}
pub fn setdata(filelist: &Vec<String>, output: &String) -> Result<()> {
    let data = util::read_file(&output).unwrap();

    for file in filelist {
        let mut payload = dumb::read_from_file(&file)?;
        dumb::set_data(&mut payload, &data)?;
        dumb::write_to_file(&payload, &file)?;
    }
    return Ok(());
}
