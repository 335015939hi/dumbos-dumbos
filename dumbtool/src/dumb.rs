use std::ffi::CStr;
use std::ffi::CString;
use std::os::raw::c_char;
use std::os::raw::c_int;
use std::os::raw::c_void;
use std::ptr;

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

pub fn u8_to_dumbpayload_nocheck(data: &Vec<u8>) -> Result<DumbPayload, String> {
    unsafe {
        let ptr = libc::malloc(data.len()) as *mut u8;

        if ptr.is_null() {
            let err = errno::errno();
            return Err(format!("malloc failed:{} (OS error {})", err, err.0));
        }

        ptr::copy_nonoverlapping(data.as_ptr(), ptr, data.len());

        Ok(DumbPayload {
            ptr: ptr as *mut DUMB_PAYLOAD,
        })
    }
}

pub fn u8_to_dumbpayload(data: &Vec<u8>) -> Result<DumbPayload, String> {
    unsafe {
        if data.len() < dp_get_base_size() {
            return Err(String::from("invalid payload"));
        }
    }
    let payload = u8_to_dumbpayload_nocheck(data)?;
    unsafe {
        if dp_validate_size(payload.ptr, data.len()) == false {
            return Err(String::from("invalid payload"));
        }
    }
    Ok(payload)
}

// danger!!! a payload with an invalid size, or reporting a invalid data size, could cause bad things to happen
pub fn dumbpayload_to_u8(payload: &DumbPayload) -> Vec<u8> {
    let size: usize;
    let result: Vec<u8>;
    unsafe {
        size = dp_get_base_size() + dp_get_data_size(payload.ptr) as usize;
        result = std::slice::from_raw_parts(payload.ptr as *const u8, size).to_vec();
    }
    result
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
    fn dumb_code_verify_chars(code: *const c_char) -> bool;
    fn dp_get_base_size() -> usize;
    fn dp_get_data_size(payload: *const DUMB_PAYLOAD) -> isize;
    fn dp_is_expired(payload: *mut DUMB_PAYLOAD) -> bool;
    fn dp_sign(paylod: *mut DUMB_PAYLOAD, size: usize, private_key_hex: *const c_char) -> c_int;
    fn dp_verify(payload: *mut DUMB_PAYLOAD, size: usize, pubkey_hex: *const c_char) -> c_int;
    //defined in requestid.h
    fn request_id_verify(
        user: *const c_char,
        request_id: *const c_char,
        request_time_str: *const c_char,
        signature: *const c_char,
        pub_key_hex: *const c_char,
    ) -> c_int;
    fn dumbos_alloc_new_user(user: *const c_char, priv_key_hex: *const c_char) -> *mut c_void;
    fn dumbos_user_data_size() -> usize;
    fn dumbos_set_fancy_name(data: *mut c_void, name: *const c_char) -> c_int;
    fn request_id_verify_chars(request: *const c_char) -> bool;
    fn dumbos_user_verify_chars(user: *const c_char) -> bool;
    //defined in ed25519.h
    fn ed25519_generate_keypair_hex(public_hex: *mut c_char, private_hex: *mut c_char) -> c_int;
    //other C functions
    fn free(buf: *mut c_void);
}

pub fn ed25519_generate_keys() -> Result<(String, String), String> {
    //defined in ed25519.h
    let public_key_hex_size = 65;
    //defined in ed25519.h
    let private_key_hex_size = 65;
    let mut public_key: Vec<u8> = vec![0; public_key_hex_size];
    let mut private_key: Vec<u8> = vec![0; private_key_hex_size];
    unsafe {
        let status = ed25519_generate_keypair_hex(
            public_key.as_mut_ptr() as *mut c_char,
            private_key.as_mut_ptr() as *mut c_char,
        );
        if status != 0 {
            let err = errno::errno();
            return Err(format!(
                "ed25519_generate_keypair_hex() failed:{} ({err})",
                err.0
            ));
        }
        let public_key = CStr::from_ptr(public_key.as_ptr() as *const c_char)
            .to_str()
            .unwrap()
            .to_owned();
        let private_key = CStr::from_ptr(private_key.as_ptr() as *const c_char)
            .to_str()
            .unwrap()
            .to_owned();
        return Ok((public_key, private_key));
    }
}

pub fn set_expire_raw(payload: &mut DumbPayload, expire: &String) -> Result<(), String> {
    unsafe {
        let result =
            dp_set_expire_str(payload.ptr, CString::new(expire.as_str()).unwrap().as_ptr());
        if result != 0 {
            let err = errno::errno();
            return Err(format!("dp_set_expire_str() failed:{} ({})", err.0, err));
        }
    }
    Ok(())
}
pub fn get_expire_raw(payload: &DumbPayload) -> Result<String, String> {
    let expire: String;
    unsafe {
        let result = dp_get_expire_str(payload.ptr);
        if result == std::ptr::null_mut() {
            let err = errno::errno();
            return Err(format!("dp_get_expire_str() failed:{} ({})", err.0, err));
        }
        expire = CStr::from_ptr(result).to_str().unwrap().to_owned();
    }
    return Ok(expire);
}

pub fn create_new() -> Result<DumbPayload, String> {
    let new_payload: *mut DUMB_PAYLOAD;
    unsafe {
        new_payload = dp_create_new();
        if new_payload == std::ptr::null_mut() {
            let err = errno::errno();
            return Err(format!("dp_create_new() failed:{} ({})", err.0, err));
        }
    }
    return Ok(DumbPayload { ptr: new_payload });
}

pub fn write_to_file(payload: &DumbPayload, path: &String) -> Result<(), String> {
    let result;
    unsafe {
        result = dp_write_to_file(payload.ptr, CString::new(path.as_str()).unwrap().as_ptr());
    }
    if result != 0 {
        let err = errno::errno();
        return Err(format!("dp_write_to_file() failed: {} ({})", err.0, err));
    }
    Ok(())
}

pub fn read_from_file(path: &String) -> Result<DumbPayload, String> {
    let mut size: usize = 0;
    let payload: *mut DUMB_PAYLOAD;
    unsafe {
        payload = dp_malloc_load(CString::new(path.as_str()).unwrap().as_ptr(), &mut size);
        if payload == std::ptr::null_mut() {
            let err = errno::errno();
            return Err(format!("dp_malloc_load failed:{} ({})", err.0, err));
        }
        if !dp_validate_size(payload, size) {
            return Err(format!(
                "malformed payload detected (stated size doesn't match detected size)"
            ));
        }
    }
    return Ok(DumbPayload { ptr: payload });
}

pub fn get_command(payload: &DumbPayload) -> Result<String, String> {
    let command: *const c_char;
    unsafe {
        command = dp_get_command(payload.ptr);
        if command == std::ptr::null_mut() {
            let err = errno::errno();
            return Err(format!("dp_get_command() failed:{} ({})", err.0, err));
        }
    }
    let command = unsafe { CStr::from_ptr(command).to_str().unwrap().to_owned() };
    Ok(command)
}

pub fn set_command(payload: &mut DumbPayload, command: &String) -> Result<(), String> {
    unsafe {
        let result = dp_set_command(
            payload.ptr,
            CString::new(command.as_str()).unwrap().as_ptr(),
        );
        if result != 0 {
            let err = errno::errno();
            return Err(format!("dp_set_command() failed:{} ({})", err.0, err));
        }
    }
    Ok(())
}

pub fn set_data(mut payload: DumbPayload, data: &Vec<u8>) -> Result<DumbPayload, String> {
    let size: usize = data.len();
    unsafe {
        let data: *const c_void = data.as_ptr().cast();
        let new_payload = dp_set_data(payload.ptr, data, size);
        if new_payload == std::ptr::null_mut() {
            let err = errno::errno();
            return Err(format!("dp_set_data failed:{} ({})", err.0, err));
        }
        payload.ptr = new_payload;
    }
    return Ok(payload);
}

pub fn get_data(payload: &DumbPayload) -> Result<Vec<u8>, String> {
    let result: Vec<u8>;
    unsafe {
        let mut size: usize = 0;
        let data = dp_malloc_get_data(payload.ptr, &mut size);
        if data == std::ptr::null_mut() {
            let err = errno::errno();
            if err.0 == 0 {
                return Ok(vec![]);
            } else {
                return Err(format!("dp_malloc_get_data() failed:{} ({})", err.0, err));
            }
        }
        result = std::slice::from_raw_parts(data as *const u8, size).to_vec();
        free(data);
    }
    return Ok(result);
}

pub fn make_user(username: &String, priv_key: &String) -> Result<Vec<u8>, String> {
    let data: Vec<u8>;
    unsafe {
        let size = dumbos_user_data_size();
        let data_raw = dumbos_alloc_new_user(
            CString::new(username.as_str()).unwrap().as_ptr(),
            CString::new(priv_key.as_str()).unwrap().as_ptr(),
        );
        if data_raw == std::ptr::null_mut() {
            let err = errno::errno();
            return Err(format!(
                "dumbos_alloc_new_user() failed:{} ({})",
                err.0, err
            ));
        }
        data = std::slice::from_raw_parts(data_raw as *const u8, size).to_vec();
    }
    Ok(data)
}

pub fn user_set_fancy_name(user_data: &mut Vec<u8>, fancy_name: &String) -> Result<(), String> {
    let fancy_name =
        CString::new(fancy_name.as_str()).expect("String contained internal null byte");
    let fancy_name = fancy_name.as_ptr();
    unsafe {
        if user_data.len() != dumbos_user_data_size() {
            return Err(format!("user_set_fancy_name(): recieved invalid data"));
        }
        let status = dumbos_set_fancy_name(user_data.as_mut_ptr() as *mut c_void, fancy_name);
        if status != 0 {
            let err = errno::errno();
            return Err(format!(
                "dumbos_set_fancy_name() failed:{} ({})",
                err.0, err
            ));
        }
    }
    Ok(())
}

pub fn verify_chars_requestid(requestid: &String) -> bool {
    unsafe {
        request_id_verify_chars(CString::new(requestid.as_str()).unwrap().as_ptr() as *const c_char)
    }
}

pub fn verify_chars_username(username: &String) -> bool {
    unsafe {
        dumbos_user_verify_chars(CString::new(username.as_str()).unwrap().as_ptr() as *const c_char)
    }
}

pub fn verify_chars_secretcode(code: &String) -> bool {
    unsafe {
        dumb_code_verify_chars(CString::new(code.as_str()).unwrap().as_ptr() as *const c_char)
    }
}

pub fn verify_dumbos_request(
    user: &String,
    request_id: &String,
    request_time: u64,
    signature: &String,
    pubkey_hex: &String,
) -> Result<(), String> {
    let request_time = format!("{request_time}");
    let verify_result;
    unsafe {
        verify_result = request_id_verify(
            CString::new(user.as_str()).unwrap().as_ptr(),
            CString::new(request_id.as_str()).unwrap().as_ptr(),
            CString::new(request_time.as_str()).unwrap().as_ptr(),
            CString::new(signature.as_str()).unwrap().as_ptr(),
            CString::new(pubkey_hex.as_str()).unwrap().as_ptr(),
        );
    }
    if verify_result != 0 {
        let err = errno::errno();
        return Err(err.to_string());
    }
    Ok(())
}

pub fn check_expire_and_set(payload: &mut DumbPayload) -> bool {
    unsafe {
        return dp_is_expired(payload.ptr);
    }
}

pub fn sign(payload: &mut DumbPayload, priv_key: &String) -> Result<(), String> {
    let result;
    unsafe {
        let size = dp_get_data_size(payload.ptr) as usize + dp_get_base_size();
        if dp_validate_size(payload.ptr, size) == false {
            return Err("invalid payload".into());
        }
        result = dp_sign(
            payload.ptr,
            size,
            CString::new(priv_key.as_str()).unwrap().as_ptr(),
        );
    }
    if result != 0 {
        let err = errno::errno();
        return Err(format!("dp_sign() failed:{} (OS error {})", err, err.0));
    }
    Ok(())
}

pub fn verify(payload: &DumbPayload, pub_key: &String) -> Result<(), String> {
    let payload = u8_to_dumbpayload(&dumbpayload_to_u8(payload))?;
    let size;
    let result;
    unsafe {
        size = dp_get_base_size() + dp_get_data_size(payload.ptr) as usize;
        result = dp_verify(
            payload.ptr,
            size,
            CString::new(pub_key.as_str()).unwrap().as_ptr(),
        );
    }
    if result != 0 {
        let err = errno::errno();
        return Err(format!(
            "payload verify failed:{} (OS error {})",
            err, err.0
        ));
    }
    Ok(())
}
