//defined in dumb.h
pub const CODE_FILE_PATH: &str = "dumb-codes/";
//defined in server/src/dumbserver.h
pub const USERDATA_PREFIX: &str = "user/";
pub const SERVER_USER_PUBKEY_FILE: &str = "pubkey";
pub const CODE_FILE_PREFIX: &str = "code-";

//see dumb.h
pub const CMD_OK: &str = "ok";
pub const CMD_INSTALLTHIS: &str = "install-this";
pub const CMD_FILE_EXPORT: &str = "export-files";
pub const CMD_FILE_IMPORT: &str = "import-files";

//defined in C files that include these headers
pub const PUBKEY_HEADER: &str = "key_public.h";
pub const PRIVKEY_HEADER: &str = "key_private.h";
