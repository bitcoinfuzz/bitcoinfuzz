use std::ffi::CString;
use std::os::raw::c_char;
use std::str::FromStr;

unsafe fn str_to_c_string(input: &str) -> *mut c_char {
    CString::new(input).unwrap().into_raw()
}

#[no_mangle]
pub unsafe extern "C" fn ldk_des_invoice(data: *const u8, len: usize) -> *mut c_char {
    if data.is_null() || len == 0 {
        return std::ptr::null_mut();
    }

    let data_slice = std::slice::from_raw_parts(data, len);
    let s = match std::str::from_utf8(data_slice) {
        Ok(v) => v,
        Err(_) => {
            return std::ptr::null_mut();
        }
    };

    let res = match lightning_invoice::Bolt11Invoice::from_str(s) {
        Ok(invoice) => format!("{:?}", invoice),
        Err(_) => {
            return std::ptr::null_mut();
        }
    };

    str_to_c_string(&res)
}
