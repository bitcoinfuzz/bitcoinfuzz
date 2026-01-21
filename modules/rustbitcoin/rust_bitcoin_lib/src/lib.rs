use bitcoin::absolute::Decodable;
use bitcoin::address::Address;
use bitcoin::bip152::HeaderAndShortIds;
use bitcoin::bip32::ChainCode;
use bitcoin::bip32::ChildNumber;
use bitcoin::bip32::Fingerprint;
use bitcoin::bip32::Xpriv;
use bitcoin::bip32::Xpub;
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
use std::net::Ipv4Addr;
use std::net::Ipv6Addr;
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

/// Converts AddrV2 to hex string representation of its raw bytes
fn addrv2_to_hex(addr: &AddrV2) -> String {
    match addr {
        AddrV2::Ipv4(ip) => ip.octets().iter().map(|b| format!("{:02x}", b)).collect(),
        AddrV2::Ipv6(ip) => ip.octets().iter().map(|b| format!("{:02x}", b)).collect(),
        AddrV2::TorV3(bytes) => bytes.iter().map(|b| format!("{:02x}", b)).collect(),
        AddrV2::I2p(bytes) => bytes.iter().map(|b| format!("{:02x}", b)).collect(),
        AddrV2::Cjdns(ip) => ip.octets().iter().map(|b| format!("{:02x}", b)).collect(),
        AddrV2::Unknown(network_id, bytes) => {
            format!(
                "{:02x}:{}",
                network_id,
                bytes
                    .iter()
                    .map(|b| format!("{:02x}", b))
                    .collect::<String>()
            )
        }
    }
}

/// Returns the type string for an AddrV2 address
fn addrv2_type(addr: &AddrV2) -> &'static str {
    match addr {
        AddrV2::Ipv4(_) => "ipv4",
        AddrV2::Ipv6(_) => "ipv6",
        AddrV2::TorV3(_) => "tor",
        AddrV2::I2p(_) => "i2p",
        AddrV2::Cjdns(_) => "cjdns",
        AddrV2::Unknown(_, _) => "unknown",
    }
}

fn is_routable_ipv4(ip: &Ipv4Addr) -> bool {
    let octets = ip.octets();

    // 0.0.0.0/8 - "This" network
    if octets[0] == 0 {
        return false;
    }

    // Loopback, broadcast, private (RFC 1918)
    if ip.is_loopback() || ip.is_broadcast() || ip.is_private() {
        return false;
    }

    // RFC 2544 - Benchmarking - 198.18.0.0/15
    if octets[0] == 198 && (octets[1] == 18 || octets[1] == 19) {
        return false;
    }

    // RFC 3927 - Link-Local - 169.254.0.0/16
    if ip.is_link_local() {
        return false;
    }

    // RFC 6598 - Shared Address Space (CGNAT) - 100.64.0.0/10
    if octets[0] == 100 && (octets[1] >= 64 && octets[1] <= 127) {
        return false;
    }

    // RFC 5737 - Documentation (TEST-NET-1, TEST-NET-2, TEST-NET-3)
    if ip.is_documentation() {
        return false;
    }

    true
}

fn is_routable_ipv6(ip: &Ipv6Addr) -> bool {
    let octets = ip.octets();

    // Unspecified, loopback, unique local (RFC 4193 - fc00::/7)
    if ip.is_unspecified() || ip.is_loopback() || ip.is_unique_local() {
        return false;
    }

    // RFC 4843 - ORCHID - 2001:10::/28
    if octets[0] == 0x20 && octets[1] == 0x01 && octets[2] == 0x00 && (octets[3] & 0xF0) == 0x10 {
        return false;
    }

    // RFC 4862 - Link-local - fe80::/64
    if octets[..8] == [0xFE, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00] {
        return false;
    }

    // RFC 7343 - ORCHIDv2 - 2001:20::/28
    if octets[0] == 0x20 && octets[1] == 0x01 && octets[2] == 0x00 && (octets[3] & 0xf0) == 0x20 {
        return false;
    }

    true
}

#[no_mangle]
pub unsafe extern "C" fn rust_bitcoin_addrv2(data: *const u8, len: usize) -> *mut c_char {
    // Safety: Ensure that the data pointer is valid for the given length
    let data_slice = slice::from_raw_parts(data, len);

    let addr: Result<(AddrV2Payload, usize), _> = encode::deserialize_partial(data_slice);
    match addr {
        Err(e) => {
            let msg = e.to_string();
            if msg.contains("invalid CJDNS address") || msg.contains("IPV4 wrapped address") {
                return std::ptr::null_mut();
            }
            str_to_c_string("[]")
        }
        Ok(vec_addr) => {
            let mut entries: Vec<String> = Vec::new();
            for a in vec_addr.0 .0 {
                if matches!(a.addr, AddrV2::Unknown(..)) {
                    continue;
                }
                // Match the same behavior as isRoutable() function from core.
                if let AddrV2::Ipv4(ip) = &a.addr {
                    if !is_routable_ipv4(ip) {
                        continue;
                    }
                }
                if let AddrV2::Ipv6(ip) = &a.addr {
                    if !is_routable_ipv6(ip) {
                        continue;
                    }
                }
                let addr_hex = addrv2_to_hex(&a.addr);
                let addr_type = addrv2_type(&a.addr);
                let time = a.time;
                let services = a.services.to_u64();
                let port = a.port;
                entries.push(format!(
                    "{{\"addr\":\"{}\",\"type\":\"{}\",\"time\":\"{}\",\"services\":\"{}\",\"port\":\"{}\"}}",
                    addr_hex, addr_type, time, services, port
                ));
            }
            let result = format!("[{}]", entries.join(","));
            str_to_c_string(&result)
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

//helper func
fn format_ext_key_common(
    depth: u8,
    fingerprint: Fingerprint,
    child_number: ChildNumber,
    chain_code: ChainCode,
    key_bytes: &[u8],
) -> String {
    let hex_key_bytes: String = key_bytes.iter().map(|b| format!("{:02x}", b)).collect();
    let child_u32: u32 = child_number.into();
    format!(
        "depth={:02x};fp={:02x}{:02x}{:02x}{:02x};child={:08x};chaincode={};key={}",
        depth,
        fingerprint[0],
        fingerprint[1],
        fingerprint[2],
        fingerprint[3],
        child_u32,
        chain_code,
        hex_key_bytes
    )
}

#[no_mangle]
pub unsafe extern "C" fn rust_bitcoin_bip32_deserialize_extended_key(
    data: *const u8,
    len: usize,
) -> *mut c_char {
    let data_slice = slice::from_raw_parts(data, len);
    let ext_str = match std::str::from_utf8(data_slice) {
        Ok(s) => s,
        Err(_) => return str_to_c_string("INVALID"),
    };
    if let Ok(xprv) = Xpriv::from_str(&ext_str) {
        //format the result to string
        let result = format_ext_key_common(
            xprv.depth,
            xprv.parent_fingerprint,
            xprv.child_number,
            xprv.chain_code,
            &xprv.private_key.to_secret_bytes(), // as bytes
        );
        //return formatted string
        str_to_c_string(&result)
    } else {
        if let Ok(xpub) = Xpub::from_str(&ext_str) {
            //format the result to string
            let result = format_ext_key_common(
                xpub.depth,
                xpub.parent_fingerprint,
                xpub.child_number,
                xpub.chain_code,
                &xpub.public_key.serialize(), // as bytes
            );
            //return formatted string
            str_to_c_string(&result)
        } else {
            str_to_c_string("INVALID")
        }
    }
}
