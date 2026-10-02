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
    ] {
        println!("cargo:rerun-if-changed={file}");
    }

    println!("cargo::rustc-link-lib=dylib=crypto");

    cc::Build::new()
        .file("c/dumbpayload.c")
        .file("c/common.c")
        .file("c/ed25519.c")
        .file("c/requestid.c")
        .include("c")
        .compile("foo");
}
