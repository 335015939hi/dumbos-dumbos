use crate::dumb;

pub fn get_raw(filelist: &Vec<String>) -> Result<(), String> {
    if filelist.len() == 0 {
        return Err("One of --code or --file must be specified".to_string());
    }

    for file in filelist {
        let payload = dumb::read_from_file(&file);
        let expire = dumb::get_expire_raw(&payload);
        println!("{file}:{expire}");
    }
    return Ok(());
}

pub fn set_raw(filelist: &Vec<String>, expire: &String) -> Result<(), String> {
    if filelist.len() == 0 {
        return Err("One of --code or --file must be specified".to_string());
    }

    for file in filelist {
        let mut payload = dumb::read_from_file(&file);
        dumb::set_expire_raw(&mut payload, &expire);
        dumb::write_to_file(&payload, &file);
    }
    return Ok(());
}

pub fn set(
    filelist: &Vec<String>,
    raw: &Option<String>,
    timestamp: &Option<u64>,
    relative: &Option<i32>,
) -> Result<(), String> {
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
    return Err("time not found".to_string());
}

pub fn get(filelist: &Vec<String>) -> Result<(), String> {
    for file in filelist {
        let payload = dumb::read_from_file(&file);
        let mut expire = dumb::get_expire_raw(&payload);
        let mut is_relative = false;
        let mut is_default = false;
        //see dumb.h
        let default_timeout = 60;
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
                default_timeout
            }
        };
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
