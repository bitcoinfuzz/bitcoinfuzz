use bdk_sp::{
    bitcoin::{
        hashes::Hash,
        key::{Parity, Secp256k1, TweakedPublicKey},
        secp256k1::{All, PublicKey, Scalar, SecretKey},
        Network, ScriptBuf, WPubkeyHash,
    },
    encoding::SilentPaymentCode,
    send::{create_silentpayment_partial_secret, create_silentpayment_scriptpubkeys},
};
use std::collections::HashMap;
use std::slice;

#[path = "../../../../include/bitcoinfuzz/ffi.rs"]
mod ffi;
use ffi::BfResult;

/// Kept byte-for-byte in sync with the peer modules' sentinel: the driver
/// compares the responses verbatim, so a different spelling would read as a
/// mismatch instead of as agreement on rejecting the key.
const INVALID_SECKEY: &str = "INVALID_SECKEY";

/// Returned when tweaking a spend key with its label is rejected, which needs a
/// label hash outside the curve order or a label that negates the spend key.
/// Kept distinct from CREATE_FAIL so a disagreement points at the step.
const LABEL_FAIL: &str = "LABEL_FAIL";

/// Creates the BIP-352 Silent Payments outputs for a set of recipients, mirroring
/// the `secp256k1` module's `secp256k1_silentpayments_create_outputs` so the C++
/// driver can compare the two byte-for-byte.
///
/// `bdk_sp` selects the input keys eligible for the shared secret by inspecting
/// each input's script pubkey, so a placeholder P2TR or P2WPKH script is built
/// per input key according to `input_is_taproot`. That mirrors libsecp256k1's
/// split of the same keys into taproot keypairs (negated to even-Y before being
/// summed) and plain secret keys.
///
/// # Arguments
/// * `outpoint36` - the serialized 36-byte smallest outpoint of the transaction
/// * `input_seckeys` - `n_inputs` concatenated 32-byte input secret keys
/// * `input_is_taproot` - `n_inputs` flags, non-zero for a taproot input
/// * `n_inputs` - number of inputs being spent
/// * `scan_seckeys` - `n_recipients` concatenated 32-byte scan secret keys
/// * `spend_seckeys` - `n_recipients` concatenated 32-byte spend secret keys
/// * `recipient_is_labeled` - `n_recipients` flags, non-zero for a labeled address
/// * `recipient_labels` - `n_recipients` label integers, read where the flag is set
/// * `n_recipients` - number of recipients
///
/// # Returns
/// * the concatenated 32-byte x-only outputs in recipient order, hex-encoded;
/// * the "CREATE_FAIL" sentinel when output creation is rejected;
/// * the "INVALID_SECKEY" sentinel when a secret key is out of range;
/// * the "LABEL_FAIL" sentinel when tweaking a spend key with its label fails;
/// * skip for the known upstream divergence described on
///   [`diverges_on_intermediate_zero_sum`], or when the arguments themselves
///   are malformed.
///
/// The sentinels are failure reasons: the driver compares them against the peer
/// module, so an accept-vs-reject disagreement trips its assert.
///
/// # Safety
/// `outpoint36` must point to 36 valid bytes, `input_seckeys` to `n_inputs * 32`,
/// `input_is_taproot` to `n_inputs`, `scan_seckeys` and `spend_seckeys` to
/// `n_recipients * 32` each, and `recipient_is_labeled` and `recipient_labels`
/// to `n_recipients` elements each.
#[no_mangle]
pub unsafe extern "C" fn bdk_sp_create_outputs(
    outpoint36: *const u8,
    input_seckeys: *const u8,
    input_is_taproot: *const u8,
    n_inputs: usize,
    scan_seckeys: *const u8,
    spend_seckeys: *const u8,
    recipient_is_labeled: *const u8,
    recipient_labels: *const u32,
    n_recipients: usize,
) -> BfResult {
    if outpoint36.is_null()
        || input_seckeys.is_null()
        || input_is_taproot.is_null()
        || scan_seckeys.is_null()
        || spend_seckeys.is_null()
        || recipient_is_labeled.is_null()
        || recipient_labels.is_null()
        || n_inputs == 0
        || n_recipients == 0
    {
        return BfResult::skip();
    }

    let outpoint: [u8; 36] = match slice::from_raw_parts(outpoint36, 36).try_into() {
        Ok(arr) => arr,
        Err(_) => return BfResult::skip(),
    };
    let input_seckeys = slice::from_raw_parts(input_seckeys, n_inputs * 32);
    let is_taproot = slice::from_raw_parts(input_is_taproot, n_inputs);
    let scan_seckeys = slice::from_raw_parts(scan_seckeys, n_recipients * 32);
    let spend_seckeys = slice::from_raw_parts(spend_seckeys, n_recipients * 32);
    let is_labeled = slice::from_raw_parts(recipient_is_labeled, n_recipients);
    let labels = slice::from_raw_parts(recipient_labels, n_recipients);

    let secp = Secp256k1::new();

    let mut spks_with_keys: Vec<(ScriptBuf, SecretKey)> = Vec::with_capacity(n_inputs);
    for i in 0..n_inputs {
        let seckey = match parse_seckey(&input_seckeys[i * 32..(i + 1) * 32]) {
            Some(sk) => sk,
            None => return BfResult::fail_with(INVALID_SECKEY),
        };
        let spk = if is_taproot[i] != 0 {
            let (x_only_pubkey, _) = seckey.x_only_public_key(&secp);
            // The script only has to be recognized as P2TR: bdk_sp derives the
            // even-Y form from the secret key itself, not from the script.
            ScriptBuf::new_p2tr_tweaked(TweakedPublicKey::dangerous_assume_tweaked(x_only_pubkey))
        } else {
            let pubkey = PublicKey::from_secret_key(&secp, &seckey);
            ScriptBuf::new_p2wpkh(&WPubkeyHash::hash(&pubkey.serialize()))
        };
        spks_with_keys.push((spk, seckey));
    }

    // Every key is parsed before any protocol step runs, because libsecp256k1
    // validates all of them up front too. Interleaving the two would let one
    // library answer INVALID_SECKEY where the other answers CREATE_FAIL for the
    // same input, which the driver would report as a mismatch.
    let mut sp_codes: Vec<SilentPaymentCode> = Vec::with_capacity(n_recipients);
    for i in 0..n_recipients {
        let scan_seckey = match parse_seckey(&scan_seckeys[i * 32..(i + 1) * 32]) {
            Some(sk) => sk,
            None => return BfResult::fail_with(INVALID_SECKEY),
        };
        let spend_seckey = match parse_seckey(&spend_seckeys[i * 32..(i + 1) * 32]) {
            Some(sk) => sk,
            None => return BfResult::fail_with(INVALID_SECKEY),
        };
        let sp_code = SilentPaymentCode::new_v0(
            PublicKey::from_secret_key(&secp, &scan_seckey),
            PublicKey::from_secret_key(&secp, &spend_seckey),
            Network::Bitcoin,
        );
        // A labeled address carries B_spend + m*G in place of the spend key.
        // The sender cannot tell the difference, so all this compares is that
        // every module derives the same label tweak.
        sp_codes.push(if is_labeled[i] != 0 {
            let label = SilentPaymentCode::get_label(scan_seckey, labels[i]);
            match sp_code.add_label(label) {
                Ok(labeled) => labeled,
                Err(_) => return BfResult::fail_with(LABEL_FAIL),
            }
        } else {
            sp_code
        });
    }

    // TODO: remove with diverges_on_intermediate_zero_sum once bdk_sp stops
    // rejecting a zero intermediate input key sum. Upstream already knows about
    // it, so comparing it would only stop the target from reaching anything else.
    if diverges_on_intermediate_zero_sum(&secp, &spks_with_keys) {
        return BfResult::skip();
    }

    let partial_secret = match create_silentpayment_partial_secret(&outpoint, &spks_with_keys) {
        Ok(secret) => secret,
        Err(_) => return BfResult::fail_with("CREATE_FAIL"),
    };

    let payments = create_silentpayment_scriptpubkeys(partial_secret, &sp_codes);

    // The returned map groups the outputs per payment code, with each group in
    // recipient order. Walk the recipients in their original order and take the
    // n-th output of their code, so the response lines up with libsecp256k1's
    // recipient-ordered output array.
    let mut seen_per_code = <HashMap<&SilentPaymentCode, usize>>::new();
    let mut result = String::with_capacity(n_recipients * 64);
    for sp_code in sp_codes.iter() {
        let nth = seen_per_code.entry(sp_code).or_insert(0);
        let output = match payments.get(sp_code).and_then(|outputs| outputs.get(*nth)) {
            Some(output) => output,
            None => return BfResult::fail_with("CREATE_FAIL"),
        };
        *nth += 1;
        result.push_str(&hex::encode(output.serialize()));
    }

    BfResult::ok(result)
}

fn parse_seckey(bytes: &[u8]) -> Option<SecretKey> {
    SecretKey::from_slice(bytes).ok()
}

/// Reports the one input shape where `bdk_sp` knowingly disagrees with BIP-352,
/// so the caller can drop the response instead of letting the driver flag a
/// mismatch that is already known.
///
/// BIP-352 constrains only the total: "Let ''a = a_1 + a_2 + ... + a_n'' [...]
/// If ''a = 0'', fail". `create_silentpayment_partial_secret` folds the keys
/// with `SecretKey::add_tweak`, which rejects zero at every step, so it also
/// fails when an intermediate sum is zero and the total is not. libsecp256k1
/// only checks the total and succeeds, which is the divergence. It is reachable
/// with, for instance, one taproot input whose key is negated to even-Y and one
/// non-taproot input holding that same key.
///
/// Upstream already tracks this: bdk_sp ships the BIP-352 vector "Input keys
/// intermediate sum is zero but final sum is non-zero" marked
/// `#[ignore = "currently fails due to intermediate sum being zero"]`. Skipping
/// keeps every other input comparing bdk_sp's own key summation.
///
/// TODO: delete this function and its call site once bdk_sp sums the input keys
/// without rejecting intermediate zeros. The target then covers these inputs
/// like any other, and the skip would start hiding real regressions.
fn diverges_on_intermediate_zero_sum(
    secp: &Secp256k1<All>,
    spks_with_keys: &[(ScriptBuf, SecretKey)],
) -> bool {
    // Mirrors create_silentpayment_partial_secret's selection: taproot keys are
    // negated to their even-Y form, the other eligible script types are summed
    // as they are.
    let keys = spks_with_keys
        .iter()
        .filter_map(|(spk, sk)| {
            if spk.is_p2tr() {
                let (_, parity) = sk.x_only_public_key(secp);
                Some(if parity == Parity::Odd {
                    sk.negate()
                } else {
                    *sk
                })
            } else if spk.is_p2pkh() || spk.is_p2sh() || spk.is_p2wpkh() {
                Some(*sk)
            } else {
                None
            }
        })
        .collect::<Vec<SecretKey>>();

    let Some((first, rest)) = keys.split_first() else {
        return false;
    };

    // add_tweak rejects an in-range secret key only when the running sum reaches
    // zero, so an error here is the intermediate zero itself.
    let mut running = *first;
    let mut hit_zero = false;
    for sk in rest {
        match running.add_tweak(&Scalar::from(*sk)) {
            Ok(sum) => running = sum,
            Err(_) => {
                hit_zero = true;
                break;
            }
        }
    }
    if !hit_zero {
        return false;
    }

    // A zero total is a genuine BIP-352 failure that both libraries report, so
    // only a non-zero total diverges. Summing the public keys decides that
    // without tripping over the intermediate zero: point addition stays defined
    // for every partial sum and fails only if the total is the point at
    // infinity, which is exactly a = 0.
    let pubkeys = keys
        .iter()
        .map(|sk| sk.public_key(secp))
        .collect::<Vec<PublicKey>>();
    let pubkey_refs = pubkeys.iter().collect::<Vec<&PublicKey>>();
    PublicKey::combine_keys(&pubkey_refs).is_ok()
}
