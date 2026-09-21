use crate::dumb;

pub fn main(
    code: Option<Vec<String>>,
    file: Option<Vec<String>>,
    command: &String,
) -> Result<(), String> {
    if code == None && file == None {
        return Err("One of --code or --file must be specified".to_string());
    }
    let mut filelist: Vec<String> = Vec::new();
    if code != None {
        for codename in code.unwrap() {
            filelist.push(format!("code-{}", codename));
        }
    }
    if file != None {
        for filename in file.unwrap() {
            filelist.push(filename);
        }
    }

    for file in filelist {
        let mut payload = dumb::read_from_file(&file);
        dumb::set_command(&payload, command);

        dumb::write_to_file(&payload, &file);
    }
    return Ok(());
}
