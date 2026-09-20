fn main() {
    for file in [
        "c/dumb.h",
        "c/dumbpayload.c",
        "c/common.h",
        "c/common.c",
    ] {
        println!("cargo:rerun-if-changed={file}");
    }

    cc::Build::new()
        .file("c/dumbpayload.c")
        .file("c/common.c")
        .include("c")
        .compile("foo");
}
