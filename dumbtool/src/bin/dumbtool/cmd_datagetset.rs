use anyhow::Result;
use dumbtool::dumb::DumbPayload;
use dumbtool::util;

pub fn getdata(filelist: &Vec<String>, output: &String) -> Result<()> {
    let mut data: Vec<u8> = Vec::new();
    for file in filelist {
        let payload = DumbPayload::from_file(&file)?;
        let mut new_data = payload.data()?;
        data.append(&mut new_data);
    }
    return Ok(util::write_file(&output, &data).unwrap());
}
pub fn setdata(filelist: &Vec<String>, output: &String) -> Result<()> {
    let data = util::read_file(&output).unwrap();

    for file in filelist {
        let mut payload = DumbPayload::from_file(&file)?;
        payload.set_data(&data)?;
        payload.write_to_file(&file)?;
    }
    return Ok(());
}
