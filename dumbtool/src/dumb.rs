use std::ffi::CString;

#[repr(C)]
#[allow(non_camel_case_types)]
pub struct DUMB_PAYLOAD {
    _private: [u8; 0],
}

unsafe extern "C" {
    fn dp_create_new() -> *mut DUMB_PAYLOAD;
    fn dp_write_to_file(
        payload: *const DUMB_PAYLOAD,
        pathname: *const std::os::raw::c_char,
    ) -> std::os::raw::c_int;
}

pub fn create_new() -> *mut DUMB_PAYLOAD {
    let new_payload: *mut DUMB_PAYLOAD;
    unsafe {
        new_payload = dp_create_new();
        if new_payload == std::ptr::null_mut() {
            let err = errno::errno();
            panic!("dp_create_new() failed:{}: {}", err.0, err);
        }
    }
    return new_payload;
}

pub fn write_to_file(payload: *const DUMB_PAYLOAD, path: String) {
    let result;
    unsafe {
        result = dp_write_to_file(payload, CString::new(path).unwrap().as_ptr());
    }
    if result != 0 {
        let err = errno::errno();
        panic!("dp_write_to_file() failed: {}: {}", err.0, err);
    }
}
