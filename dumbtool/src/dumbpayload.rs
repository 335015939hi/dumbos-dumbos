use crate::consts;
use crate::dumb;
use crate::dumb::DumbPayload;
use crate::util;
use anyhow::Result;

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
    ///returns (is relative time,is default,timestamp in seconds)
    pub fn effective_expire(&self) -> Result<(bool, bool, i64)> {
        let mut expire = self.expire_raw()?;
        let mut is_relative = false;
        let mut is_default = false;
        if expire.len() != 0 {
            if expire.as_bytes()[0] == b':' {
                expire = expire.chars().skip(1).collect();
                is_relative = true;
            }
        }
        let expire: i64 = match expire.parse() {
            Ok(n) => n,
            Err(_e) => {
                is_relative = true;
                is_default = true;
                consts::DEFAULT_EXPIRE_TIMEOUT
            }
        };
        Ok((is_relative, is_default, expire))
    }
    pub fn set_expire_default(&mut self) -> Result<()> {
        self.set_expire_raw(&"_".into()) // any invalid string will do
    }
    pub fn set_expire_relative(&mut self, offset: i64) -> Result<()> {
        self.set_expire_raw(&format!(":{offset}"))
    }
    pub fn set_expire_timestamp(&mut self, time: i64) -> Result<()> {
        self.set_expire_raw(&format!("{time}"))
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
    pub fn clear_data(&mut self) -> Result<()> {
        self.set_data(&vec![])
    }
    pub fn sign(&mut self, key: &String) -> Result<()> {
        dumb::sign(self, key)
    }
    pub fn verify(&self, key: &String) -> Result<bool> {
        dumb::verify(self, key)
    }

    fn set_data_stringlist(&mut self, strings: &Vec<String>) -> Result<()> {
        let mut data: Vec<u8> = vec![];
        if strings.len() == 0 {
            data.push(0);
        }
        for string in strings {
            data.append(&mut string.clone().into_bytes());
            data.push(0);
        }
        self.set_data(&data)
    }
    fn set_command_no_data(&mut self, command: &str) -> Result<()> {
        self.set_command(&command.into())?;
        self.clear_data()
    }

    pub fn command_install_this_apk(&mut self, apk_path: &String) -> Result<()> {
        let apk_data = util::read_file(&apk_path)?;
        self.set_command(&consts::CMD_INSTALLTHIS.into())?;
        self.set_data(&apk_data)?;
        Ok(())
    }
    pub fn command_install_this_apkdata(&mut self, apk_data: &Vec<u8>) -> Result<()> {
        self.set_command(&consts::CMD_INSTALLTHIS.into())?;
        self.set_data(&apk_data)?;
        Ok(())
    }
    pub fn command_firewall_allow(&mut self, apps: &Vec<String>) -> Result<()> {
        self.set_command(&consts::CMD_FW_ALLOW.into())?;
        self.set_data_stringlist(apps)
    }
    pub fn command_firewall_deny(&mut self, apps: &Vec<String>) -> Result<()> {
        self.set_command(&consts::CMD_FW_DENY.into())?;
        self.set_data_stringlist(apps)
    }
    pub fn command_singbox_allow(&mut self, domains: &Vec<String>) -> Result<()> {
        self.set_command(&consts::CMD_SINGBOX_ADD.into())?;
        self.set_data_stringlist(domains)
    }
    pub fn command_singbox_deny(&mut self, domains: &Vec<String>) -> Result<()> {
        self.set_command(&consts::CMD_SINGBOX_REMOVE.into())?;
        self.set_data_stringlist(domains)
    }
    pub fn command_ok(&mut self) -> Result<()> {
        self.set_command_no_data(consts::CMD_OK)
    }
    pub fn command_file_export(&mut self) -> Result<()> {
        self.set_command_no_data(consts::CMD_FILE_EXPORT)
    }
    pub fn command_file_import(&mut self) -> Result<()> {
        self.set_command_no_data(consts::CMD_FILE_IMPORT)
    }
    pub fn command_firewall_flush(&mut self) -> Result<()> {
        self.set_command_no_data(consts::CMD_FW_FLUSH)
    }
}
