use std::slice;

use rustreexo::mem_forest::MemForest;
use rustreexo::node_hash::{AccumulatorHash, BitcoinNodeHash};
use rustreexo::proof::Proof;
use rustreexo::stump::Stump;

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
    let stump = match stump.modify(&new_txout_hashes, &[], &Proof::default()) {
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

unsafe fn to_hashes(ptr: *const u8, count: usize) -> Vec<BitcoinNodeHash> {
    if count == 0 {
        return Vec::new();
    }
    slice::from_raw_parts(ptr, count * 32)
        .chunks_exact(32)
        .map(|chunk| BitcoinNodeHash::new(chunk.try_into().unwrap()))
        .collect()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn rustreexo_stump_update(
    add_hashes_flat: *const u8,
    add_count: usize,
    del_hashes_flat: *const u8,
    del_count: usize,
    new_add_hashes_flat: *const u8,
    new_add_count: usize,
) -> BfResult {
    let adds = to_hashes(add_hashes_flat, add_count);
    let dels = to_hashes(del_hashes_flat, del_count);
    let new_adds = to_hashes(new_add_hashes_flat, new_add_count);

    // Build the full accumulator over the first batch so it can prove the
    // deletions.
    let mut forest = MemForest::<BitcoinNodeHash>::new();
    if forest.modify(&adds, &[]).is_err() {
        return BfResult::fail();
    }

    // Track the same state in a Stump (the compact-state client).
    let stump = Stump::new();
    let stump = match stump.modify(&adds, &[], &Proof::default()) {
        Ok(s) => s,
        Err(_) => return BfResult::fail(),
    };

    let proof = match forest.prove(&dels) {
        Ok(p) => p,
        Err(_) => return BfResult::fail(),
    };

    // The second update deletes the proven leaves and adds the new batch,
    // like a block spending and creating UTXOs.
    let stump = match stump.modify(&new_adds, &dels, &proof) {
        Ok(s) => s,
        Err(_) => return BfResult::fail(),
    };
    if forest.modify(&new_adds, &dels).is_err() {
        return BfResult::fail();
    }

    let mut stump_ser: Vec<u8> = Vec::new();
    stump
        .serialize(&mut stump_ser)
        .expect("Vec<u8> serialization should not fail");

    // Serialize the forest roots the same way `Stump::serialize` writes its
    // roots, so both states get compared across modules.
    let roots = forest.get_roots();
    let mut roots_ser: Vec<u8> = Vec::new();
    roots_ser.extend_from_slice(&(roots.len() as u64).to_le_bytes());
    for root in roots {
        root.get_data()
            .write(&mut roots_ser)
            .expect("Vec<u8> serialization should not fail");
    }

    BfResult::ok(format!(
        "{}:{}",
        hex::encode(stump_ser),
        hex::encode(roots_ser)
    ))
}
