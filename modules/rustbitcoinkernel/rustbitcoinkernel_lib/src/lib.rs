use bitcoinkernel::core::{ScriptPubkeyExt, TransactionExt, TxInExt, TxOutExt, TxOutPointExt};
use bitcoinkernel::{Block, Transaction};
use std::ffi::CString;
use std::os::raw::c_char;
use std::slice;

extern crate bitcoinkernel;

unsafe fn str_to_c_string(input: &str) -> *mut c_char {
    CString::new(input).unwrap().into_raw()
}

/// Frees a C string created by `str_to_c_string`.
///
/// # Safety
/// The pointer must have been created by `str_to_c_string` and not yet freed.
/// After calling this function, the pointer is invalid and must not be used.
#[no_mangle]
pub unsafe extern "C" fn kernel_free_c_string(ptr: *mut c_char) {
    if !ptr.is_null() {
        // Convert the raw pointer back to a CString, which will be dropped
        // and free the memory when it goes out of scope
        let _ = CString::from_raw(ptr);
    }
}

#[no_mangle]
pub unsafe extern "C" fn rustbitcoinkernel_transaction(data: *const u8, len: usize) -> *mut c_char {
    // Safety: Ensure that the data pointer is valid for the given length
    let data_slice = slice::from_raw_parts(data, len);
    let Ok(tx) = Transaction::new(data_slice) else {
        return str_to_c_string("0");
    };

    let mut result = String::new();
    result.push_str(&format!("txid={};", tx.txid().to_string()));

    for input in tx.inputs() {
        result.push_str(&format!("index={}", input.outpoint().index().to_string()));
        result.push_str(&format!("txid={};", input.outpoint().txid().to_string()));
    }

    for output in tx.outputs() {
        result.push_str(&format!("amount={}", output.value().to_string()));
        let script_hex = output
            .script_pubkey()
            .to_bytes()
            .iter()
            .map(|b| format!("{:02x}", b))
            .collect::<String>();
        result.push_str(&format!("script_pubkey={};", script_hex));
    }

    str_to_c_string(&result)
}

#[no_mangle]
pub unsafe extern "C" fn rustbitcoinkernel_block(data: *const u8, len: usize) -> *mut c_char {
    // Safety: Ensure that the data pointer is valid for the given length
    let data_slice = slice::from_raw_parts(data, len);
    let Ok(block) = Block::new(data_slice) else {
        return str_to_c_string("0");
    };

    let mut result = String::new();
    result.push_str(&block.hash().to_string());

    for tx in block.transactions() {
        result.push_str(&format!("txid={};", tx.txid().to_string()));
    }

    str_to_c_string(&result)
}
