#![allow(unused)]

use std::ffi::CString;
use std::os::raw::c_char;
use std::ptr::slice_from_raw_parts;
use std::{ptr, slice};

use rustreexo::accumulator::node_hash::BitcoinNodeHash;
use rustreexo::accumulator::proof::Proof;
use rustreexo::accumulator::stump::{Stump, StumpError, UpdateData};

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rustreexo_stump_modify(
    add_hashes_flat: *const u8,
    add_hashes_count: usize,
) -> *mut c_char {
    // Create new txout hashes.
    let new_txout_hashes: Vec<BitcoinNodeHash> =
        slice::from_raw_parts(add_hashes_flat, add_hashes_count * 32)
            .chunks_exact(32)
            .map(|chunk| BitcoinNodeHash::new(chunk.try_into().unwrap()))
            .collect();

    // Create a new stump and add the new txout hashes to it.
    let stump = Stump::new();
    let (stump, _) = match stump.modify(&new_txout_hashes, &[], &Proof::default()) {
        Ok(s) => s,
        Err(_) => return str_to_c_string(""),
    };

    // Serialize the `Stump` into a hex string.
    let mut stump_ser: Vec<u8> = Vec::new();
    stump
        .serialize(&mut stump_ser)
        .expect("Vec<u8> serialization should not fail");
    str_to_c_string(&hex::encode(stump_ser))
}

#[unsafe(no_mangle)]
/// Verify the validity of a [`Proof`] based on the [`Stump`].
///
/// Arguments:
///  - The [`Proof`] being verified
///  - The [hashes]() being deleted from the [`Stump`].
pub unsafe extern "C" fn rustreexo_verify(buffer: *const u8) -> *mut c_char {
    unimplemented!()
}

unsafe fn str_to_c_string(input: &str) -> *mut c_char {
    CString::new(input).unwrap().into_raw()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rustreexo_free_string(ptr: *mut c_char) {
    if !ptr.is_null() {
        let _ = CString::from_raw(ptr);
    }
}
