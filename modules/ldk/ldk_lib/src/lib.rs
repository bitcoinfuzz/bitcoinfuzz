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
pub extern "C" fn ldk_construct_htlc_success_tx(commitment_tx_hex: *const c_char, htlc_index: u32, preimage: *const c_char, fee_rate: u64) -> *mut c_char {
    if commitment_tx_hex.is_null() || preimage.is_null() {
        return unsafe { str_to_c_string("") };
    }
    
    let _commitment_tx_hex_str = unsafe {
        match CStr::from_ptr(commitment_tx_hex).to_str() {
            Ok(s) => s,
            Err(_) => return str_to_c_string(""),
        }
    };
    
    let _preimage_str = unsafe {
        match CStr::from_ptr(preimage).to_str() {
            Ok(s) => s,
            Err(_) => return str_to_c_string(""),
        }
    };
    
    let mut tx_bytes = Vec::new();
    
    // Version (4 bytes)
    tx_bytes.extend_from_slice(&2u32.to_le_bytes());
    
    // Input count (1 byte varint)
    tx_bytes.push(1);
    
    // Previous outpoint (32 bytes txid + 4 bytes vout)
    tx_bytes.extend_from_slice(&[0; 32]); // Dummy txid
    tx_bytes.extend_from_slice(&htlc_index.to_le_bytes()); // vout
    
    // Script length (1 byte varint)
    tx_bytes.push(0);
    
    // Sequence (4 bytes)
    tx_bytes.extend_from_slice(&0xFFFFFFFFu32.to_le_bytes());
    
    // Output count (1 byte varint)
    tx_bytes.push(1);
    
    // Output value (8 bytes)
    let fee = fee_rate * 250 / 1000;
    let output_value = 1000000 - fee;
    tx_bytes.extend_from_slice(&output_value.to_le_bytes());
    
    // Output script length (1 byte varint)
    tx_bytes.push(22); // P2WPKH script is 22 bytes
    
    // Output script (P2WPKH)
    tx_bytes.push(0x00); // OP_0
    tx_bytes.push(0x14); // Push 20 bytes
    tx_bytes.extend_from_slice(&[0; 20]); // Dummy pubkey hash
    
    // Locktime (4 bytes)
    tx_bytes.extend_from_slice(&0u32.to_le_bytes());
    
    let tx_hex = hex::encode(tx_bytes);
    
    unsafe { str_to_c_string(&tx_hex) }
}

#[no_mangle]
pub extern "C" fn ldk_construct_htlc_timeout_tx(commitment_tx_hex: *const c_char, htlc_index: u32, cltv_expiry: u32, fee_rate: u64) -> *mut c_char {
    if commitment_tx_hex.is_null() {
        return unsafe { str_to_c_string("") };
    }
    
    let _commitment_tx_hex_str = unsafe {
        match CStr::from_ptr(commitment_tx_hex).to_str() {
            Ok(s) => s,
            Err(_) => return str_to_c_string(""),
        }
    };
    
    
    let mut tx_bytes = Vec::new();
    
    // Version (4 bytes)
    tx_bytes.extend_from_slice(&2u32.to_le_bytes());
    
    // Input count (1 byte varint)
    tx_bytes.push(1);
    
    // Previous outpoint (32 bytes txid + 4 bytes vout)
    tx_bytes.extend_from_slice(&[0; 32]); // Dummy txid
    tx_bytes.extend_from_slice(&htlc_index.to_le_bytes()); // vout
    
    // Script length (1 byte varint)
    tx_bytes.push(0);
    
    // Sequence (4 bytes)
    tx_bytes.extend_from_slice(&0u32.to_le_bytes()); // Enable locktime
    
    // Output count (1 byte varint)
    tx_bytes.push(1);
    
    // Output value (8 bytes)
    let fee = fee_rate * 250 / 1000; // Simplified fee calculation
    let output_value = 1000000 - fee; // Dummy value minus fee
    tx_bytes.extend_from_slice(&output_value.to_le_bytes());
    
    // Output script length (1 byte varint)
    tx_bytes.push(22); // P2WPKH script is 22 bytes
    
    // Output script (P2WPKH)
    tx_bytes.push(0x00); // OP_0
    tx_bytes.push(0x14); // Push 20 bytes
    tx_bytes.extend_from_slice(&[0; 20]); // Dummy pubkey hash
    
    // Locktime (4 bytes) - set to CLTV expiry
    tx_bytes.extend_from_slice(&cltv_expiry.to_le_bytes());
    
    // Convert to hex
    let tx_hex = hex::encode(tx_bytes);
    
    unsafe { str_to_c_string(&tx_hex) }
}

#[no_mangle]
pub extern "C" fn ldk_free_string(ptr: *mut c_char) {
    if !ptr.is_null() {
        unsafe {
            let _ = CString::from_raw(ptr);
        }
    }
}