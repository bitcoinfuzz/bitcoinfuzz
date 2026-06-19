use musig2::KeyAggContext;
use secp256k1::{PublicKey, Secp256k1, SecretKey};
use std::ffi::CString;
use std::os::raw::c_char;
use std::{ptr, slice};

/// Derives a public key per scalar and aggregates them into one MuSig2 key.
///
/// # Arguments
/// * `seckeys` - Pointer to concatenated 32-byte private keys
/// * `num_keys` - Number of private keys to aggregate
///
/// # Returns
/// * Hex-encoded 33-byte compressed aggregated public key on success;
/// * the "AGG_FAIL" sentinel if aggregation itself is rejected (so the C++
///   driver compares it against the peer module and catches accept-vs-reject
///   disagreements);
/// * null if an input scalar is invalid (symmetric, skipped by the driver).
///
/// # Safety
/// Caller must ensure `seckeys` points to valid memory of size `num_keys * 32`
#[no_mangle]
pub unsafe extern "C" fn musig2_key_agg(seckeys: *const u8, num_keys: usize) -> *mut c_char {
    if seckeys.is_null() || num_keys == 0 || num_keys > 100 {
        return ptr::null_mut();
    }

    let seckeys_slice = slice::from_raw_parts(seckeys, num_keys * 32);
    let secp = Secp256k1::new();

    // Derive a pubkey for each 32-byte scalar, preserving input order: BIP-327
    // key aggregation is order-sensitive (sorting is a separate, optional step).
    let mut parsed_pubkeys: Vec<PublicKey> = Vec::with_capacity(num_keys);
    for i in 0..num_keys {
        let start = i * 32;
        let end = start + 32;
        let seckey_bytes: [u8; 32] = match seckeys_slice[start..end].try_into() {
            Ok(arr) => arr,
            Err(_) => return ptr::null_mut(),
        };
        let seckey = match SecretKey::from_byte_array(seckey_bytes) {
            Ok(sk) => sk,
            Err(_) => return ptr::null_mut(),
        };
        parsed_pubkeys.push(PublicKey::from_secret_key(&secp, &seckey));
    }

    // Aggregate the public keys using musig2
    let key_agg_ctx = match KeyAggContext::new(parsed_pubkeys) {
        Ok(ctx) => ctx,
        Err(_) => return str_to_c_string("AGG_FAIL"),
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
        // Two distinct, valid scalars; pubkeys are derived internally.
        let mut seckeys = Vec::new();
        seckeys.extend_from_slice(&[1u8; 32]);
        seckeys.extend_from_slice(&[2u8; 32]);

        unsafe {
            let result = musig2_key_agg(seckeys.as_ptr(), 2);
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
    fn test_key_agg_invalid_scalar() {
        // All-zero scalar is not a valid secret key -> null (skipped by driver).
        let invalid_seckey = vec![0u8; 32];

        unsafe {
            let result = musig2_key_agg(invalid_seckey.as_ptr(), 1);
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
        let seckey = vec![1u8; 32];

        unsafe {
            let result = musig2_key_agg(seckey.as_ptr(), 0);
            assert!(result.is_null());
        }
    }
}
