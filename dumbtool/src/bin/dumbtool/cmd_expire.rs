use anyhow::Result;
use anyhow::bail;
use dumbtool::dumb::DumbPayload;

pub fn get_raw(filelist: &Vec<String>) -> Result<()> {
    for file in filelist {
        let payload = DumbPayload::from_file(&file)?;
        let expire = payload.expire_raw()?;
        println!("{file}:{expire}");
    }
    return Ok(());
}

pub fn set_raw(filelist: &Vec<String>, expire: &String) -> Result<()> {
    for file in filelist {
        let mut payload = DumbPayload::from_file(&file)?;
        payload.set_expire_raw(&expire)?;
        payload.write_to_file(&file)?;
    }
    return Ok(());
}

pub fn set(
    filelist: &Vec<String>,
    raw: &Option<String>,
    timestamp: &Option<u64>,
    relative: &Option<i32>,
) -> Result<()> {
    if *raw != None {
        let s = raw.as_ref().unwrap();
        return set_raw(filelist, &s);
    }
    if *timestamp != None {
        let s = format!("{}", timestamp.unwrap());
        return set_raw(filelist, &s);
    }
    if *relative != None {
        let s = format!(":{}", relative.unwrap());
        return set_raw(filelist, &s);
    }
    bail!("time not found");
}

pub fn get(filelist: &Vec<String>) -> Result<()> {
    for file in filelist {
        let payload = DumbPayload::from_file(&file)?;
        let expire = payload.effective_expire()?;
        let is_relative = expire.0;
        let is_default = expire.1;
        let expire = expire.2;
        let mut str = format!("{file}: ");
        if is_relative {
            str = format!("{str}+{expire}");
        } else {
            str = format!("{str}{expire}");
            let expire = chrono::DateTime::from_timestamp(expire, 0).unwrap();
            str = format!("{str} ({})", expire.format("%Y-%m-%d %H:%M:%S %:z"));
        }
        if is_default {
            str = format!("{str} (default)");
        }
        println!("{str}");
    }
    Ok(())
}
