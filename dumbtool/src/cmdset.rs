use crate::consts;
use crate::dumb;
use crate::util;

fn set_command(filelist: &Vec<String>, command: &str) -> Result<(), String> {
    for file in filelist {
        let mut payload = dumb::read_from_file(file);
        dumb::set_command(&mut payload, &command.to_string());
        let payload = dumb::set_data(payload, &vec![]);
        dumb::write_to_file(&payload, &file);
    }
    Ok(())
}

pub fn ok(filelist: &Vec<String>) -> Result<(), String> {
    return set_command(&filelist, consts::CMD_OK);
}

pub fn install_this(filelist: &Vec<String>, apk: &String) -> Result<(), String> {
    let apkfile = util::read_file(&apk).unwrap();
    for file in filelist {
        let mut payload = dumb::read_from_file(file);
        dumb::set_command(&mut payload, &consts::CMD_INSTALLTHIS.to_string());
        let payload = dumb::set_data(payload, &apkfile);
        dumb::write_to_file(&payload, &file);
    }
    Ok(())
}

pub fn file_export(filelist: &Vec<String>) -> Result<(), String> {
    return set_command(&filelist, consts::CMD_FILE_EXPORT);
}

pub fn file_import(filelist: &Vec<String>) -> Result<(), String> {
    return set_command(&filelist, consts::CMD_FILE_IMPORT);
}
pub fn firewall_flush(filelist: &Vec<String>) -> Result<(), String> {
    return set_command(&filelist, consts::CMD_FW_FLUSH);
}

pub fn firewall_allow(filelist: &Vec<String>, apps: &Vec<String>) -> Result<(), String> {
    let mut data: Vec<u8> = vec![0];
    if apps.len() == 0 {
        panic!("Error: no apps specified");
    }
    for app in apps {
        let mut app = app.clone().into_bytes();
        app.push(0);
        data.append(&mut app);
    }
    for file in filelist {
        let mut payload = dumb::read_from_file(file);
        dumb::set_command(&mut payload, &consts::CMD_FW_ALLOW.to_string());
        let payload = dumb::set_data(payload, &data);
        dumb::write_to_file(&payload, &file);
    }
    Ok(())
}

pub fn firewall_deny(filelist: &Vec<String>, apps: &Vec<String>) -> Result<(), String> {
    let mut data: Vec<u8> = vec![0];
    if apps.len() == 0 {
        panic!("Error: no apps specified");
    }
    for app in apps {
        let mut app = app.clone().into_bytes();
        app.push(0);
        data.append(&mut app);
    }
    for file in filelist {
        let mut payload = dumb::read_from_file(file);
        dumb::set_command(&mut payload, &consts::CMD_FW_DENY.to_string());
        let payload = dumb::set_data(payload, &data);
        dumb::write_to_file(&payload, &file);
    }
    Ok(())
}
