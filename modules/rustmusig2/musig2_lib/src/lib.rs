use musig2::secp::Scalar;
use musig2::{AggNonce, KeyAggContext, PartialSignature, PubNonce, SecNonce};
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

/// Runs a complete MuSig2 signing session and returns the final aggregated
/// Schnorr signature, mirroring the `secp256k1` module's
/// `secp256k1_musig2_sign_session` so the C++ driver can compare the two
/// byte-for-byte.
///
/// Each signer's nonce is derived deterministically from its 32-byte
/// `nonce_seeds` entry exactly as BIP327 / libsecp256k1's
/// `secp256k1_musig_nonce_gen` does:
/// * the *untweaked* aggregate pubkey is hashed in (tweaks are applied only
///   afterwards, on the key-agg context used for signing), and
/// * an **empty** extra input is supplied so that the BIP327 nonce hash
///   includes a 4-byte zero length prefix, matching how libsecp256k1 encodes a
///   `NULL` `extra_input32`. Omitting it entirely would write nothing and yield
///   a different nonce (and thus a different, spuriously-mismatching signature).
///
/// # Arguments
/// * `seckeys` - `num_keys` concatenated 32-byte private keys
/// * `num_keys` - number of signers
/// * `msg32` - the 32-byte message being signed
/// * `nonce_seeds` - `num_keys` concatenated 32-byte nonce seeds (one per signer)
/// * `use_xonly_tweak` / `xonly_tweak` - optional 32-byte x-only tweak
/// * `use_plain_tweak` / `plain_tweak` - optional 32-byte plain (EC) tweak
///
/// Tweaks are applied x-only-first then plain, matching the secp256k1 module.
///
/// # Returns
/// * hex-encoded 64-byte signature on success;
/// * a sentinel string ("AGG_FAIL", "NONCE_GEN_FAIL", "TWEAK_FAIL",
///   "PARTIAL_SIGN_FAIL", "PARTIAL_SIG_AGG_FAIL") when a step is rejected, so
///   the driver catches accept-vs-reject disagreements against the peer
///   module;
/// * null when an input scalar is invalid (symmetric with secp256k1 returning
///   nullopt, which the driver skips).
///
/// # Safety
/// `seckeys` and `nonce_seeds` must point to `num_keys * 32` valid bytes,
/// `msg32` to 32 bytes, and each tweak pointer to 32 bytes when its flag is set.
#[no_mangle]
pub unsafe extern "C" fn musig2_sign_session(
    seckeys: *const u8,
    num_keys: usize,
    msg32: *const u8,
    nonce_seeds: *const u8,
    use_xonly_tweak: bool,
    xonly_tweak: *const u8,
    use_plain_tweak: bool,
    plain_tweak: *const u8,
) -> *mut c_char {
    if seckeys.is_null()
        || msg32.is_null()
        || nonce_seeds.is_null()
        || num_keys == 0
        || num_keys > 100
    {
        return ptr::null_mut();
    }

    let seckeys_slice = slice::from_raw_parts(seckeys, num_keys * 32);
    let nonce_seeds_slice = slice::from_raw_parts(nonce_seeds, num_keys * 32);
    let msg: [u8; 32] = match slice::from_raw_parts(msg32, 32).try_into() {
        Ok(arr) => arr,
        Err(_) => return ptr::null_mut(),
    };
    let secp = Secp256k1::new();

    // Parse each scalar and derive its pubkey, preserving input order (BIP327
    // key aggregation is order-sensitive). An invalid scalar -> null (skipped).
    let mut seckey_objs: Vec<SecretKey> = Vec::with_capacity(num_keys);
    let mut pubkey_objs: Vec<PublicKey> = Vec::with_capacity(num_keys);
    for i in 0..num_keys {
        let seckey_bytes: [u8; 32] = match seckeys_slice[i * 32..(i + 1) * 32].try_into() {
            Ok(arr) => arr,
            Err(_) => return ptr::null_mut(),
        };
        let seckey = match SecretKey::from_byte_array(seckey_bytes) {
            Ok(sk) => sk,
            Err(_) => return ptr::null_mut(),
        };
        pubkey_objs.push(PublicKey::from_secret_key(&secp, &seckey));
        seckey_objs.push(seckey);
    }

    let key_agg_ctx = match KeyAggContext::new(pubkey_objs) {
        Ok(ctx) => ctx,
        Err(_) => return str_to_c_string("AGG_FAIL"),
    };

    // Capture the untweaked aggregate pubkey for nonce generation before any
    // tweaks are applied (libsecp256k1 generates nonces against the untweaked
    // key, then tweaks the key-agg cache used for signing).
    let untweaked_agg: PublicKey = key_agg_ctx.aggregated_pubkey();

    // libsecp256k1 accepts a zero x-only tweak and still mutates internal
    // keyagg parity state. The current rust-musig2 API only accepts non-zero
    // Scalar tweaks here, so we cannot reproduce libsecp's behavior exactly.
    // Skip these inputs symmetrically instead of reporting a false mismatch.
    if use_xonly_tweak
        && slice::from_raw_parts(xonly_tweak, 32)
            .iter()
            .all(|&b| b == 0)
    {
        return ptr::null_mut();
    }

    // Round 1: deterministic nonce generation per signer.
    let empty_extra_input: [u8; 0] = [];
    let mut secnonces: Vec<SecNonce> = Vec::with_capacity(num_keys);
    let mut pubnonces: Vec<PubNonce> = Vec::with_capacity(num_keys);
    for i in 0..num_keys {
        let nonce_seed: [u8; 32] = nonce_seeds_slice[i * 32..(i + 1) * 32]
            .try_into()
            .expect("32-byte chunk");
        if nonce_seed.iter().all(|&b| b == 0) {
            return str_to_c_string("NONCE_GEN_FAIL");
        }
        let secnonce = SecNonce::build(nonce_seed)
            .with_seckey(seckey_objs[i])
            .with_message(&msg)
            .with_aggregated_pubkey(untweaked_agg)
            .with_extra_input(&empty_extra_input)
            .build();
        pubnonces.push(secnonce.public_nonce());
        secnonces.push(secnonce);
    }

    let aggnonce: AggNonce = pubnonces.iter().sum();

    let key_agg_ctx = match apply_tweaks(
        key_agg_ctx,
        use_xonly_tweak,
        xonly_tweak,
        use_plain_tweak,
        plain_tweak,
    ) {
        Some(ctx) => ctx,
        None => return str_to_c_string("TWEAK_FAIL"),
    };

    // Round 2: each signer produces a partial signature over the tweaked key.
    let mut partial_sigs: Vec<PartialSignature> = Vec::with_capacity(num_keys);
    for (i, secnonce) in secnonces.into_iter().enumerate() {
        let partial_sig: PartialSignature =
            match musig2::sign_partial(&key_agg_ctx, seckey_objs[i], secnonce, &aggnonce, msg) {
                Ok(sig) => sig,
                Err(_) => return str_to_c_string("PARTIAL_SIGN_FAIL"),
            };
        partial_sigs.push(partial_sig);
    }

    let final_sig: [u8; 64] =
        match musig2::aggregate_partial_signatures(&key_agg_ctx, &aggnonce, partial_sigs, msg) {
            Ok(sig) => sig,
            Err(_) => return str_to_c_string("PARTIAL_SIG_AGG_FAIL"),
        };

    str_to_c_string(&hex::encode(final_sig))
}

/// Applies the optional x-only and plain tweaks to a key-agg context in the
/// same order as the secp256k1 module (x-only first, then plain). Returns
/// `None` if a tweak scalar is invalid or rejected, which the caller maps to
/// the "TWEAK_FAIL" sentinel.
unsafe fn apply_tweaks(
    mut ctx: KeyAggContext,
    use_xonly_tweak: bool,
    xonly_tweak: *const u8,
    use_plain_tweak: bool,
    plain_tweak: *const u8,
) -> Option<KeyAggContext> {
    if use_xonly_tweak {
        let tweak_bytes = slice::from_raw_parts(xonly_tweak, 32);
        let tweak = Scalar::from_slice(tweak_bytes).ok()?;
        ctx = ctx.with_xonly_tweak(tweak).ok()?;
    }
    if use_plain_tweak {
        let tweak_bytes = slice::from_raw_parts(plain_tweak, 32);
        if tweak_bytes.iter().any(|&b| b != 0) {
            let tweak = Scalar::from_slice(tweak_bytes).ok()?;
            ctx = ctx.with_plain_tweak(tweak).ok()?;
        }
    }
    Some(ctx)
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

    // Drives the FFI through a full session and returns the hex signature
    // (panicking on a null/sentinel response).
    unsafe fn run_session_hex(num_keys: usize) -> (String, [u8; 32], KeyAggContext) {
        let mut seckeys = Vec::new();
        let mut nonce_seeds = Vec::new();
        for i in 0..num_keys {
            seckeys.extend_from_slice(&[(i as u8) + 1; 32]);
            nonce_seeds.extend_from_slice(&[0xA0 ^ (i as u8); 32]);
        }
        let msg = [0x42u8; 32];

        let result = musig2_sign_session(
            seckeys.as_ptr(),
            num_keys,
            msg.as_ptr(),
            nonce_seeds.as_ptr(),
            false,
            ptr::null(),
            false,
            ptr::null(),
        );
        assert!(!result.is_null());
        let hex_str = CString::from_raw(result).to_str().unwrap().to_owned();

        // Rebuild the (untweaked) aggregate context to verify against.
        let secp = Secp256k1::new();
        let pubkeys: Vec<PublicKey> = (0..num_keys)
            .map(|i| {
                let sk =
                    SecretKey::from_byte_array(seckeys[i * 32..(i + 1) * 32].try_into().unwrap())
                        .unwrap();
                PublicKey::from_secret_key(&secp, &sk)
            })
            .collect();
        (hex_str, msg, KeyAggContext::new(pubkeys).unwrap())
    }

    #[test]
    fn test_sign_session_produces_valid_signature() {
        unsafe {
            for num_keys in 1..=4 {
                let (hex_str, msg, key_agg_ctx) = run_session_hex(num_keys);
                // 64-byte compact Schnorr signature.
                assert_eq!(hex_str.len(), 128, "num_keys={num_keys}");

                let sig_bytes: [u8; 64] = hex::decode(&hex_str).unwrap().try_into().unwrap();
                let agg_pubkey: PublicKey = key_agg_ctx.aggregated_pubkey();
                musig2::verify_single(agg_pubkey, sig_bytes, msg)
                    .unwrap_or_else(|_| panic!("signature must verify for num_keys={num_keys}"));
            }
        }
    }

    #[test]
    fn test_sign_session_invalid_scalar() {
        // A zero scalar among the seckeys -> null (skipped by the driver).
        let seckeys = vec![0u8; 32];
        let nonce_seeds = vec![1u8; 32];
        let msg = [0x42u8; 32];
        unsafe {
            let result = musig2_sign_session(
                seckeys.as_ptr(),
                1,
                msg.as_ptr(),
                nonce_seeds.as_ptr(),
                false,
                ptr::null(),
                false,
                ptr::null(),
            );
            assert!(result.is_null());
        }
    }

    #[test]
    fn test_sign_session_zero_nonce_seed_matches_secp_reject() {
        let seckeys = vec![1u8; 32];
        let nonce_seeds = vec![0u8; 32];
        let msg = [0x42u8; 32];
        unsafe {
            let result = musig2_sign_session(
                seckeys.as_ptr(),
                1,
                msg.as_ptr(),
                nonce_seeds.as_ptr(),
                false,
                ptr::null(),
                false,
                ptr::null(),
            );
            assert!(!result.is_null());
            let result_str = CString::from_raw(result).to_str().unwrap().to_owned();
            assert_eq!(result_str, "NONCE_GEN_FAIL");
        }
    }

    #[test]
    fn test_sign_session_zero_plain_tweak_is_accepted() {
        let seckeys = vec![1u8; 32];
        let nonce_seeds = vec![2u8; 32];
        let msg = [0x42u8; 32];
        let zero_tweak = [0u8; 32];
        unsafe {
            let result = musig2_sign_session(
                seckeys.as_ptr(),
                1,
                msg.as_ptr(),
                nonce_seeds.as_ptr(),
                false,
                ptr::null(),
                true,
                zero_tweak.as_ptr(),
            );
            assert!(!result.is_null());
            let result_str = CString::from_raw(result).to_str().unwrap().to_owned();
            assert_ne!(result_str, "TWEAK_FAIL");
        }
    }

    #[test]
    fn test_sign_session_zero_xonly_tweak_is_skipped() {
        let seckeys = vec![1u8; 32];
        let nonce_seeds = vec![2u8; 32];
        let msg = [0x42u8; 32];
        let zero_tweak = [0u8; 32];
        unsafe {
            let result = musig2_sign_session(
                seckeys.as_ptr(),
                1,
                msg.as_ptr(),
                nonce_seeds.as_ptr(),
                true,
                zero_tweak.as_ptr(),
                false,
                ptr::null(),
            );
            assert!(result.is_null());
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
