use crate::consts;
use anyhow::Result;
use anyhow::bail;
use dumbtool::dumb;
use dumbtool::dumbpayload;
use dumbtool::util;

fn set_command(filelist: &Vec<String>, command: &str) -> Result<()> {
    for file in filelist {
        let mut payload = dumb::read_from_file(file)?;
        dumb::set_command(&mut payload, &command.to_string())?;
        let payload = dumb::set_data(payload, &vec![])?;
        dumb::write_to_file(&payload, &file)?;
    }
    Ok(())
}

pub fn ok(filelist: &Vec<String>) -> Result<()> {
    return set_command(&filelist, consts::CMD_OK);
}

pub fn install_this(filelist: &Vec<String>, apk: &String) -> Result<()> {
    let apkfile = util::read_file(&apk)?;
    for file in filelist {
        let payload = dumb::read_from_file(file)?;
        let payload = dumbpayload::command_install_this_apkdata(payload, &apkfile)?;
        dumb::write_to_file(&payload, &file)?;
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
    let mut data: Vec<u8> = vec![];
    if apps.len() == 0 {
        bail!("no apps specified");
    }
    for app in apps {
        let mut app = app.clone().into_bytes();
        app.push(0);
        data.append(&mut app);
    }
    for file in filelist {
        let mut payload = dumb::read_from_file(file)?;
        dumb::set_command(&mut payload, &consts::CMD_FW_ALLOW.to_string())?;
        let payload = dumb::set_data(payload, &data)?;
        dumb::write_to_file(&payload, &file)?;
    }
    Ok(())
}

pub fn firewall_deny(filelist: &Vec<String>, apps: &Vec<String>) -> Result<()> {
    let mut data: Vec<u8> = vec![];
    if apps.len() == 0 {
        bail!("no apps specified");
    }
    for app in apps {
        let mut app = app.clone().into_bytes();
        app.push(0);
        data.append(&mut app);
    }
    for file in filelist {
        let mut payload = dumb::read_from_file(file)?;
        dumb::set_command(&mut payload, &consts::CMD_FW_DENY.to_string())?;
        let payload = dumb::set_data(payload, &data)?;
        dumb::write_to_file(&payload, &file)?;
    }
    Ok(())
}
pub fn singbox_allow(filelist: &Vec<String>, domains: &Vec<String>) -> Result<()> {
    let mut data: Vec<u8> = vec![];
    if domains.len() == 0 {
        bail!("no domains specified");
    }
    for domain in domains {
        let mut domain = domain.clone().into_bytes();
        domain.push(0);
        data.append(&mut domain);
    }
    for file in filelist {
        let mut payload = dumb::read_from_file(file)?;
        dumb::set_command(&mut payload, &consts::CMD_SINGBOX_ADD.to_string())?;
        let payload = dumb::set_data(payload, &data)?;
        dumb::write_to_file(&payload, &file)?;
    }
    Ok(())
}

pub fn singbox_deny(filelist: &Vec<String>, domains: &Vec<String>) -> Result<()> {
    let mut data: Vec<u8> = vec![];
    if domains.len() == 0 {
        bail!("no domains specified");
    }
    for domain in domains {
        let mut domain = domain.clone().into_bytes();
        domain.push(0);
        data.append(&mut domain);
    }
    for file in filelist {
        let mut payload = dumb::read_from_file(file)?;
        dumb::set_command(&mut payload, &consts::CMD_SINGBOX_REMOVE.to_string())?;
        let payload = dumb::set_data(payload, &data)?;
        dumb::write_to_file(&payload, &file)?;
    }
    Ok(())
}
