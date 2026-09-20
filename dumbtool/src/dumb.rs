use std::ffi::CStr;
use std::ffi::CString;
use std::os::raw::c_char;
use std::os::raw::c_int;

#[repr(C)]
#[allow(non_camel_case_types)]
pub struct DUMB_PAYLOAD {
    _private: [u8; 0],
}

unsafe extern "C" {
    fn dp_create_new() -> *mut DUMB_PAYLOAD;
    fn dp_write_to_file(payload: *const DUMB_PAYLOAD, pathname: *const c_char) -> c_int;
    fn dp_get_command(payload: *const DUMB_PAYLOAD) -> *const c_char;
}

pub fn create_new() -> *mut DUMB_PAYLOAD {
    let new_payload: *mut DUMB_PAYLOAD;
    unsafe {
        new_payload = dp_create_new();
        if new_payload == std::ptr::null_mut() {
            let err = errno::errno();
            panic!("dp_create_new() failed:{} ({})", err.0, err);
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
        panic!("dp_write_to_file() failed: {} ({})", err.0, err);
    }
}

pub fn get_command(payload: *const DUMB_PAYLOAD) -> String {
    let command: *const c_char;
    unsafe {
        command = dp_get_command(payload);
        if command == std::ptr::null_mut() {
            let err = errno::errno();
            panic!("dp_get_command() failed:{} ({})", err.0, err);
        }
    }
    let command = unsafe { CStr::from_ptr(command).to_str().unwrap().to_owned() };
    return command;
}
