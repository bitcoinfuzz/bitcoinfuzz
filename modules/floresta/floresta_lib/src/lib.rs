use bitcoin::consensus::encode;
use p2p::address::{AddrV2, AddrV2Message};
use p2p::ServiceFlags;
use std::ffi::CString;
use std::os::raw::c_char;
use std::slice;

/// Max addresses in a single addrv2 message (from Floresta PR #781)
const MAX_ADDR_TO_SEND: usize = 1000;

unsafe fn str_to_c_string(input: &str) -> *mut c_char {
    CString::new(input).unwrap().into_raw()
}

/// Frees a C string created by `str_to_c_string`.
///
/// # Safety
/// The pointer must have been created by `str_to_c_string` and not yet freed.
/// After calling this function, the pointer is invalid and must not be used.
#[no_mangle]
pub unsafe extern "C" fn floresta_free_c_string(ptr: *mut c_char) {
    if !ptr.is_null() {
        let _ = CString::from_raw(ptr);
    }
}

/// Check if an address is private (RFC1918 for IPv4, fc/fd prefix for IPv6 ULA)
/// Also includes 0.x.x.x range (from Floresta PR #781)
fn is_private(addr: &AddrV2) -> bool {
    match addr {
        AddrV2::Ipv4(ip) => ip.is_private() || ip.octets()[0] == 0,
        // RFC 4193: Unique Local Addresses use fc00::/7 (0xfc and 0xfd)
        AddrV2::Ipv6(ip) => {
            let first = ip.octets()[0];
            first == 0xfc || first == 0xfd
        }
        _ => false,
    }
}

/// Check if an address is localhost
fn is_localhost(addr: &AddrV2) -> bool {
    match addr {
        AddrV2::Ipv4(ip) => ip.is_loopback(),
        AddrV2::Ipv6(ip) => ip.is_loopback(),
        _ => false,
    }
}

/// Check if the address has minimum required services (WITNESS and NETWORK)
fn has_required_services(services: ServiceFlags) -> bool {
    services.has(ServiceFlags::WITNESS) && services.has(ServiceFlags::NETWORK)
}

/// Validate an addrv2 entry using Floresta's full-node validation logic
fn is_valid_address(msg: &AddrV2Message) -> bool {
    if is_private(&msg.addr) {
        return false;
    }
    if is_localhost(&msg.addr) {
        return false;
    }
    if !has_required_services(msg.services) {
        return false;
    }
    true
}

/// Parse addrv2 messages with Floresta's full-node validation.
///
/// # Safety
/// The data pointer must be valid for the given length.
#[no_mangle]
pub unsafe extern "C" fn floresta_addrv2(data: *const u8, len: usize) -> *mut c_char {
    let data_slice = slice::from_raw_parts(data, len);

    let addr_vec: Result<(Vec<AddrV2Message>, usize), _> = encode::deserialize_partial(data_slice);

    let mut clearnet: u64 = 0;
    let mut tor: u64 = 0;
    let mut i2p: u64 = 0;
    let mut cjdns: u64 = 0;

    match addr_vec {
        Err(_) => str_to_c_string("clearnet=0tor=0cjdns=0i2p=0"),
        Ok((addresses, _)) => {
            // Reject messages with too many addresses (DoS protection)
            if addresses.len() > MAX_ADDR_TO_SEND {
                return str_to_c_string("clearnet=0tor=0cjdns=0i2p=0");
            }

            for msg in addresses {
                if !is_valid_address(&msg) {
                    continue;
                }

                match msg.addr {
                    AddrV2::Ipv4(_) | AddrV2::Ipv6(_) => clearnet += 1,
                    AddrV2::TorV3(_) => tor += 1,
                    AddrV2::Cjdns(_) => cjdns += 1,
                    AddrV2::I2p(_) => i2p += 1,
                    AddrV2::Unknown(_, _) => {}
                }
            }

            let res = format!("clearnet={}tor={}cjdns={}i2p={}", clearnet, tor, cjdns, i2p);
            str_to_c_string(&res)
        }
    }
}
