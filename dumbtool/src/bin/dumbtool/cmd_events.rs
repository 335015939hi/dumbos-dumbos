use anyhow::Result;
use anyhow::bail;
use dumbtool::consts;
use dumbtool::tables::EventsTable;
use fjall::Database;

pub fn main(directory: &String, clear: bool) -> Result<()> {
    let db = match Database::builder(format!("{directory}/{}", consts::FJALL_DB_FILENAME)).open() {
        Ok(v) => v,
        Err(e) => {
            bail!("error opening database: {e}");
        }
    };

    let mut table = match EventsTable::open(&db) {
        Ok(v) => v,
        Err(e) => {
            bail!("error opening table: {e}");
        }
    };

    if clear {
        match table.clear() {
            Ok(_) => {}
            Err(e) => {
                bail!("error clearing table: {e}");
            }
        };
    } else {
        for index in 0..table.len() {
            let event = match table.get(index) {
                Ok(v) => v,
                Err(e) => {
                    bail!("error getting event {index}: {e}");
                }
            };
            match event {
                Some(event) => {
                    event.print();
                }
                None => {
                    println!("event at index {index} misssing");
                }
            }
        }
    }

    Ok(())
}
