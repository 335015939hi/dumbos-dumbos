fn main() {
    for file in [
        "c/dumb.h",
        "c/dumbpayload.c",
        "c/common.h",
        "c/common.c",
        "c/ed25519.h",
        "c/ed25519.c",
        "c/requestid.h",
        "c/requestid.c",
        "c/key_private.h",
        "c/server_private_key.c",
    ] {
        println!("cargo:rerun-if-changed={file}");
    }

    println!("cargo::rustc-link-lib=dylib=crypto");

    let mut c = cc::Build::new();

    c.file("c/dumbpayload.c")
        .file("c/common.c")
        .file("c/ed25519.c")
        .file("c/requestid.c");

    let path = "c/key_private.h";
    let compile_privkey: bool;
    match std::fs::symlink_metadata(path) {
        Ok(meta) if meta.file_type().is_symlink() => {
            // It's a symlink.
            if std::fs::metadata(path).is_err() {
                compile_privkey = false;
            } else {
                compile_privkey = true;
            }
        }
        Ok(_) => {
            compile_privkey = true;
        }
        Err(e) => {
            // The path itself doesn't exist, or couldn't be inspected.
            println!("cargo:warning={path}: {e}");
            compile_privkey = false;
        }
    }

    if compile_privkey {
        c.file("c/server_private_key.c");
    }

    c.include("c").compile("dumbcommon");
}
