use bitcoin::consensus::encode;
use bitcoin::p2p::address::{AddrV2, AddrV2Message};
use floresta_wire::address_man::{AddressMan, LocalAddress};
use std::ffi::CString;
use std::os::raw::c_char;
use std::slice;

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

/// Parse addrv2 messages using Floresta's AddressMan validation logic.
///
/// This uses Floresta's actual address management infrastructure to validate
/// incoming addrv2 messages, applying the same filters used by Floresta nodes:
/// - Filters out private addresses (RFC1918 for IPv4, fd/fe prefix for IPv6)
/// - Filters out localhost addresses
/// - Requires WITNESS and NETWORK service flags
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
            // Create a Floresta AddressMan to use its validation logic
            let mut address_man = AddressMan::default();

            // Convert AddrV2Messages to LocalAddresses using Floresta's From impl
            // This preserves the time, services, port from the original message
            let local_addresses: Vec<LocalAddress> = addresses
                .into_iter()
                .map(LocalAddress::from)
                .collect();

            // Use AddressMan's push_addresses to apply Floresta's validation
            // This internally filters out:
            // - Private addresses (is_private check)
            // - Localhost addresses (is_localhost check)
            // - Addresses without WITNESS and NETWORK services
            address_man.push_addresses(&local_addresses);

            // Get all valid addresses that passed Floresta's validation
            // get_addresses_to_send returns good addresses as (AddrV2, timestamp, services, port)
            for (addr, _, _, _) in address_man.get_addresses_to_send() {
                match addr {
                    AddrV2::Ipv4(_) | AddrV2::Ipv6(_) => clearnet += 1,
                    AddrV2::TorV3(_) => tor += 1,
                    AddrV2::Cjdns(_) => cjdns += 1,
                    AddrV2::I2p(_) => i2p += 1,
                    _ => {}
                }
            }

            let res = format!("clearnet={}tor={}cjdns={}i2p={}", clearnet, tor, cjdns, i2p);
            str_to_c_string(&res)
        }
    }
}
