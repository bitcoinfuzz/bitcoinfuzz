use std::slice;

use psbt_v2::{DetermineLockTimeError, Psbt};

#[path = "../../../../include/bitcoinfuzz/ffi.rs"]
mod ffi;
use ffi::BfResult;

// Version-agnostic view of the fields the differential fuzz targets compare
// (matching the Bitcoin Core module's output format exactly). The BIP-370
// fields are only emitted by the PSBTv2 target.
struct GlobalSummary {
    tx_version: u32,
    fallback_lock_time: Option<u32>,
    // An absent PSBT_GLOBAL_TX_MODIFIABLE reads back as 0.
    tx_modifiable: u8,
    lock_time: u32,
}

struct InputSummary {
    prev_txid: String,
    prev_vout: u32,
    // `None` when the sequence number is omitted, which can only happen for
    // PSBTv2 (BIP-370 says it then defaults to the final sequence number).
    sequence: Option<u32>,
    required_time: Option<u32>,
    required_height: Option<u32>,
    has_utxo: bool,
    partial_signatures: usize,
    redeem_script_hex: String,
    witness_script_hex: String,
    // Raw PSBT_IN_SIGHASH_TYPE byte value, or 0 if unset.
    sighash_type: u32,
    bip32_count: usize,
    // Whether the input carries a *non-empty* final scriptSig or scriptWitness.
    // Emptiness rather than presence is deliberate: Bitcoin Core stores
    // `final_script_witness` as a plain `CScriptWitness` (not an optional), and
    // its `IsNull()` is just `stack.empty()`, so Core cannot distinguish an
    // absent PSBT_IN_FINAL_SCRIPTWITNESS key from one present with a zero-item
    // stack. Presence-based semantics would therefore make this flag
    // incomparable across modules for that (degenerate, information-free)
    // input. An empty witness finalizes nothing, so "non-empty" is also the
    // more meaningful reading.
    finalized: bool,
}

struct OutputSummary {
    value: i64,
    script_hex: String,
    redeem_script_hex: String,
    witness_script_hex: String,
    bip32_count: usize,
}

fn opt_to_string(value: Option<u32>) -> String {
    value.map(|v| v.to_string()).unwrap_or_default()
}

fn format_result(
    v2: bool,
    global: &GlobalSummary,
    inputs: &[InputSummary],
    outputs: &[OutputSummary],
) -> String {
    let mut result = String::new();

    result.push_str(&format!("tx_version={};", global.tx_version));
    if v2 {
        result.push_str(&format!(
            "fallback_locktime={};",
            opt_to_string(global.fallback_lock_time)
        ));
        result.push_str(&format!("tx_modifiable={};", global.tx_modifiable));
    }
    result.push_str(&format!("lock_time={};", global.lock_time));
    result.push_str(&format!("inputs={};", inputs.len()));
    result.push_str(&format!("outputs={};", outputs.len()));

    for (i, input) in inputs.iter().enumerate() {
        result.push_str(&format!(
            "input{}previous_output={}:{};",
            i, input.prev_txid, input.prev_vout
        ));
        result.push_str(&format!(
            "input{}sequence={};",
            i,
            opt_to_string(input.sequence)
        ));

        if v2 {
            result.push_str(&format!(
                "input{}required_time={};",
                i,
                opt_to_string(input.required_time)
            ));
            result.push_str(&format!(
                "input{}required_height={};",
                i,
                opt_to_string(input.required_height)
            ));
        }

        if input.has_utxo {
            result.push_str(&format!("input{}utxo=1;", i));
        }

        result.push_str(&format!(
            "input{}partial_signatures={};",
            i, input.partial_signatures
        ));
        result.push_str(&format!(
            "input{}redeem_script={};",
            i, input.redeem_script_hex
        ));
        result.push_str(&format!(
            "input{}witness_script={};",
            i, input.witness_script_hex
        ));
        result.push_str(&format!("input{}sighash_type={};", i, input.sighash_type));
        result.push_str(&format!("input{}bip32={};", i, input.bip32_count));
        if input.finalized {
            result.push_str(&format!("input{}finalized=1;", i));
        }
    }

    for (i, output) in outputs.iter().enumerate() {
        result.push_str(&format!("output{}val={};", i, output.value));
        result.push_str(&format!("output{}script={};", i, output.script_hex));
        result.push_str(&format!(
            "output{}redeem_script={};",
            i, output.redeem_script_hex
        ));
        result.push_str(&format!(
            "output{}witness_script={};",
            i, output.witness_script_hex
        ));
        result.push_str(&format!("output{}bip32={};", i, output.bip32_count));
    }

    result
}

// rust-psbt decodes a PSBTv0 into the same `Psbt` type as a PSBTv2 (moving
// the unsigned tx fields into the per-input/output maps), so both versions
// share `summarize`.
fn try_parse_v0(data: &[u8]) -> Option<String> {
    let psbt = Psbt::deserialize_v0(data).ok()?;
    summarize(&psbt, false).ok()
}

// Minimal CompactSize (Bitcoin's little-endian varint) decoder, returning
// the decoded value and the number of bytes it occupied.
fn read_compact_size(data: &[u8]) -> Option<(u64, usize)> {
    let first = *data.first()?;
    match first {
        0..=252 => Some((first as u64, 1)),
        253 => Some((
            u16::from_le_bytes(data.get(1..3)?.try_into().ok()?) as u64,
            3,
        )),
        254 => Some((
            u32::from_le_bytes(data.get(1..5)?.try_into().ok()?) as u64,
            5,
        )),
        255 => Some((u64::from_le_bytes(data.get(1..9)?.try_into().ok()?), 9)),
    }
}

fn v2_counts_are_plausible(data: &[u8]) -> bool {
    const MAGIC: &[u8] = b"psbt\xff";
    if data.len() < MAGIC.len() || &data[..MAGIC.len()] != MAGIC {
        return true; // not our concern here; let the real parser reject it
    }

    let mut offset = MAGIC.len();
    while offset < data.len() {
        let Some((keylen, keylen_size)) = read_compact_size(&data[offset..]) else {
            return true;
        };
        offset += keylen_size;
        if keylen == 0 {
            return true; // end of the global map
        }
        let Some(key_end) = offset
            .checked_add(keylen as usize)
            .filter(|&e| e <= data.len())
        else {
            return true;
        };
        let key = &data[offset..key_end];
        offset = key_end;

        let Some((vallen, vallen_size)) = read_compact_size(&data[offset..]) else {
            return true;
        };
        offset += vallen_size;
        let Some(val_end) = offset
            .checked_add(vallen as usize)
            .filter(|&e| e <= data.len())
        else {
            return true;
        };
        let value = &data[offset..val_end];
        offset = val_end;

        // PSBT_GLOBAL_INPUT_COUNT = 0x04, PSBT_GLOBAL_OUTPUT_COUNT = 0x05
        // (single-byte key, no key data).
        if key.len() == 1 && (key[0] == 0x04 || key[0] == 0x05) {
            if let Some((count, _)) = read_compact_size(value) {
                if count > data.len() as u64 {
                    return false;
                }
            }
        }
    }
    true
}

// Distinguishes a PSBT that isn't a valid PSBTv2 at all from one that parsed
// fine but hits the BIP-370 conflicting-lock-time case, a well-defined
// "invalid PSBT" outcome every module should agree on, mirroring the Bitcoin
// Core module.
enum TryParseV2Error {
    Invalid,
    ConflictingLockTime,
}

fn try_parse_v2(data: &[u8]) -> Result<String, TryParseV2Error> {
    if !v2_counts_are_plausible(data) {
        return Err(TryParseV2Error::Invalid);
    }
    let psbt = Psbt::deserialize(data).map_err(|_| TryParseV2Error::Invalid)?;
    summarize(&psbt, true).map_err(|_| TryParseV2Error::ConflictingLockTime)
}

fn summarize(psbt: &Psbt, v2: bool) -> Result<String, DetermineLockTimeError> {
    let global = GlobalSummary {
        tx_version: psbt.global.tx_version.0 as u32,
        fallback_lock_time: psbt
            .global
            .fallback_lock_time
            .map(|lt| lt.to_consensus_u32()),
        tx_modifiable: psbt.global.tx_modifiable_flags,
        lock_time: psbt.determine_lock_time()?.to_consensus_u32(),
    };

    let inputs: Vec<InputSummary> = psbt
        .inputs
        .iter()
        .map(|psbt_input| InputSummary {
            prev_txid: psbt_input.previous_txid.to_string(),
            prev_vout: psbt_input.spent_output_index,
            sequence: psbt_input.sequence.map(|s| s.0),
            required_time: psbt_input.min_time.map(|t| t.to_consensus_u32()),
            required_height: psbt_input.min_height.map(|h| h.to_consensus_u32()),
            has_utxo: psbt_input.witness_utxo.is_some() || psbt_input.non_witness_utxo.is_some(),
            partial_signatures: psbt_input.partial_sigs.len(),
            redeem_script_hex: psbt_input
                .redeem_script
                .as_ref()
                .map(|s| s.to_hex_string())
                .unwrap_or_default(),
            witness_script_hex: psbt_input
                .witness_script
                .as_ref()
                .map(|s| s.to_hex_string())
                .unwrap_or_default(),
            sighash_type: psbt_input.sighash_type.map(|s| s.to_u32()).unwrap_or(0),
            bip32_count: psbt_input.bip32_derivations.len(),
            finalized: psbt_input
                .final_script_sig
                .as_ref()
                .is_some_and(|s| !s.is_empty())
                || psbt_input
                    .final_script_witness
                    .as_ref()
                    .is_some_and(|w| !w.is_empty()),
        })
        .collect();

    let outputs: Vec<OutputSummary> = psbt
        .outputs
        .iter()
        .map(|psbt_output| OutputSummary {
            value: psbt_output.amount.to_sat() as i64,
            script_hex: psbt_output.script_pubkey.to_hex_string(),
            redeem_script_hex: psbt_output
                .redeem_script
                .as_ref()
                .map(|s| s.to_hex_string())
                .unwrap_or_default(),
            witness_script_hex: psbt_output
                .witness_script
                .as_ref()
                .map(|s| s.to_hex_string())
                .unwrap_or_default(),
            bip32_count: psbt_output.bip32_derivations.len(),
        })
        .collect();

    Ok(format_result(v2, &global, &inputs, &outputs))
}

// PSBTv0 target: a PSBT that only parses as v2 belongs to the v2 target, so
// it's skipped rather than reported as invalid.
#[no_mangle]
pub unsafe extern "C" fn rust_psbt_psbt_v0_parse(data: *const u8, len: usize) -> BfResult {
    let data_slice = slice::from_raw_parts(data, len);

    if let Some(result) = try_parse_v0(data_slice) {
        return BfResult::ok(result);
    }

    match try_parse_v2(data_slice) {
        Ok(_) | Err(TryParseV2Error::ConflictingLockTime) => BfResult::skip(),
        Err(TryParseV2Error::Invalid) => BfResult::fail_with("INVALID"),
    }
}

// PSBTv2 target: a PSBT that parses as v0 belongs to the v0 target, so it's
// skipped rather than reported as invalid.
#[no_mangle]
pub unsafe extern "C" fn rust_psbt_psbt_v2_parse(data: *const u8, len: usize) -> BfResult {
    let data_slice = slice::from_raw_parts(data, len);

    if try_parse_v0(data_slice).is_some() {
        return BfResult::skip();
    }

    match try_parse_v2(data_slice) {
        Ok(result) => BfResult::ok(result),
        // Conflicting per-input lock time requirements (BIP-370) is a
        // well-defined "reject" outcome, not a generic parse failure. Fail
        // with a named reason so it's actually compared across modules (the
        // driver's PSBT targets skip empty results from comparison entirely)
        // rather than silently opted out, mirroring the other PSBTv2-aware
        // modules.
        Err(TryParseV2Error::ConflictingLockTime) => BfResult::fail_with("CONFLICTING_LOCKTIME"),
        Err(TryParseV2Error::Invalid) => BfResult::fail_with("INVALID"),
    }
}
