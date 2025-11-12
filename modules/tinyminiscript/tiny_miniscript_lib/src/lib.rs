use std::ffi::CStr;
use std::os::raw::c_char;
use std::str::Utf8Error;

use tinyminiscript::Context;

unsafe fn c_str_to_str<'a>(input: *const c_char) -> Result<&'a str, Utf8Error> {
    CStr::from_ptr(input).to_str()
}

#[no_mangle]
pub unsafe extern "C" fn tiny_miniscript_descriptor_parse(input: *const c_char) -> bool {
    let Ok(desc) = c_str_to_str(input) else {
        return false;
    };

    let ts_descriptor = Context::try_from(desc);
    match ts_descriptor {
        Err(_) => false,
        Ok(_) => true,
    }
}