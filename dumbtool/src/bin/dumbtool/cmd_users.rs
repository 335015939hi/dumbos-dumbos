pub fn create(directory: &String, users: &Vec<String>) -> Result<(), String> {
    for user in users {
        match dumbtool::dumbutil::create_user(directory, user) {
            Ok(_) => {}
            Err(e) => {
                return Err(e.to_string());
            }
        }
    }
    Ok(())
}
