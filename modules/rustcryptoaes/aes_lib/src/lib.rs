use aes::cipher::block_padding::{NoPadding, Pkcs7};
use aes::cipher::{BlockModeDecrypt, BlockModeEncrypt, KeyIvInit};
use std::slice;

#[path = "../../../../include/bitcoinfuzz/ffi.rs"]
mod ffi;
use ffi::BfResult;

type Aes256CbcEnc = cbc::Encryptor<aes::Aes256>;
type Aes256CbcDec = cbc::Decryptor<aes::Aes256>;

const BLOCK_SIZE: usize = 16;
const KEY_SIZE: usize = 32;

fn cbc_encrypt(key: &[u8], iv: &[u8], pad: bool, data: &[u8]) -> String {
    // Mirror Bitcoin Core's CBCEncrypt: without padding the input must be a
    // whole number of blocks.
    if !pad && data.len() % BLOCK_SIZE != 0 {
        return "ERR".to_string();
    }

    let enc = Aes256CbcEnc::new_from_slices(key, iv).expect("key and iv sizes are fixed");
    let ciphertext = if pad {
        enc.encrypt_padded_vec::<Pkcs7>(data)
    } else {
        enc.encrypt_padded_vec::<NoPadding>(data)
    };
    hex::encode(ciphertext)
}

fn cbc_decrypt(key: &[u8], iv: &[u8], pad: bool, data: &[u8]) -> String {
    // Mirror Bitcoin Core's CBCDecrypt: empty or partial-block input fails.
    if data.is_empty() || data.len() % BLOCK_SIZE != 0 {
        return "ERR".to_string();
    }

    let dec = Aes256CbcDec::new_from_slices(key, iv).expect("key and iv sizes are fixed");
    let result = if pad {
        dec.decrypt_padded_vec::<Pkcs7>(data)
    } else {
        dec.decrypt_padded_vec::<NoPadding>(data)
    };
    match result {
        Ok(plaintext) => hex::encode(plaintext),
        Err(_) => "ERR".to_string(),
    }
}

/// Encrypts and decrypts `data` with AES-256-CBC and returns
/// "enc=<hex|ERR> dec=<hex|ERR>".
#[no_mangle]
pub unsafe extern "C" fn rustcrypto_aes256_cbc(
    key: *const u8,
    iv: *const u8,
    pad: bool,
    data: *const u8,
    len: usize,
) -> BfResult {
    let key_slice = slice::from_raw_parts(key, KEY_SIZE);
    let iv_slice = slice::from_raw_parts(iv, BLOCK_SIZE);
    let data_slice = slice::from_raw_parts(data, len);

    let result = format!(
        "enc={} dec={}",
        cbc_encrypt(key_slice, iv_slice, pad, data_slice),
        cbc_decrypt(key_slice, iv_slice, pad, data_slice)
    );

    BfResult::ok(result)
}
