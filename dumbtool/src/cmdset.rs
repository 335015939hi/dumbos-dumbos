use crate::dumb;
use crate::util;

//see dumb.h
const CMD_OK: &str = "ok";
const CMD_INSTALLTHIS: &str = "install-this";
const CMD_FILE_EXPORT: &str = "export-files";
const CMD_FILE_IMPORT: &str = "import-files";

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
    return set_command(&filelist, CMD_OK);
}

pub fn install_this(filelist: &Vec<String>, apk: &String) -> Result<(), String> {
    let apkfile = util::read_file(&apk).unwrap();
    for file in filelist {
        let mut payload = dumb::read_from_file(file);
        dumb::set_command(&mut payload, &CMD_INSTALLTHIS.to_string());
        let payload = dumb::set_data(payload, &apkfile);
        dumb::write_to_file(&payload, &file);
    }
    Ok(())
}

pub fn file_export(filelist: &Vec<String>) -> Result<(), String> {
    return set_command(&filelist, CMD_FILE_EXPORT);
}

pub fn file_import(filelist: &Vec<String>) -> Result<(), String> {
    return set_command(&filelist, CMD_FILE_IMPORT);
}
