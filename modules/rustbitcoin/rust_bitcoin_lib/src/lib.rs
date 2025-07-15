use bitcoin::address::Address;
use bitcoin::bip152::HeaderAndShortIds;
use bitcoin::block::BlockUncheckedExt;
use bitcoin::consensus::{deserialize_partial, encode, serialize};
use bitcoin::script::{ScriptBuf, ScriptExt};
use bitcoin::Block;
use p2p::address::AddrV2;
use p2p::message::AddrV2Payload;
use std::ffi::CStr;
use std::ffi::CString;
use std::os::raw::c_char;
use std::slice;
use std::str::{FromStr, Utf8Error};

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
            let block_hash = res.0.block_hash();
            let is_valid = res.0.validate().is_ok();
            if is_valid {
                return str_to_c_string(&block_hash.to_string());
            } else {
                return str_to_c_string("0");
            }
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

    let script: Result<(ScriptBuf, usize), encode::ParseError> =
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
        Ok(addr_unchecked) => match addr_unchecked.require_network(bitcoin::Network::Bitcoin) {
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
            }
            Err(_) => str_to_c_string("INVALID"),
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

#[no_mangle]
pub unsafe extern "C" fn rust_bitcoin_addrv2(data: *const u8, len: usize) -> *mut c_char {
    // Safety: Ensure that the data pointer is valid for the given length
    let data_slice = slice::from_raw_parts(data, len);

    let addr: Result<(AddrV2Payload, usize), _> = encode::deserialize_partial(data_slice);
    let mut clearnet: u64 = 0;
    let mut tor: u64 = 0;
    let mut i2p: u64 = 0;
    let mut cjdns: u64 = 0;
    match addr {
        Err(_) => str_to_c_string("clearnet=0tor=0cjdns=0i2p=0"),
        Ok(vec_addr) => {
            for a in vec_addr.0 .0 {
                if matches!(a.addr, AddrV2::Ipv4(_)) {
                    clearnet += 1;
                } else if matches!(a.addr, AddrV2::Ipv6(_)) {
                    clearnet += 1;
                } else if matches!(a.addr, AddrV2::TorV3(_)) {
                    tor += 1;
                } else if matches!(a.addr, AddrV2::Cjdns(_)) {
                    cjdns += 1;
                } else if matches!(a.addr, AddrV2::I2p(_)) {
                    i2p += 1;
                }
            }
            let res = "clearnet=".to_string()
                + &clearnet.to_string()
                + "tor="
                + &tor.to_string()
                + "cjdns="
                + &cjdns.to_string()
                + "i2p="
                + &i2p.to_string();
            return str_to_c_string(&res);
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn rust_bitcoin_cmpctblocks_parse(data: *const u8, len: usize) -> i32 {
    // Safety: Ensure that the data pointer is valid for the given length
    let data_slice = slice::from_raw_parts(data, len);

    let res = deserialize_partial::<HeaderAndShortIds>(data_slice);

    match res {
        Ok((header_and_short_ids, _)) => {
            let serialized_data = serialize(&header_and_short_ids);
            serialized_data.len() as i32
        }
        Err(err) => {
            if err.to_string().starts_with("unsupported segwit version") {
                return -2;
            }
            return -1;
        }
    }
}

unsafe fn c_str_to_str<'a>(input: *const c_char) -> Result<&'a str, Utf8Error> {
    CStr::from_ptr(input).to_str()
}
