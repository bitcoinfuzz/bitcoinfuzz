use bitcoin::absolute::Decodable;
use bitcoin::address::Address;
use bitcoin::bip152::HeaderAndShortIds;
use bitcoin::bip32::Xpriv;
use bitcoin::block::BlockUncheckedExt;
use bitcoin::consensus::{deserialize_partial, encode, serialize};
use bitcoin::script::ScriptExt;
use bitcoin::script::ScriptPubKeyBuf;
use bitcoin::script::ScriptPubKeyExt;
use bitcoin::Block;
use bitcoin::NetworkKind;
use p2p::address::AddrV2;
use p2p::message::{AddrV2Payload, RawNetworkMessage};
use p2p::Magic;
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
            let block = match res.0.validate() {
                Ok(block_checked) => block_checked,
                Err(_) => return str_to_c_string("0"),
            };
            let block_hash = block.block_hash();
            return str_to_c_string(&block_hash.to_string());
        }
        Err(err) => {
            if err.to_string().starts_with("unsupported SegWit version") {
                return str_to_c_string("skip error");
            }
            if err
                .to_string()
                .starts_with("parse failed: amount is greater than Amount::MAX_MONEY")
            {
                return str_to_c_string("skip error");
            }
            return str_to_c_string("0");
        }
    };
}

#[no_mangle]
pub unsafe extern "C" fn rust_bitcoin_script(data: *const u8, len: usize) -> *mut c_char {
    // Safety: Ensure that the data pointer is valid for the given length
    let data_slice = slice::from_raw_parts(data, len);

    let script: Result<(ScriptPubKeyBuf, usize), encode::ParseError> =
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
pub unsafe extern "C" fn rust_bitcoin_parse_p2p_message(
    data: *const u8,
    len: usize,
) -> *mut c_char {
    let mut data_slice = slice::from_raw_parts(data, len);

    let message = match RawNetworkMessage::consensus_decode(&mut data_slice) {
        Ok(m) => m,
        Err(encode::Error::Parse(encode::ParseError::UnsupportedSegwitFlag(_))) => {
            return std::ptr::null_mut()
        }
        Err(_) => return str_to_c_string("0"),
    };
    if message.magic() != &Magic::BITCOIN {
        return str_to_c_string("0");
    }
    let cmd = message.cmd();
    match cmd {
        "unknown" => str_to_c_string("0"),
        _ => str_to_c_string(cmd),
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
            for (i, input) in psbt.unsigned_tx.inputs.iter().enumerate() {
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

            for (i, output) in psbt.unsigned_tx.outputs.iter().enumerate() {
                if i < psbt.outputs.len() {
                    // refer: https://github.com/bitcoinfuzz/bitcoinfuzz/issues/134#issuecomment-2884936854 for typecasting
                    result.push_str(&format!("out{}val={};", i, output.amount.to_sat() as i64));
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

/// Parses addrv2 message data and returns a JSON-like array string.
///
/// # Output Format
/// Returns a JSON-like array of address objects. Note: the output format is structured
/// for parseability but is not guaranteed to be strictly valid JSON. Since address values
/// are hex-encoded, no special character escaping is needed.
///
/// Example output:
/// ```text
/// [{"addr":"0x7f000001","time":1700000000,"services":"0x1","port":8333}]
/// ```
///
/// On parse error, returns an empty array: `[]`
///
/// # Safety
/// The caller must ensure that `data` points to a valid memory region of at least `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn rust_bitcoin_addrv2(data: *const u8, len: usize) -> *mut c_char {
    let data_slice = slice::from_raw_parts(data, len);

    let addr: Result<(AddrV2Payload, usize), _> = encode::deserialize_partial(data_slice);
    match addr {
        Err(_) => str_to_c_string("[]"),
        Ok(vec_addr) => {
            let entries: Vec<String> = vec_addr.0 .0.iter().map(format_addrv2_message).collect();
            let res = format!("[{}]", entries.join(","));
            str_to_c_string(&res)
        }
    }
}

/// Formats an AddrV2Message into a structured string representation.
fn format_addrv2_message(msg: &p2p::address::AddrV2Message) -> String {
    let addr_hex = addr_to_hex(&msg.addr);
    format!(
        "{{\"addr\":\"{}\",\"time\":{},\"services\":\"{:#x}\",\"port\":{}}}",
        addr_hex,
        msg.time,
        msg.services.to_u64(),
        msg.port
    )
}

/// Converts an AddrV2 address to its hexadecimal representation.
fn addr_to_hex(addr: &AddrV2) -> String {
    match addr {
        AddrV2::Ipv4(ip) => format!("0x{}", hex::encode(ip.octets())),
        AddrV2::Ipv6(ip) => format!("0x{}", hex::encode(ip.octets())),
        AddrV2::TorV3(bytes) => format!("0x{}", hex::encode(bytes)),
        AddrV2::I2p(bytes) => format!("0x{}", hex::encode(bytes)),
        AddrV2::Cjdns(ip) => format!("0x{}", hex::encode(ip.octets())),
        AddrV2::Unknown(net_id, bytes) => format!("0x{:02x}{}", net_id, hex::encode(bytes)),
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

#[no_mangle]
pub unsafe extern "C" fn rust_bitcoin_bip32_master_keygen(
    data: *const u8,
    len: usize,
) -> *mut c_char {
    let seed = slice::from_raw_parts(data, len);
    let sk = Xpriv::new_master(NetworkKind::Main, &seed);
    str_to_c_string(&sk.to_string())
}

unsafe fn c_str_to_str<'a>(input: *const c_char) -> Result<&'a str, Utf8Error> {
    CStr::from_ptr(input).to_str()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::net::{Ipv4Addr, Ipv6Addr};

    #[test]
    fn test_addr_to_hex_ipv4() {
        let addr = AddrV2::Ipv4(Ipv4Addr::new(127, 0, 0, 1));
        let hex = addr_to_hex(&addr);
        assert_eq!(hex, "0x7f000001");
    }

    #[test]
    fn test_addr_to_hex_ipv6() {
        let addr = AddrV2::Ipv6(Ipv6Addr::new(0x2001, 0x0db8, 0, 0, 0, 0, 0, 1));
        let hex = addr_to_hex(&addr);
        assert_eq!(hex, "0x20010db8000000000000000000000001");
    }

    #[test]
    fn test_addr_to_hex_torv3() {
        let bytes: [u8; 32] = [0x01; 32];
        let addr = AddrV2::TorV3(bytes);
        let hex = addr_to_hex(&addr);
        assert!(hex.starts_with("0x"));
        assert_eq!(hex.len(), 66); // "0x" + 64 hex chars
    }

    #[test]
    fn test_addr_to_hex_i2p() {
        let bytes: [u8; 32] = [0xab; 32];
        let addr = AddrV2::I2p(bytes);
        let hex = addr_to_hex(&addr);
        assert!(hex.starts_with("0x"));
        assert_eq!(hex.len(), 66);
    }

    #[test]
    fn test_addr_to_hex_cjdns() {
        let addr = AddrV2::Cjdns(Ipv6Addr::new(0xfc00, 0, 0, 0, 0, 0, 0, 1));
        let hex = addr_to_hex(&addr);
        assert_eq!(hex, "0xfc000000000000000000000000000001");
    }

    #[test]
    fn test_addr_to_hex_unknown() {
        let addr = AddrV2::Unknown(0x99, vec![0xde, 0xad, 0xbe, 0xef]);
        let hex = addr_to_hex(&addr);
        assert_eq!(hex, "0x99deadbeef");
    }

    #[test]
    fn test_format_addrv2_message() {
        use p2p::address::AddrV2Message;
        use p2p::ServiceFlags;

        let msg = AddrV2Message {
            time: 1700000000,
            services: ServiceFlags::NETWORK,
            addr: AddrV2::Ipv4(Ipv4Addr::new(192, 168, 1, 1)),
            port: 8333,
        };

        let formatted = format_addrv2_message(&msg);
        assert!(formatted.contains("\"addr\":\"0xc0a80101\""));
        assert!(formatted.contains("\"time\":1700000000"));
        assert!(formatted.contains("\"services\":\"0x1\""));
        assert!(formatted.contains("\"port\":8333"));
    }

    #[test]
    fn test_rust_bitcoin_addrv2_invalid_data() {
        // Test that invalid data returns empty array
        let invalid_data: [u8; 4] = [0xff, 0xff, 0xff, 0xff];
        unsafe {
            let result = rust_bitcoin_addrv2(invalid_data.as_ptr(), invalid_data.len());
            let result_str = CStr::from_ptr(result).to_str().unwrap();
            assert_eq!(result_str, "[]");
            free_c_string(result);
        }
    }

    #[test]
    fn test_rust_bitcoin_addrv2_empty_data() {
        // Test that empty payload returns empty array
        let empty_data: [u8; 1] = [0x00]; // varint 0 = empty list
        unsafe {
            let result = rust_bitcoin_addrv2(empty_data.as_ptr(), empty_data.len());
            let result_str = CStr::from_ptr(result).to_str().unwrap();
            assert_eq!(result_str, "[]");
            free_c_string(result);
        }
    }
}
