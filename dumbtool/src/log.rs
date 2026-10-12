use std::ffi::CString;
use std::ffi::c_char;
use std::ffi::c_int;

unsafe extern "C" {
    fn _log_(s: *const c_char);
    fn _log_verbose_(s: *const c_char);
    fn _log_debug_(s: *const c_char);
    fn _log_warn_(s: *const c_char);
    fn _log_error_(s: *const c_char);
    fn _log_fatal_(s: *const c_char);
    fn _log_errno_(s: *const c_char);
    fn _log_fatal_errno_(s: *const c_char);
    fn _log_warn_errno_(s: *const c_char);
    fn set_log_verbosity(s: *const c_char) -> c_int;
    fn get_log_verbosity() -> c_int;
}

pub fn set_verbosity(s: &str) -> Result<(), i32> {
    let r;
    unsafe {
        r = set_log_verbosity(CString::new(s).unwrap().as_ptr());
    }
    if r == 0 {
        return Ok(());
    }
    return Err(r);
}
pub fn get_verbosity() -> i32 {
    unsafe {
        return get_log_verbosity();
    }
}

pub fn log(s: &str) {
    unsafe {
        _log_(CString::new(s).unwrap().as_ptr());
    }
}
pub fn verbose(s: &str) {
    unsafe {
        _log_verbose_(CString::new(s).unwrap().as_ptr());
    }
}
pub fn debug(s: &str) {
    unsafe {
        _log_debug_(CString::new(s).unwrap().as_ptr());
    }
}
pub fn warn(s: &str) {
    unsafe {
        _log_warn_(CString::new(s).unwrap().as_ptr());
    }
}
pub fn error(s: &str) {
    unsafe {
        _log_error_(CString::new(s).unwrap().as_ptr());
    }
}
pub fn fatal(s: &str) {
    unsafe {
        _log_fatal_(CString::new(s).unwrap().as_ptr());
    }
}
pub fn errno(s: &str) {
    unsafe {
        _log_errno_(CString::new(s).unwrap().as_ptr());
    }
}
pub fn fatal_errno_(s: &str) {
    unsafe {
        _log_fatal_errno_(CString::new(s).unwrap().as_ptr());
    }
}
pub fn warn_errno(s: &str) {
    unsafe {
        _log_warn_errno_(CString::new(s).unwrap().as_ptr());
    }
}
