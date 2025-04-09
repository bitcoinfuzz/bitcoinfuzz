use lightning::ln::msgs::{ChannelAnnouncement};
use lightning::util::ser::{Readable};
use lightning::io;
use lightning_invoice::{
    Bolt11InvoiceDescriptionRef, Bolt11SemanticError, Currency, ParseOrSemanticError,
};

use std::ffi::CString;
use std::os::raw::c_char;
use std::{ffi::CStr, str::FromStr};

unsafe fn str_to_c_string(input: &str) -> *mut c_char {
    CString::new(input).unwrap().into_raw()
}

#[no_mangle]
pub unsafe extern "C" fn ldk_des_invoice(input: *const std::os::raw::c_char) -> *mut c_char {
    if input.is_null() {
        return str_to_c_string("");
    }

    // Convert C string to Rust string
    let c_str = match CStr::from_ptr(input).to_str() {
        Ok(s) => s,
        Err(_) => return str_to_c_string(""),
    };

    match lightning_invoice::Bolt11Invoice::from_str(c_str) {
        Ok(invoice) => {
            if invoice.currency() != Currency::Bitcoin {
                return str_to_c_string("");
            }
            let mut result = String::new();

            result.push_str("HASH=");
            result.push_str(&invoice.payment_hash().to_string());

            result.push_str(";AMOUNT=");
            if let Some(amount) = invoice.amount_milli_satoshis() {
                result.push_str(&amount.to_string());
            }

            result.push_str(";DESCRIPTION=");
            if let Bolt11InvoiceDescriptionRef::Direct(direct_description) = invoice.description() {
                result.push_str(&direct_description.to_string());
            }

            let invoice_payee_pub_key = match invoice.payee_pub_key() {
                Some(payee) => payee.clone(),
                None => invoice.recover_payee_pub_key(),
            };

            result.push_str(";RECIPIENT=");
            result.push_str(&invoice_payee_pub_key.to_string());

            result.push_str(";EXPIRY=");
            result.push_str(&invoice.expiry_time().as_secs().to_string());

            result.push_str(";TIMESTAMP=");
            result.push_str(
                &invoice
                    .clone()
                    .into_signed_raw()
                    .raw_invoice()
                    .data
                    .timestamp
                    .as_unix_timestamp()
                    .to_string(),
            );

            result.push_str(";ROUTING_HINTS=");
            result.push_str(&invoice.private_routes().len().to_string());

            result.push_str(";MIN_CLTV=");
            result.push_str(&invoice.min_final_cltv_expiry_delta().to_string());

            str_to_c_string(&result)
        }
        // Handle invoices without payment secrets by returning null
        // This is needed because some Lightning implementations don't require payment secrets,
        // and we need to maintain compatibility with these implementations
        Err(ParseOrSemanticError::SemanticError(Bolt11SemanticError::NoPaymentSecret)) => {
            std::ptr::null_mut()
        }
        // Handle invoices with multiple payment hashes by returning null
        // This is needed because some Lightning implementations don't require payment to have only one hash,
        // and we need to maintain compatibility with these implementations
        Err(ParseOrSemanticError::SemanticError(Bolt11SemanticError::MultiplePaymentHashes)) => {
            std::ptr::null_mut()
        }
        // Handle invoices with multiple descriptions hashes by returning null
        // This is needed because some Lightning implementations don't require to have only one description,
        // and we need to maintain compatibility with these implementations
        Err(ParseOrSemanticError::SemanticError(Bolt11SemanticError::MultipleDescriptions)) => {
            std::ptr::null_mut()
        }
        Err(_) => str_to_c_string(""),
    }
}

#[no_mangle]
pub extern "C" fn ldk_free_string(ptr: *mut c_char) {
    if !ptr.is_null() {
        unsafe {
            let _ = CString::from_raw(ptr);
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn ldk_parse_channel_announcement(data: *const u8, len: usize) -> *mut c_char {
    if data.is_null() || len == 0 {
        return str_to_c_string("");
    }
    let bytes = std::slice::from_raw_parts(data, len);
    
    // parse data as ChannelAnnouncement message
    let mut cursor = io::Cursor::new(bytes);
    match ChannelAnnouncement::read(&mut cursor) {
        Ok(announcement) => {
            let mut result = String::new();
            
            result.push_str(&format!("NODE1={};", announcement.contents.node_id_1.to_string()));
            result.push_str(&format!("NODE2={};", announcement.contents.node_id_2.to_string()));
            result.push_str(&format!("BITCOIN_KEY1={};", announcement.contents.bitcoin_key_1.to_string()));
            result.push_str(&format!("BITCOIN_KEY2={};", announcement.contents.bitcoin_key_2.to_string()));
            result.push_str(&format!("SHORT_CHAN_ID={};", announcement.contents.short_channel_id));
                        
            str_to_c_string(&result)
        },
        Err(_) => str_to_c_string("")
    }
}