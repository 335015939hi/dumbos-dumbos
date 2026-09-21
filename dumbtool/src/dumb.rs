use std::ffi::CStr;
use std::ffi::CString;
use std::os::raw::c_char;
use std::os::raw::c_int;
use std::os::raw::c_void;

#[repr(C)]
#[allow(non_camel_case_types)]
struct DUMB_PAYLOAD {
    _private: [u8; 0],
}

pub struct DumbPayload {
    ptr: *mut DUMB_PAYLOAD,
}

impl Drop for DumbPayload {
    fn drop(&mut self) {
        if self.ptr == std::ptr::null_mut() {
            return;
        }
        unsafe {
            let ptr: *mut c_void = self.ptr.cast();
            free(ptr);
            self.ptr = std::ptr::null_mut();
        }
    }
}

unsafe extern "C" {
    //defined in dumb.h
    fn dp_create_new() -> *mut DUMB_PAYLOAD;
    fn dp_write_to_file(payload: *const DUMB_PAYLOAD, pathname: *const c_char) -> c_int;
    fn dp_get_command(payload: *const DUMB_PAYLOAD) -> *const c_char;
    fn dp_set_command(payload: *mut DUMB_PAYLOAD, command: *const c_char) -> c_int;
    fn dp_malloc_load(path: *const c_char, ret_size: *mut usize) -> *mut DUMB_PAYLOAD;
    fn dp_validate_size(payload: *const DUMB_PAYLOAD, detected_full_size: usize) -> bool;
    fn dp_set_data(
        payload: *mut DUMB_PAYLOAD,
        data: *const c_void,
        size: usize,
    ) -> *mut DUMB_PAYLOAD;
    fn dp_malloc_get_data(payload: *const DUMB_PAYLOAD, size_dest: *mut usize) -> *mut c_void;
    fn dp_get_expire_str(payload: *const DUMB_PAYLOAD) -> *const c_char;
    fn dp_set_expire_str(payload: *mut DUMB_PAYLOAD, expire_string: *const c_char) -> c_int;
    //other C functions
    fn free(buf: *mut c_void);
}

pub fn set_expire_raw(payload: &mut DumbPayload, expire: &String) {
    unsafe {
        let result =
            dp_set_expire_str(payload.ptr, CString::new(expire.as_str()).unwrap().as_ptr());
        if result != 0 {
            let err = errno::errno();
            panic!("dp_set_expire_str() failed:{} ({})", err.0, err);
        }
    }
}
pub fn get_expire_raw(payload: &DumbPayload) -> String {
    let expire: String;
    unsafe {
        let result = dp_get_expire_str(payload.ptr);
        if result == std::ptr::null_mut() {
            let err = errno::errno();
            panic!("dp_get_expire_str() failed:{} ({})", err.0, err);
        }
        expire = CStr::from_ptr(result).to_str().unwrap().to_owned();
    }
    return expire;
}

pub fn create_new() -> DumbPayload {
    let new_payload: *mut DUMB_PAYLOAD;
    unsafe {
        new_payload = dp_create_new();
        if new_payload == std::ptr::null_mut() {
            let err = errno::errno();
            panic!("dp_create_new() failed:{} ({})", err.0, err);
        }
    }
    return DumbPayload { ptr: new_payload };
}

pub fn write_to_file(payload: &DumbPayload, path: &String) {
    let result;
    unsafe {
        result = dp_write_to_file(payload.ptr, CString::new(path.as_str()).unwrap().as_ptr());
    }
    if result != 0 {
        let err = errno::errno();
        panic!("dp_write_to_file() failed: {} ({})", err.0, err);
    }
}

pub fn read_from_file(path: &String) -> DumbPayload {
    let mut size: usize = 0;
    let payload: *mut DUMB_PAYLOAD;
    unsafe {
        payload = dp_malloc_load(CString::new(path.as_str()).unwrap().as_ptr(), &mut size);
        if payload == std::ptr::null_mut() {
            let err = errno::errno();
            panic!("dp_malloc_load failed:{} ({})", err.0, err);
        }
        if !dp_validate_size(payload, size) {
            panic!("malformed payload detected (stated size doesn't match detected size)");
        }
    }
    return DumbPayload { ptr: payload };
}

pub fn get_command(payload: &DumbPayload) -> String {
    let command: *const c_char;
    unsafe {
        command = dp_get_command(payload.ptr);
        if command == std::ptr::null_mut() {
            let err = errno::errno();
            panic!("dp_get_command() failed:{} ({})", err.0, err);
        }
    }
    let command = unsafe { CStr::from_ptr(command).to_str().unwrap().to_owned() };
    return command;
}

pub fn set_command(payload: &mut DumbPayload, command: &String) {
    unsafe {
        let result = dp_set_command(
            payload.ptr,
            CString::new(command.as_str()).unwrap().as_ptr(),
        );
        if result != 0 {
            let err = errno::errno();
            panic!("dp_set_command() failed:{} ({})", err.0, err);
        }
    }
}

pub fn set_data(mut payload: DumbPayload, data: &Vec<u8>) -> DumbPayload {
    let size: usize = data.len();
    unsafe {
        let data: *const c_void = data.as_ptr().cast();
        let new_payload = dp_set_data(payload.ptr, data, size);
        if new_payload == std::ptr::null_mut() {
            let err = errno::errno();
            panic!("dp_set_data failed:{} ({})", err.0, err);
        }
        payload.ptr = new_payload;
    }
    return payload;
}

pub fn get_data(payload: &DumbPayload) -> Vec<u8> {
    let result: Vec<u8>;
    unsafe {
        let mut size: usize = 0;
        let data = dp_malloc_get_data(payload.ptr, &mut size);
        if data == std::ptr::null_mut() {
            let err = errno::errno();
            if err.0 == 0 {
                return vec![];
            } else {
                panic!("dp_malloc_get_data() failed:{} ({})", err.0, err);
            }
        }
        result = std::slice::from_raw_parts(data as *const u8, size).to_vec();
        free(data);
    }
    return result;
}
