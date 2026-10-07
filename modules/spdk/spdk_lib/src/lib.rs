use silentpayments::{
    receiving::{Label, Receiver},
    secp256k1::{All, Parity, PublicKey, Scalar, Secp256k1, SecretKey},
    sending::generate_recipient_pubkeys,
    utils::{sending::calculate_partial_secret, OutPoint},
    Network, SilentPaymentAddress, SpVersion,
};
use std::collections::HashMap;
use std::slice;

#[path = "../../../../include/bitcoinfuzz/ffi.rs"]
mod ffi;
use ffi::BfResult;

/// Kept byte-for-byte in sync with the peer modules' sentinels: the driver
/// compares the responses verbatim, so a different spelling would read as a
/// mismatch instead of as agreement on rejecting the input.
const INVALID_SECKEY: &str = "INVALID_SECKEY";
const CREATE_FAIL: &str = "CREATE_FAIL";

/// Returned when tweaking a spend key with its label is rejected, which needs a
/// label hash outside the curve order or a label that negates the spend key.
/// Kept distinct from CREATE_FAIL so a disagreement points at the step.
const LABEL_FAIL: &str = "LABEL_FAIL";

const SECKEY_LEN: usize = 32;
const OUTPOINT_LEN: usize = 36;

/// Creates the BIP-352 Silent Payments outputs for a set of recipients, mirroring
/// the `secp256k1` module's `secp256k1_silentpayments_create_outputs` so the C++
/// driver can compare the two byte-for-byte.
///
/// `silentpayments` takes the eligible input keys directly as `(SecretKey,
/// bool)` pairs rather than deriving eligibility from script pubkeys, so the
/// target's taproot flag maps straight onto the boolean. Taproot keys are
/// negated to their even-Y form before summing, matching libsecp256k1's split of
/// the same keys into keypairs and plain secret keys.
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
/// modules, so an accept-vs-reject disagreement trips its assert.
///
/// # Safety
/// `outpoint36` must point to 36 valid bytes, `input_seckeys` to `n_inputs * 32`,
/// `input_is_taproot` to `n_inputs`, `scan_seckeys` and `spend_seckeys` to
/// `n_recipients * 32` each, and `recipient_is_labeled` and `recipient_labels`
/// to `n_recipients` elements each.
#[no_mangle]
pub unsafe extern "C" fn spdk_create_outputs(
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

    let outpoint: [u8; OUTPOINT_LEN] =
        match slice::from_raw_parts(outpoint36, OUTPOINT_LEN).try_into() {
            Ok(arr) => arr,
            Err(_) => return BfResult::skip(),
        };
    let input_seckeys = slice::from_raw_parts(input_seckeys, n_inputs * SECKEY_LEN);
    let is_taproot = slice::from_raw_parts(input_is_taproot, n_inputs);
    let scan_seckeys = slice::from_raw_parts(scan_seckeys, n_recipients * SECKEY_LEN);
    let spend_seckeys = slice::from_raw_parts(spend_seckeys, n_recipients * SECKEY_LEN);
    let is_labeled = slice::from_raw_parts(recipient_is_labeled, n_recipients);
    let labels = slice::from_raw_parts(recipient_labels, n_recipients);

    let secp = Secp256k1::new();

    let mut input_keys: Vec<(SecretKey, bool)> = Vec::with_capacity(n_inputs);
    for i in 0..n_inputs {
        let seckey = match parse_seckey(&input_seckeys[i * SECKEY_LEN..(i + 1) * SECKEY_LEN]) {
            Some(sk) => sk,
            None => return BfResult::fail_with(INVALID_SECKEY),
        };
        input_keys.push((seckey, is_taproot[i] != 0));
    }

    // Every key is parsed before any protocol step runs, because libsecp256k1
    // validates all of them up front too. Interleaving the two would let one
    // library answer INVALID_SECKEY where the other answers CREATE_FAIL for the
    // same input, which the driver would report as a mismatch.
    let mut addresses: Vec<SilentPaymentAddress> = Vec::with_capacity(n_recipients);
    for i in 0..n_recipients {
        let scan_seckey = match parse_seckey(&scan_seckeys[i * SECKEY_LEN..(i + 1) * SECKEY_LEN]) {
            Some(sk) => sk,
            None => return BfResult::fail_with(INVALID_SECKEY),
        };
        let spend_seckey = match parse_seckey(&spend_seckeys[i * SECKEY_LEN..(i + 1) * SECKEY_LEN])
        {
            Some(sk) => sk,
            None => return BfResult::fail_with(INVALID_SECKEY),
        };
        let scan_pubkey = PublicKey::from_secret_key(&secp, &scan_seckey);
        let spend_pubkey = PublicKey::from_secret_key(&secp, &spend_seckey);
        // A labeled address carries B_spend + m*G in place of the spend key.
        // The sender cannot tell the difference, so all this compares is that
        // every module derives the same label tweak.
        addresses.push(if is_labeled[i] != 0 {
            match labeled_address(scan_seckey, scan_pubkey, spend_pubkey, labels[i]) {
                Some(address) => address,
                None => return BfResult::fail_with(LABEL_FAIL),
            }
        } else {
            SilentPaymentAddress::new_v0(scan_pubkey, spend_pubkey)
        });
    }

    // TODO: remove with diverges_on_intermediate_zero_sum once silentpayments
    // stops rejecting a zero intermediate input key sum. Comparing it would only
    // stop the target from reaching anything else.
    if diverges_on_intermediate_zero_sum(&secp, &input_keys) {
        return BfResult::skip();
    }

    // A single outpoint is passed because the target already picks the smallest
    // one; `calculate_partial_secret` would otherwise select it itself.
    let outpoints = [OutPoint::from_bytes(outpoint)];
    let partial_secret = match calculate_partial_secret(&input_keys, &outpoints) {
        Ok(secret) => secret,
        Err(_) => return BfResult::fail_with(CREATE_FAIL),
    };

    let payments = match generate_recipient_pubkeys(addresses.clone(), partial_secret) {
        Ok(payments) => payments,
        Err(_) => return BfResult::fail_with(CREATE_FAIL),
    };

    // The returned map groups the outputs per address, with each group in
    // recipient order. Walk the recipients in their original order and take the
    // n-th output of their address, so the response lines up with libsecp256k1's
    // recipient-ordered output array.
    let mut seen_per_address = <HashMap<&SilentPaymentAddress, usize>>::new();
    let mut result = String::with_capacity(n_recipients * SECKEY_LEN * 2);
    for address in addresses.iter() {
        let nth = seen_per_address.entry(address).or_insert(0);
        let output = match payments.get(address).and_then(|outputs| outputs.get(*nth)) {
            Some(output) => output,
            None => return BfResult::fail_with(CREATE_FAIL),
        };
        *nth += 1;
        result.push_str(&hex::encode(output.serialize()));
    }

    BfResult::ok(result)
}

fn parse_seckey(bytes: &[u8]) -> Option<SecretKey> {
    SecretKey::from_slice(bytes).ok()
}

/// Derives the address a recipient hands out for label `m`.
///
/// The crate keeps labels on the receiving side, behind a `Receiver` that owns
/// its label set, so the address has to be taken through that type rather than
/// built from the tweak directly. `Receiver::new` insists on a change label, so
/// m = 0 is registered first even when the wanted label is another one; both
/// steps only fail on a label hash outside the curve order or one that negates
/// the spend key.
fn labeled_address(
    scan_seckey: SecretKey,
    scan_pubkey: PublicKey,
    spend_pubkey: PublicKey,
    m: u32,
) -> Option<SilentPaymentAddress> {
    let mut receiver = Receiver::new(
        SpVersion::ZERO,
        scan_pubkey,
        spend_pubkey,
        Label::new(scan_seckey, 0),
        Network::Mainnet,
    )
    .ok()?;
    let label = Label::new(scan_seckey, m);
    receiver.add_label(label.clone()).ok()?;
    receiver.get_receiving_address_for_label(&label).ok()
}

/// Reports the one input shape where `silentpayments` disagrees with BIP-352, so
/// the caller can drop the response instead of letting the driver flag a
/// mismatch that is already understood.
///
/// BIP-352 constrains only the total: "Let ''a = a_1 + a_2 + ... + a_n'' [...]
/// If ''a = 0'', fail". `get_a_sum_secret_keys` folds the keys with
/// `SecretKey::add_tweak`, which rejects zero at every step, so it also fails
/// when an intermediate sum is zero and the total is not. libsecp256k1 only
/// checks the total and succeeds, which is the divergence. It is reachable with,
/// for instance, one taproot input whose key is negated to even-Y and one
/// non-taproot input holding that same key.
///
/// bdk_sp has the same gap and tracks it with an ignored BIP-352 vector; this
/// one has not been reported upstream yet. Skipping keeps every other input
/// comparing this library's own key summation.
///
/// TODO: delete this function and its call site once `silentpayments` sums the
/// input keys without rejecting intermediate zeros. The target then covers these
/// inputs like any other, and the skip would start hiding real regressions.
fn diverges_on_intermediate_zero_sum(
    secp: &Secp256k1<All>,
    input_keys: &[(SecretKey, bool)],
) -> bool {
    // Mirrors get_a_sum_secret_keys: taproot keys are negated to their even-Y
    // form, everything else is summed as it is.
    let keys = input_keys
        .iter()
        .map(|(sk, is_taproot)| {
            let (_, parity) = sk.x_only_public_key(secp);
            if *is_taproot && parity == Parity::Odd {
                sk.negate()
            } else {
                *sk
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

    // A zero total is a genuine BIP-352 failure that every module reports, so
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
