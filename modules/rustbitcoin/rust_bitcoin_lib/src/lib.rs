use std::ffi::CStr;
use std::ffi::CString;
use std::os::raw::c_char;
use std::slice;
use std::str::{Utf8Error, FromStr};

use bitcoin::address::Address;
use bitcoin::consensus::{deserialize_partial, encode};
use bitcoin::Block;

unsafe fn str_to_c_string(input: &str) -> *mut c_char {
    CString::new(input).unwrap().into_raw()
}

/// Frees a C string created by `str_to_c_string`.
///
/// # Safety
/// The pointer must have been created by `str_to_c_string` and not yet freed.
/// After calling this function, the pointer is invalid and must not be used.
#[no_mangle]
pub unsafe extern "C" fn free_c_string(ptr: *mut c_char) {
    if !ptr.is_null() {
        // Convert the raw pointer back to a CString, which will be dropped
        // and free the memory when it goes out of scope
        let _ = CString::from_raw(ptr);
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_bitcoin_des_block(
    data: *const u8,
    len: usize,
    _out_len: *mut usize,
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
pub unsafe extern "C" fn rust_bitcoin_script(data: *const u8, len: usize) -> *mut c_char {
    // Safety: Ensure that the data pointer is valid for the given length
    let data_slice = slice::from_raw_parts(data, len);

    let script: Result<(bitcoin::script::ScriptBuf, usize), encode::Error> =
        encode::deserialize_partial(data_slice);
    match script {
        Err(_) => str_to_c_string("0"),
        Ok(s) => {
            if s.0.is_op_return() || s.0.len() > 10_000 {
                return str_to_c_string("0");
            }
            let mut final_res = s.0.count_sigops_legacy().to_string();
            final_res.push_str(if s.0.is_witness_program() { "1" } else { "0" });
            final_res.push_str(if s.0.is_push_only() { "1" } else { "0" });
            str_to_c_string(&final_res)
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_bitcoin_address_parse(address: *const c_char) -> *mut c_char {
    if address.is_null() {
        return std::ptr::null_mut();
    }

    let address_str = match c_str_to_str(address) {
        Ok(s) => s,
        Err(_) => return str_to_c_string("INVALID"),
    };

    match Address::from_str(address_str) {
        Ok(addr_unchecked) => {
            match addr_unchecked.require_network(bitcoin::Network::Bitcoin) {
                Ok(addr) => {
                    let prefix = match addr.address_type() {
                        Some(bitcoin::address::AddressType::P2pkh) => "PKH:",
                        Some(bitcoin::address::AddressType::P2sh) => "SH:",
                        Some(bitcoin::address::AddressType::P2wpkh) => "WPKH:",
                        Some(bitcoin::address::AddressType::P2wsh) => "WSH:",
                        Some(bitcoin::address::AddressType::P2tr) => "TR:",
                        Some(_) => "UNK:",
                        None => "UNK:",
                    };

                    let result = format!("{}{:}", prefix, addr);
                    str_to_c_string(&result)
                },
                Err(_) => str_to_c_string("INVALID"),
            }
        },
        Err(_) => str_to_c_string("INVALID"),
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_bitcoin_psbt_parse(data: *const u8, len: usize) -> *mut c_char {
    let data_slice = slice::from_raw_parts(data, len);

    match bitcoin::psbt::Psbt::deserialize(data_slice) {
        Ok(psbt) => {
            let mut result = String::new();

            result.push_str(&format!("v={};", psbt.unsigned_tx.version));
            result.push_str(&format!("lt={};", psbt.unsigned_tx.lock_time));
            result.push_str(&format!("in={};", psbt.inputs.len()));
            result.push_str(&format!("out={};", psbt.outputs.len()));

            for (i, input) in psbt.unsigned_tx.input.iter().enumerate() {
                if i < psbt.inputs.len() {
                    result.push_str(&format!("in{}prev={}:{};", 
                        i, input.previous_output.txid, input.previous_output.vout));
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
                    result.push_str(&format!("out{}val={};", i, output.value.to_sat()));
                    result.push_str(&format!("out{}script={};", i, output.script_pubkey.to_hex_string()));
                }
            }

            str_to_c_string(&result)
        },
        Err(_) => std::ptr::null_mut(),
    }
}

unsafe fn c_str_to_str<'a>(input: *const c_char) -> Result<&'a str, Utf8Error> {
    CStr::from_ptr(input).to_str()
}
