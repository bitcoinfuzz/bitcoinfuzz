use std::ffi::CString;
use std::os::raw::c_char;
use std::slice;

use psbt_v2::bitcoin::Psbt;

unsafe fn str_to_c_string(input: &str) -> *mut c_char {
    CString::new(input).unwrap().into_raw()
}

/// Frees a C string created by `str_to_c_string`.
///
/// # Safety
/// The pointer must have been created by `str_to_c_string` and not yet freed.
/// After calling this function, the pointer is invalid and must not be used.
#[no_mangle]
pub unsafe extern "C" fn rust_psbt_free_c_string(ptr: *mut c_char) {
    if !ptr.is_null() {
        // Convert the raw pointer back to a CString, which will be dropped
        // and free the memory when it goes out of scope
        let _ = CString::from_raw(ptr);
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_psbt_psbt_parse(data: *const u8, len: usize) -> *mut c_char {
    let data_slice = slice::from_raw_parts(data, len);

    match Psbt::deserialize(data_slice) {
        Ok(psbt) => {
            let mut result = String::new();

            //result.push_str(&format!("v={};", psbt.unsigned_tx.version));
            result.push_str(&format!("lt={};", psbt.unsigned_tx.lock_time));
            result.push_str(&format!("in={};", psbt.inputs.len()));
            result.push_str(&format!("out={};", psbt.outputs.len()));
            for (i, input) in psbt.unsigned_tx.input.iter().enumerate() {
                if i < psbt.inputs.len() {
                    result.push_str(&format!(
                        "in{}prev={}:{};",
                        i, input.previous_output.txid, input.previous_output.vout
                    ));
                    result.push_str(&format!("in{}seq={};", i, input.sequence));

                    let psbt_input = &psbt.inputs[i];

                    if psbt_input.witness_utxo.is_some() || psbt_input.non_witness_utxo.is_some() {
                        result.push_str(&format!("in{}utxo=1;", i));
                    }

                    result.push_str(&format!("in{}sigs={};", i, psbt_input.partial_sigs.len()));
                }
            }

            for (i, output) in psbt.unsigned_tx.output.iter().enumerate() {
                if i < psbt.outputs.len() {
                    // refer: https://github.com/bitcoinfuzz/bitcoinfuzz/issues/134#issuecomment-2884936854 for typecasting
                    result.push_str(&format!("out{}val={};", i, output.value.to_sat() as i64));
                    result.push_str(&format!(
                        "out{}script={};",
                        i,
                        output.script_pubkey.to_hex_string()
                    ));
                }
            }

            str_to_c_string(&result)
        }
        Err(_) => std::ptr::null_mut(),
    }
}
