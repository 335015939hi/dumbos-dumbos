use dumbtool::consts;
use dumbtool::dumb;
use dumbtool::util;

pub fn main(directory: &String) -> Result<(), String> {
    let pubkey_path = format!("{directory}{}", consts::PUBKEY_HEADER);
    let privkey_path = format!("{directory}{}", consts::PRIVKEY_HEADER);
    let keys = dumb::ed25519_generate_keys()?;
    let pubkey = keys.0;
    let privkey = keys.1;
    let pubkey = format!(
        "#ifndef _PUBKEY_HEADER_H
#define _PUBKEY_HEADER_H
static const char _ed25519_public_key_hex[] = \"{}\";
#endif
",
        pubkey
    );
    let privkey = format!(
        "#ifndef _PUBKEY_HEADER_H
#define _PUBKEY_HEADER_H
static const char _ed25519_private_key_hex[] = \"{}\";
#endif
",
        privkey
    );
    util::write_file(&pubkey_path, &pubkey.into_bytes()).unwrap();
    util::write_file(&privkey_path, &privkey.into_bytes()).unwrap();
    Ok(())
}
