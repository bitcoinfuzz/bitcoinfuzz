use std::collections::HashMap;
use std::ffi::CStr;
use std::ffi::CString;
use std::os::raw::c_char;
use std::slice;
use std::str::Utf8Error;

use bitcoin::consensus::{deserialize_partial, encode};
use bitcoin::Block;

unsafe fn str_to_c_string(input: &str) -> *mut c_char {
    CString::new(input).unwrap().into_raw()
}

#[no_mangle]
pub unsafe extern "C" fn rust_bitcoin_des_block(
    data: *const u8,
    len: usize,
    out_len: *mut usize,
) -> *mut c_char {
    let data_slice = std::slice::from_raw_parts(data, len);
    let res = deserialize_partial::<Block>(data_slice);

    match res {
        Ok(res) => {
            let check = res.0.check_merkle_root() && res.0.check_witness_commitment();
            return str_to_c_string(check.to_string().as_str());
        }
        Err(err) => {
            if err.to_string().starts_with("unsupported segwit version") {
                return str_to_c_string("unsupported segwit version");
            }
            return str_to_c_string("0");
        }
    };
}

#[no_mangle]
pub unsafe extern "C" fn rust_bitcoin_script(data: *const u8, len: usize) -> bool {
    // Safety: Ensure that the data pointer is valid for the given length
    let data_slice = slice::from_raw_parts(data, len);

    let script: Result<(bitcoin::script::ScriptBuf, usize), encode::Error> =
        encode::deserialize_partial(data_slice);
    match script {
        Err(_) => false,
        Ok(s) => {
            if s.0.is_op_return() || s.0.len() > 10_000 {
                return false;
            }
            true
        }
    }
}

unsafe fn c_str_to_str<'a>(input: *const c_char) -> Result<&'a str, Utf8Error> {
    CStr::from_ptr(input).to_str()
}
