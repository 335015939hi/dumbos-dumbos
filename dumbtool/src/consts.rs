//defined in C header files or C files
//do not change without also changing the C code

//defined in dumb.h

//see dumb.h
pub const CMD_OK: &str = "ok";
pub const CMD_INSTALLTHIS: &str = "install-this";
pub const CMD_FILE_EXPORT: &str = "export-files";
pub const CMD_FILE_IMPORT: &str = "import-files";
pub const CMD_FW_ALLOW: &str = "fw-allow";
pub const CMD_FW_DENY: &str = "fw-deny";
pub const CMD_FW_FLUSH: &str = "fw-flush";

//defined in C files that include these headers
pub const PUBKEY_HEADER: &str = "key_public.h";
pub const PRIVKEY_HEADER: &str = "key_private.h";

//server-side only constants
//safely (mostly) change these without affecting phones

//path component of user-specific data
pub const USERDATA_PREFIX: &str = "user/";
//filename of user's public key
pub const SERVER_USER_PUBKEY_FILE: &str = "pubkey";
//filename prefix of a payload
pub const CODE_FILE_PREFIX: &str = "code-";
//default path prefix component
pub const CODE_FILE_PATH: &str = "dumb-codes/";
//path of the requestID table
pub const REQUESTID_TABLE_PATH: &str = "dumb-codes/request_ids.bin";
