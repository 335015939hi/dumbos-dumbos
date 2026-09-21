use crate::dumb;

pub fn getRaw(filelist: &Vec<String>) -> Result<(), String> {
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

pub fn setRaw(filelist: &Vec<String>, expire: &String) -> Result<(), String> {
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
