use k256::ecdsa::signature::hazmat::{PrehashSigner, PrehashVerifier};
use k256::ecdsa::{Signature, SigningKey, VerifyingKey};
use k256::elliptic_curve::ops::Reduce;
use k256::schnorr::SigningKey as SchnorrSigningKey;
use k256::sha2::{Digest, Sha256};
use k256::{EncodedPoint, ProjectivePoint, Scalar, U256};
use std::slice;

#[path = "../../../../include/bitcoinfuzz/ffi.rs"]
mod ffi;
use ffi::BfResult;

#[no_mangle]
pub unsafe extern "C" fn k256_private_to_public_key(buffer: *const u8) -> BfResult {
    let privkey_slice = slice::from_raw_parts(buffer, 32);

    let priv_key = match SigningKey::from_slice(privkey_slice) {
        Ok(k) => k,
        Err(_) => {
            return BfResult::skip();
        }
    };

    let pub_key = priv_key.verifying_key().to_encoded_point(true);

    return BfResult::ok(hex::encode(pub_key));
}

/// Parses a SEC1-encoded public key and returns the canonical compressed
/// encoding, so the driver compares both acceptance and the accepted point.
/// Note k256's sec1 layer has no hybrid (0x06/0x07) tag and accepts the
/// compact (0x05) encoding; the driver skips those input classes for this
/// module.
#[no_mangle]
pub unsafe extern "C" fn k256_pubkey_parse(data: *const u8, len: usize) -> BfResult {
    let bytes = slice::from_raw_parts(data, len);

    let pub_key = match k256::PublicKey::from_sec1_bytes(bytes) {
        Ok(k) => k,
        Err(_) => {
            return BfResult::fail_with("ERR");
        }
    };

    let compressed = VerifyingKey::from(pub_key).to_encoded_point(true);
    return BfResult::ok(format!("OK:{}", hex::encode(compressed.as_bytes())));
}

#[no_mangle]
pub unsafe extern "C" fn k256_sign_compact(buffer: *const u8, hash: *const u8) -> BfResult {
    let privkey_slice = slice::from_raw_parts(buffer, 32);
    let hash_slice = slice::from_raw_parts(hash, 32);

    let priv_key = match SigningKey::from_slice(privkey_slice) {
        Ok(k) => k,
        Err(_) => {
            return BfResult::skip();
        }
    };

    // Reduce the hash modulo the secp256k1 curve order to ensure compatibility with
    // libsecp256k1's behavior.
    let scalar: Scalar = <Scalar as Reduce<U256>>::reduce(&U256::from_be_slice(hash_slice));
    let signature: Signature = match priv_key.sign_prehash(&mut scalar.to_bytes()) {
        Ok(s) => s,
        Err(_) => {
            return BfResult::fail();
        }
    };

    return BfResult::ok(hex::encode(signature.to_bytes()));
}

#[no_mangle]
pub unsafe extern "C" fn k256_sign_der(buffer: *const u8, hash: *const u8) -> BfResult {
    let privkey_slice = slice::from_raw_parts(buffer, 32);
    let hash_slice = slice::from_raw_parts(hash, 32);

    let priv_key = match SigningKey::from_slice(privkey_slice) {
        Ok(k) => k,
        Err(_) => {
            return BfResult::skip();
        }
    };

    // Reduce the hash modulo the secp256k1 curve order to ensure compatibility with
    // libsecp256k1's behavior.
    let scalar: Scalar = <Scalar as Reduce<U256>>::reduce(&U256::from_be_slice(hash_slice));
    let signature: Signature = match priv_key.sign_prehash(&mut scalar.to_bytes()) {
        Ok(s) => s,
        Err(_) => {
            return BfResult::fail();
        }
    };

    return BfResult::ok(hex::encode(signature.to_der()));
}

#[no_mangle]
pub unsafe extern "C" fn k256_sign_verify(
    buffer: *const u8,
    hash: *const u8,
    sign: *const u8,
    sign_len: usize,
) -> bool {
    let privkey_slice = slice::from_raw_parts(buffer, 32);
    let hash_slice = slice::from_raw_parts(hash, 32);
    let sign_slice = slice::from_raw_parts(sign, sign_len);

    let priv_key = match SigningKey::from_slice(privkey_slice) {
        Ok(k) => k,
        Err(_) => {
            return false;
        }
    };

    let pub_key = priv_key.verifying_key();
    let signature = match Signature::from_der(sign_slice) {
        Ok(s) => s,
        Err(_) => {
            return false;
        }
    };

    return pub_key.verify_prehash(hash_slice, &signature).is_ok();
}

#[no_mangle]
pub unsafe extern "C" fn k256_ecdh(buffer: *const u8, pubkey: *const u8) -> BfResult {
    let privkey_slice = slice::from_raw_parts(buffer, 32);
    let pubkey_slice = slice::from_raw_parts(pubkey, 33);

    let priv_key = match SigningKey::from_slice(privkey_slice) {
        Ok(k) => k,
        Err(_) => {
            return BfResult::skip();
        }
    };

    let pub_key = match VerifyingKey::from_sec1_bytes(pubkey_slice) {
        Ok(p) => p,
        Err(_) => {
            return BfResult::skip();
        }
    };

    // Here, we are not using the k256 provided diffie_hellman function because, although it currently performs
    // the same internal operations we do, it only returns the x-coordinate of the generated secret.
    // However, we need the x coordinate and y parity to generate the compressed shared public key bytes.
    let public_point = ProjectivePoint::from(*pub_key.as_affine());
    let shared_point = (public_point * priv_key.as_nonzero_scalar().as_ref()).to_affine();
    let shared_secret = Sha256::digest(EncodedPoint::from(&shared_point).as_bytes());

    return BfResult::ok(hex::encode(shared_secret));
}

#[no_mangle]
pub unsafe extern "C" fn k256_sign_schnorr(
    buffer: *const u8,
    hash: *const u8,
    aux: *const u8,
) -> BfResult {
    let privkey_slice = slice::from_raw_parts(buffer, 32);
    let hash_slice = slice::from_raw_parts(hash, 32);
    let aux_slice = slice::from_raw_parts(aux, 32);

    let aux_bytes: [u8; 32] = aux_slice
        .try_into()
        .expect("aux_slice is guaranteed to be 32 bytes");

    let priv_key = match SchnorrSigningKey::from_slice(privkey_slice) {
        Ok(k) => k,
        Err(_) => {
            return BfResult::skip();
        }
    };

    let signature = match priv_key.sign_raw(hash_slice, &aux_bytes) {
        Ok(s) => s,
        Err(_) => {
            return BfResult::fail();
        }
    };

    return BfResult::ok(hex::encode(signature.to_bytes()));
}
