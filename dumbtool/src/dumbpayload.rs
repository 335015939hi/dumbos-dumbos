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
    let payload = dumb::set_data(payload, &apk_data)?;
    Ok(payload)
}
pub fn command_install_this_apkdata(
    mut payload: DumbPayload,
    apk_data: &Vec<u8>,
) -> Result<DumbPayload> {
    dumb::set_command(&mut payload, &consts::CMD_INSTALLTHIS.into())?;
    let payload = dumb::set_data(payload, &apk_data)?;
    Ok(payload)
}
