use lightning_invoice::Currency;
use std::{ffi::CStr, str::FromStr};

#[no_mangle]
pub unsafe extern "C" fn ldk_des_invoice(input: *const std::os::raw::c_char) -> bool {
    if input.is_null() {
        return false;
    }

    // Convert C string to Rust string
    let c_str = match CStr::from_ptr(input).to_str() {
        Ok(s) => s,
        Err(_) => return false,
    };

    match lightning_invoice::SignedRawBolt11Invoice::from_str(c_str) {
        Ok(invoice) => {
            // If the destination pubkey was provided as a tagged field, use that
            // to verify the signature, otherwise recover it from the signature
            let is_signature_valid = if let Some(_) = invoice.payee_pub_key() {
                invoice.check_signature()
            } else {
                invoice.recover_payee_pub_key().is_ok()
            };
            let is_currency_bitcoin = invoice.currency() == Currency::Bitcoin;
            let has_payment_hash = invoice.payment_hash().is_some();

            has_payment_hash && is_currency_bitcoin && is_signature_valid
        }
        Err(_) => false,
    }
}
