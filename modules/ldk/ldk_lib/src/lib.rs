use lightning_invoice::Currency;
use std::{ffi::CStr, os::raw::c_char, str::FromStr};

#[no_mangle]
pub extern "C" fn ldk_des_invoice(input: *const c_char) -> bool {
    if input.is_null() {
        return false;
    }

    // Convert C string to Rust string
    let c_str = unsafe { CStr::from_ptr(input) };
    let c_str = match c_str.to_str() {
        Ok(s) => s,
        Err(_) => return false,
    };

    match lightning_invoice::SignedRawBolt11Invoice::from_str(c_str) {
        Ok(invoice) => {
            let is_signature_valid = invoice.check_signature() || invoice.recover_payee_pub_key().is_ok();
            let is_currency_bitcoin = invoice.currency() == Currency::Bitcoin;
            let has_payment_hash = invoice.payment_hash().is_some();

            has_payment_hash && is_currency_bitcoin && is_signature_valid
        }
        Err(_) => false,
    }
}
