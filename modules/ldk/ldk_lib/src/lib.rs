use std::ffi::CString;
use std::os::raw::c_char;
use std::str::FromStr;

unsafe fn str_to_c_string(input: &str) -> *mut c_char {
    CString::new(input).unwrap().into_raw()
}

#[no_mangle]
pub unsafe extern "C" fn ldk_des_invoice(
    data: *const u8,
    len: usize,
    out_len: *mut usize,
) -> *mut c_char {
    let data_slice = std::slice::from_raw_parts(data, len);
    let s = std::str::from_utf8(data_slice).unwrap();
    let res = lightning_invoice::Bolt11Invoice::from_str(s);

    match res {
        Ok(invoice) => {
            let invoice_str = invoice.to_string();
            *out_len = invoice_str.len();
            str_to_c_string(&invoice_str)
        }
        Err(err) => {
            let err_str = err.to_string();
            *out_len = err_str.len();
            str_to_c_string(&err_str)
        }
    }
}
