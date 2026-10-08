use fjall::Database;
use fjall::Keyspace;
use fjall::KeyspaceCreateOptions;
use fjall::PersistMode;

const REQUEST_ID_KEYSPACE: &str = "requestid";

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
