
use fjall::Database;
use fjall::Keyspace;
use fjall::KeyspaceCreateOptions;

use crate::events;

const REQUEST_ID_KEYSPACE: &str = "requestid";
const EVENTS_KEYSPACE: &str = "dumbEvents";

pub struct RequestIdTable {
    keyspace: Keyspace,
}

impl RequestIdTable {
    pub fn open(db: &Database) -> fjall::Result<Self> {
        Ok(Self {
            keyspace: db.keyspace(REQUEST_ID_KEYSPACE, KeyspaceCreateOptions::default)?,
        })
    }
    pub fn insert(&self, key: &str) -> fjall::Result<()> {
        self.keyspace.insert(key, [])?;
        Ok(())
    }
    pub fn exists(&self, key: &str) -> fjall::Result<bool> {
        self.keyspace.contains_key(key)
    }
}

pub struct EventsTable {
    keyspace: Keyspace,
    len: u64,
}

impl EventsTable {
    pub fn open(db: &Database) -> fjall::Result<Self> {
        let keyspace = db.keyspace(EVENTS_KEYSPACE, KeyspaceCreateOptions::default)?;
        let len = keyspace.len()?;
        Ok(Self {
            keyspace: keyspace,
            len: len as u64,
        })
    }
    pub fn push(&mut self, event: &events::Event) -> fjall::Result<()> {
        let value = match postcard::to_stdvec(&event) {
            Ok(v) => v,
            Err(e) => {
                panic!("{e}");
            }
        };
        self.keyspace.insert(self.len.to_be_bytes(), &value)?;
        self.len += 1;
        Ok(())
    }
    pub fn len(&self) -> u64 {
        self.len
    }
    pub fn get(&self, index: u64) -> fjall::Result<Option<events::Event>> {
        let val = self.keyspace.get(index.to_be_bytes())?;
        match val {
            None => Ok(None),
            Some(val) => match postcard::from_bytes(&*val) {
                Ok(v) => Ok(Some(v)),
                Err(e) => panic!("{e}"),
            },
        }
    }
    pub fn clear(&mut self) -> fjall::Result<()> {
        self.keyspace.clear()?;
        self.len = 0;
        Ok(())
    }
}
