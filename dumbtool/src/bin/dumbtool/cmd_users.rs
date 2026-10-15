use anyhow::Result;

pub fn create(directory: &String, users: &Vec<String>) -> Result<()> {
    for user in users {
        dumbtool::dumbutil::create_user(directory, user)?;
    }
    Ok(())
}
