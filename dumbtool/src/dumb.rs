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
    fn dp_malloc_load(path: *const c_char, ret_size: *mut usize) -> *mut DUMB_PAYLOAD;
    fn dp_validate_size(payload: *const DUMB_PAYLOAD, detected_full_size: usize) -> bool;
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

pub fn write_to_file(payload: *const DUMB_PAYLOAD, path: &String) {
    let result;
    unsafe {
        result = dp_write_to_file(payload, CString::new(path.as_str()).unwrap().as_ptr());
    }
    if result != 0 {
        let err = errno::errno();
        panic!("dp_write_to_file() failed: {} ({})", err.0, err);
    }
}

pub fn read_from_file(path: &String) -> *mut DUMB_PAYLOAD {
    let mut size: usize = 0;
    let payload: *mut DUMB_PAYLOAD;
    unsafe {
        payload = dp_malloc_load(CString::new(path.as_str()).unwrap().as_ptr(), &mut size);
        if payload == std::ptr::null_mut() {
            let err = errno::errno();
            panic!("dp_malloc_load failed:{} ({})", err.0, err);
        }
    }
    return payload;
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
