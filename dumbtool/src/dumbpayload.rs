use crate::consts;
use crate::dumb;
use crate::dumb::DumbPayload;
use crate::util;
use anyhow::Result;

pub fn command_install_this_apk(
    mut payload: DumbPayload,
    apk_path: &String,
) -> Result<DumbPayload> {
    let apk_data = util::read_file(&apk_path)?;
    dumb::set_command(&mut payload, &consts::CMD_INSTALLTHIS.into())?;
    dumb::set_data(&mut payload, &apk_data)?;
    Ok(payload)
}
pub fn command_install_this_apkdata(
    mut payload: DumbPayload,
    apk_data: &Vec<u8>,
) -> Result<DumbPayload> {
    dumb::set_command(&mut payload, &consts::CMD_INSTALLTHIS.into())?;
    dumb::set_data(&mut payload, &apk_data)?;
    Ok(payload)
}

impl DumbPayload {
    pub fn new() -> Result<Self> {
        dumb::create_new()
    }
    pub fn from_file(path: &String) -> Result<Self> {
        dumb::read_from_file(path)
    }
    pub fn from_u8(data: &Vec<u8>) -> Result<Self> {
        dumb::u8_to_dumbpayload(data)
    }
    pub fn clone(&self) -> Result<Self> {
        dumb::payload_clone(self)
    }

    pub fn write_to_file(&self, path: &String) -> Result<()> {
        dumb::write_to_file(self, path)
    }
    pub fn size(&self) -> usize {
        dumb::payload_size(self)
    }
    pub fn to_u8(&self) -> Result<Vec<u8>> {
        dumb::dumbpayload_to_u8(self)
    }

    pub fn expire_raw(&self) -> Result<String> {
        dumb::get_expire_raw(self)
    }
    pub fn set_expire_raw(&mut self, expire: &String) -> Result<()> {
        dumb::set_expire_raw(self, expire)
    }
    pub fn set_command(&mut self, command: &String) -> Result<()> {
        dumb::set_command(self, command)
    }
    pub fn command(&self) -> Result<String> {
        dumb::get_command(self)
    }
    pub fn data(&self) -> Result<Vec<u8>> {
        dumb::get_data(self)
    }
    pub fn set_data(&mut self, data: &Vec<u8>) -> Result<()> {
        dumb::set_data(self, data)
    }
}
