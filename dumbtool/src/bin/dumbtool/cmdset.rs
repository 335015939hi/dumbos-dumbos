use crate::consts;
use anyhow::Result;
use anyhow::bail;
use dumbtool::dumb::DumbPayload;
use dumbtool::util;

fn set_command(filelist: &Vec<String>, command: &str) -> Result<()> {
    for file in filelist {
        let mut payload = DumbPayload::from_file(file)?;
        payload.set_command(&command.to_string())?;
        payload.clear_data()?;
    }
    Ok(())
}

pub fn ok(filelist: &Vec<String>) -> Result<()> {
    return set_command(&filelist, consts::CMD_OK);
}

pub fn install_this(filelist: &Vec<String>, apk: &String) -> Result<()> {
    let apkfile = util::read_file(&apk)?;
    for file in filelist {
        let mut payload = DumbPayload::from_file(file)?;
        payload.command_install_this_apkdata(&apkfile)?;
        payload.write_to_file(&file)?;
    }
    Ok(())
}

pub fn file_export(filelist: &Vec<String>) -> Result<()> {
    Ok(set_command(&filelist, consts::CMD_FILE_EXPORT)?)
}

pub fn file_import(filelist: &Vec<String>) -> Result<()> {
    Ok(set_command(&filelist, consts::CMD_FILE_IMPORT)?)
}
pub fn firewall_flush(filelist: &Vec<String>) -> Result<()> {
    Ok(set_command(&filelist, consts::CMD_FW_FLUSH)?)
}

pub fn firewall_allow(filelist: &Vec<String>, apps: &Vec<String>) -> Result<()> {
    if apps.len() == 0 {
        bail!("no apps specified");
    }
    for file in filelist {
        let mut payload = DumbPayload::from_file(file)?;
        payload.command_firewall_allow(apps)?;
        payload.write_to_file(&file)?;
    }
    Ok(())
}

pub fn firewall_deny(filelist: &Vec<String>, apps: &Vec<String>) -> Result<()> {
    if apps.len() == 0 {
        bail!("no apps specified");
    }
    for file in filelist {
        let mut payload = DumbPayload::from_file(file)?;
        payload.command_firewall_deny(apps)?;
        payload.write_to_file(&file)?;
    }
    Ok(())
}
pub fn singbox_allow(filelist: &Vec<String>, domains: &Vec<String>) -> Result<()> {
    if domains.len() == 0 {
        bail!("no domains specified");
    }
    for file in filelist {
        let mut payload = DumbPayload::from_file(file)?;
        payload.command_singbox_allow(domains)?;
        payload.write_to_file(&file)?;
    }
    Ok(())
}

pub fn singbox_deny(filelist: &Vec<String>, domains: &Vec<String>) -> Result<()> {
    if domains.len() == 0 {
        bail!("no domains specified");
    }
    for file in filelist {
        let mut payload = DumbPayload::from_file(file)?;
        payload.command_singbox_deny(domains)?;
        payload.write_to_file(&file)?;
    }
    Ok(())
}
