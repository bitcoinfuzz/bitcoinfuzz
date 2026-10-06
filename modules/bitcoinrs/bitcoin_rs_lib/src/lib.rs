use bitcoin_rs_consensus::verify_block::verify_block_rules;
use bitcoin_rs_consensus::verify_coinbase_script_sig_size;
use bitcoin_rs_consensus::verify_tx::verify_transaction_input_outpoints;
use bitcoin_rs_primitives::encode::ConsensusDecode;
use bitcoin_rs_primitives::{Block, Tx};
use bitcoin_rs_script::count_tx_legacy;
use std::ffi::CString;
use std::os::raw::c_char;

const MAX_MONEY: u64 = 21_000_000 * 100_000_000;
const WITNESS_SCALE_FACTOR: u64 = 4;
const MAX_BLOCK_SIGOPS_COST: u64 = 80_000;

/// Block-level sigop limit of Bitcoin Core's `CheckBlock` (`bad-blk-sigops`):
/// the legacy sigop count summed over all transactions, times
/// WITNESS_SCALE_FACTOR, must not exceed MAX_BLOCK_SIGOPS_COST.
fn check_block_legacy_sigops(txs: &[Tx]) -> bool {
    let sigops = txs.iter().fold(0_u64, |acc, tx| {
        acc.saturating_add(u64::from(count_tx_legacy(tx)))
    });
    sigops.saturating_mul(WITNESS_SCALE_FACTOR) <= MAX_BLOCK_SIGOPS_COST
}

/// Context-free transaction checks, the scope of Bitcoin Core's
/// `CheckTransaction` (called per tx by `CheckBlock`): non-empty vin/vout,
/// output values in range, coinbase scriptSig size, null/duplicate inputs.
/// bitcoin-rs keeps the empty and value checks private to its contextual
/// pipeline, so they are repeated here; nothing in this function needs a
/// UTXO view, so it cannot reject on contextual (value-in vs value-out) rules.
fn check_transaction_context_free(tx: &Tx) -> bool {
    if tx.inputs.is_empty() || tx.outputs.is_empty() {
        return false;
    }
    let mut total: u64 = 0;
    for output in &tx.outputs {
        let value = output.value.to_sat();
        if value > MAX_MONEY {
            return false;
        }
        total = match total.checked_add(value) {
            Some(sum) if sum <= MAX_MONEY => sum,
            _ => return false,
        };
    }
    verify_coinbase_script_sig_size(tx).is_ok() && verify_transaction_input_outpoints(tx).is_ok()
}

fn str_to_c_string(input: &str) -> *mut c_char {
    CString::new(input).unwrap().into_raw()
}

/// Frees a C string created by `str_to_c_string`.
///
/// # Safety
/// The pointer must come from this library and must not be freed twice.
#[no_mangle]
pub unsafe extern "C" fn bitcoin_rs_free_c_string(ptr: *mut c_char) {
    if !ptr.is_null() {
        let _ = CString::from_raw(ptr);
    }
}

/// Mirrors Bitcoin Core's `deserialize_block` harness: trailing bytes are
/// tolerated (Core streams the block and ignores the rest), a block failing
/// the non-contextual rules yields "0", otherwise the block hash is returned
/// in display byte order.
///
/// # Safety
/// `data` must be valid for `len` bytes (or `len` must be 0).
#[no_mangle]
pub unsafe extern "C" fn bitcoin_rs_des_block(data: *const u8, len: usize) -> *mut c_char {
    let data_slice: &[u8] = if len == 0 {
        &[]
    } else {
        std::slice::from_raw_parts(data, len)
    };

    let mut reader = data_slice;
    let block = match Block::consensus_decode(&mut reader) {
        Ok(block) => block,
        Err(_) => return str_to_c_string("0"),
    };

    if verify_block_rules(&block).is_err() {
        return str_to_c_string("0");
    }
    if !block.txs.iter().all(check_transaction_context_free) {
        return str_to_c_string("0");
    }
    if !check_block_legacy_sigops(&block.txs) {
        return str_to_c_string("0");
    }

    str_to_c_string(&block.block_hash().0.to_string_be())
}
