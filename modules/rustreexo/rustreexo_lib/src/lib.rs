use std::slice;

use rustreexo::accumulator::node_hash::BitcoinNodeHash;
use rustreexo::accumulator::proof::Proof;
use rustreexo::accumulator::stump::Stump;

#[path = "../../../../include/bitcoinfuzz/ffi.rs"]
mod ffi;
use ffi::BfResult;

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rustreexo_stump_modify(
    add_hashes_flat: *const u8,
    add_hashes_count: usize,
) -> BfResult {
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
        Err(_) => return BfResult::fail(),
    };

    // Serialize the `Stump` into a hex string.
    let mut stump_ser: Vec<u8> = Vec::new();
    stump
        .serialize(&mut stump_ser)
        .expect("Vec<u8> serialization should not fail");
    BfResult::ok(hex::encode(stump_ser))
}
