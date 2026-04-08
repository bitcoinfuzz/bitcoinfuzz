use musig2::KeyAggContext;
use secp256k1::PublicKey;
use std::ffi::CString;
use std::os::raw::c_char;
use std::{ptr, slice};

/// Aggregates multiple public keys into a single aggregated public key.
///
/// # Arguments
/// * `pubkeys` - Pointer to concatenated 33-byte compressed public keys
/// * `num_keys` - Number of public keys to aggregate
///
/// # Returns
/// * Hex-encoded 33-byte compressed aggregated public key, or null on error
///
/// # Safety
/// Caller must ensure `pubkeys` points to valid memory of size `num_keys * 33`
#[no_mangle]
pub unsafe extern "C" fn musig2_key_agg(pubkeys: *const u8, num_keys: usize) -> *mut c_char {
    if pubkeys.is_null() || num_keys == 0 || num_keys > 100 {
        return ptr::null_mut();
    }

    let pubkeys_slice = slice::from_raw_parts(pubkeys, num_keys * 33);

    // Parse each 33-byte compressed public key
    let mut parsed_pubkeys: Vec<PublicKey> = Vec::with_capacity(num_keys);
    for i in 0..num_keys {
        let start = i * 33;
        let end = start + 33;
        let pubkey_bytes = &pubkeys_slice[start..end];

        let pubkey = match PublicKey::from_slice(pubkey_bytes) {
            Ok(pk) => pk,
            Err(_) => return ptr::null_mut(),
        };
        parsed_pubkeys.push(pubkey);
    }

    // Aggregate the public keys using musig2
    let key_agg_ctx = match KeyAggContext::new(parsed_pubkeys) {
        Ok(ctx) => ctx,
        Err(_) => return ptr::null_mut(),
    };

    // Get the aggregated public key (includes parity)
    let aggregated_pubkey: PublicKey = key_agg_ctx.aggregated_pubkey();

    // Serialize to compressed format (33 bytes with 02/03 prefix)
    let serialized = aggregated_pubkey.serialize();
    let hex_result = hex::encode(serialized);

    str_to_c_string(&hex_result)
}

/// Frees a string allocated by this library.
///
/// # Safety
/// Caller must ensure `ptr` was allocated by this library's functions.
#[no_mangle]
pub unsafe extern "C" fn musig2_free_string(ptr: *mut c_char) {
    if !ptr.is_null() {
        let _ = CString::from_raw(ptr);
    }
}

unsafe fn str_to_c_string(input: &str) -> *mut c_char {
    match CString::new(input) {
        Ok(s) => s.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_key_agg_two_keys() {
        // Test vectors from BIP-327
        let pubkey1 =
            hex::decode("02F9308A019258C31049344F85F89D5229B531C845836F99B08601F113BCE036F9")
                .unwrap();
        let pubkey2 =
            hex::decode("03DFF1D77F2A671C5F36183726DB2341BE58FEAE1DA2DECED843240F7B502BA659")
                .unwrap();

        let mut pubkeys = Vec::new();
        pubkeys.extend_from_slice(&pubkey1);
        pubkeys.extend_from_slice(&pubkey2);

        unsafe {
            let result = musig2_key_agg(pubkeys.as_ptr(), 2);
            assert!(!result.is_null());

            let result_str = CString::from_raw(result);
            let hex_str = result_str.to_str().unwrap();

            // Result should be a 66-character hex string (33 bytes compressed pubkey)
            assert_eq!(hex_str.len(), 66);
            // Should start with 02 or 03 (compressed pubkey prefix)
            assert!(hex_str.starts_with("02") || hex_str.starts_with("03"));
        }
    }

    #[test]
    fn test_key_agg_invalid_pubkey() {
        // Invalid pubkey (all zeros)
        let invalid_pubkey = vec![0u8; 33];

        unsafe {
            let result = musig2_key_agg(invalid_pubkey.as_ptr(), 1);
            assert!(result.is_null());
        }
    }

    #[test]
    fn test_key_agg_null_input() {
        unsafe {
            let result = musig2_key_agg(ptr::null(), 1);
            assert!(result.is_null());
        }
    }

    #[test]
    fn test_key_agg_zero_keys() {
        let pubkey =
            hex::decode("02F9308A019258C31049344F85F89D5229B531C845836F99B08601F113BCE036F9")
                .unwrap();

        unsafe {
            let result = musig2_key_agg(pubkey.as_ptr(), 0);
            assert!(result.is_null());
        }
    }
}
