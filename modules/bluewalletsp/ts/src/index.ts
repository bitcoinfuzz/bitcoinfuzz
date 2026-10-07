import * as readline from "readline";
import { bech32m } from "bech32";
import { ECPairFactory } from "ecpair";
import * as ecc from "tiny-secp256k1";
import { SilentPayment } from "silent-payments";

const ECPair = ECPairFactory(ecc);

// The reasons the other silentpayments_create_outputs modules fail with, so the
// driver compares rejections instead of skipping them.
const INVALID_SECKEY = "INVALID_SECKEY";
const CREATE_FAIL = "CREATE_FAIL";
// Reported when a request cannot be parsed, which is a bug in the C++ wrapper
// rather than a verdict on the input. It carries no status, so the wrapper
// aborts on it.
const BAD_REQUEST = "BAD_REQUEST";

const SECKEY_LEN = 32;
const PUBKEY_LEN = 33;
const OUTPOINT_LEN = 36;
const CURVE_ORDER = BigInt(
    "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141",
);
// Wide enough for a v0 silent payment code: 1 version word + 66 payload bytes.
const SP_BECH32_LIMIT = 118;

// Mirror BfResult (include/bitcoinfuzz/ffi.h): each response line starts with
// its status, which the C++ wrapper maps to Ok, Fail or Skip.
function ok(value: string): string {
    return "OK " + value;
}

// The driver compares the reason, so it must match what the target's other
// modules return on rejection.
function fail(reason: string): string {
    return "FAIL " + reason;
}

function skip(): string {
    return "SKIP";
}

function toHex(bytes: Uint8Array): string {
    let hex = "";
    for (const byte of bytes) {
        hex += byte.toString(16).padStart(2, "0");
    }
    return hex;
}

function fromHex(hex: string): Uint8Array | null {
    if (hex.length % 2 !== 0 || /[^0-9a-f]/.test(hex)) {
        return null;
    }
    const bytes = new Uint8Array(hex.length / 2);
    for (let i = 0; i < bytes.length; i++) {
        bytes[i] = parseInt(hex.slice(i * 2, i * 2 + 2), 16);
    }
    return bytes;
}

/**
 * Derives the compressed public key, or null if the scalar is out of range.
 *
 * tiny-secp256k1 throws for an out-of-range scalar rather than returning null,
 * and reserves null for a result that is the point at infinity. The fuzzer
 * reaches both, so every call goes through here: an escaping exception would
 * kill the runner instead of being compared as a rejection.
 */
function pubkeyFromSeckey(seckey: Uint8Array): Uint8Array | null {
    try {
        return ecc.pointFromScalar(seckey, true);
    } catch (_) {
        return null;
    }
}

/**
 * Encodes a scan/spend public key pair as the `sp1...` code the library takes
 * as a target address, which is also the only way to hand it a recipient.
 */
function encodeSilentPaymentCode(
    scanPubkey: Uint8Array,
    spendPubkey: Uint8Array,
): string {
    const payload = new Uint8Array(PUBKEY_LEN * 2);
    payload.set(scanPubkey, 0);
    payload.set(spendPubkey, PUBKEY_LEN);
    const words = bech32m.toWords(payload);
    words.unshift(0); // version
    return bech32m.encode("sp", words, SP_BECH32_LIMIT);
}

/**
 * Reports the one input shape where the library disagrees with BIP-352, so the
 * caller can skip it instead of letting the driver flag a mismatch that is
 * already understood.
 *
 * BIP-352 constrains only the total: "Let ''a = a_1 + a_2 + ... + a_n'' [...]
 * If ''a = 0'', fail". `_sumPrivkeys` folds the keys with `ecc.privateAdd`,
 * which yields null once a running sum reaches zero, and the fold then breaks
 * on the empty result, so it also fails when an intermediate sum is zero and
 * the total is not. libsecp256k1 checks only the total and succeeds.
 *
 * Reachable with, for instance, one taproot input whose key is negated to
 * even-Y plus one non-taproot input holding that same key. bdk_sp has the same
 * gap, tracked upstream; this one has not been reported yet.
 *
 * TODO: delete this and its call site once the library sums the input keys
 * without rejecting intermediate zeros, otherwise the skip starts hiding real
 * regressions.
 */
function divergesOnIntermediateZeroSum(
    seckeys: Uint8Array[],
    isTaproot: Uint8Array,
): boolean {
    // Same selection as _sumPrivkeys: taproot keys move to their even-Y form,
    // everything else is summed as it is.
    const keys: Uint8Array[] = [];
    for (let i = 0; i < seckeys.length; i++) {
        const seckey = seckeys[i] as Uint8Array;
        const pubkey = pubkeyFromSeckey(seckey);
        if (pubkey === null) {
            return false;
        }
        const odd = (pubkey[0] as number) === 0x03;
        keys.push(
            isTaproot[i] !== 0 && odd ? ecc.privateNegate(seckey) : seckey,
        );
    }

    // Summed as scalars rather than through ecc: the library's own fold stops at
    // the first zero, which is exactly the state that has to be measured past.
    const scalars = keys.map((key) => BigInt("0x" + toHex(key)));
    const total = scalars.reduce(
        (sum, scalar) => (sum + scalar) % CURVE_ORDER,
        BigInt(0),
    );

    let running = scalars[0] as bigint;
    let hitZero = false;
    for (let i = 1; i < scalars.length; i++) {
        running = (running + (scalars[i] as bigint)) % CURVE_ORDER;
        if (running === BigInt(0)) {
            hitZero = true;
            break;
        }
    }

    // A zero total is a real BIP-352 failure that every module reports, so only
    // a non-zero total is a divergence. That also covers the zero landing on the
    // final addition, where the two agree.
    return hitZero && total !== BigInt(0);
}

/**
 * Creates the BIP-352 Silent Payments outputs for a set of recipients, matching
 * the response format of the secp256k1 and bdksp modules so the driver can
 * compare the three.
 *
 * The library is transaction shaped: it takes WIF secret keys, txid/vout pairs
 * and `sp1...` target addresses, and hands back taproot addresses. Every input
 * is given the same outpoint so that the smallest outpoint the library picks is
 * the one the target chose, and the resulting addresses are decoded back to
 * x-only keys.
 *
 * @returns ok with the concatenated 32-byte x-only outputs in recipient order
 * as hex, fail with INVALID_SECKEY or CREATE_FAIL, or skip for the known
 * divergence described on divergesOnIntermediateZeroSum.
 */
function createOutputs(
    outpoint: Uint8Array,
    inputSeckeys: Uint8Array,
    inputIsTaproot: Uint8Array,
    scanSeckeys: Uint8Array,
    spendSeckeys: Uint8Array,
): string {
    const numInputs = inputIsTaproot.length;
    const numRecipients = scanSeckeys.length / SECKEY_LEN;

    // The txid is stored in reverse of its display order and the vout is
    // little-endian, which is how the library re-serializes them internally.
    const txid = toHex(outpoint.slice(0, 32).reverse());
    const voutBytes = outpoint.slice(32, OUTPOINT_LEN);
    const vout =
        ((voutBytes[0] as number) |
            ((voutBytes[1] as number) << 8) |
            ((voutBytes[2] as number) << 16) |
            ((voutBytes[3] as number) << 24)) >>>
        0;

    const utxos = [];
    const seckeys: Uint8Array[] = [];
    for (let i = 0; i < numInputs; i++) {
        const seckey = inputSeckeys.slice(i * SECKEY_LEN, (i + 1) * SECKEY_LEN);
        let wif;
        try {
            wif = ECPair.fromPrivateKey(seckey, { compressed: true }).toWIF();
        } catch (_) {
            return fail(INVALID_SECKEY);
        }
        seckeys.push(seckey);
        utxos.push({
            txid,
            // All inputs share one outpoint, so it is also the smallest.
            vout,
            wif,
            utxoType: (inputIsTaproot[i] !== 0 ? "p2tr" : "p2wpkh") as
                "p2tr" | "p2wpkh",
        });
    }

    const targets = [];
    for (let i = 0; i < numRecipients; i++) {
        const scanSeckey = scanSeckeys.slice(
            i * SECKEY_LEN,
            (i + 1) * SECKEY_LEN,
        );
        const spendSeckey = spendSeckeys.slice(
            i * SECKEY_LEN,
            (i + 1) * SECKEY_LEN,
        );
        const scanPubkey = pubkeyFromSeckey(scanSeckey);
        const spendPubkey = pubkeyFromSeckey(spendSeckey);
        if (scanPubkey === null || spendPubkey === null) {
            return fail(INVALID_SECKEY);
        }
        targets.push({
            address: encodeSilentPaymentCode(scanPubkey, spendPubkey),
        });
    }

    if (divergesOnIntermediateZeroSum(seckeys, inputIsTaproot)) {
        return skip();
    }

    let created;
    try {
        created = new SilentPayment().createTransaction(utxos, targets);
    } catch (_) {
        return fail(CREATE_FAIL);
    }

    // createTransaction writes each result back at its recipient's index, so the
    // response already lines up with the other modules' recipient ordering.
    let result = "";
    for (const target of created) {
        const address = target?.address;
        if (address === undefined) {
            return fail(CREATE_FAIL);
        }
        try {
            result += SilentPayment.addressToPubkey(address);
        } catch (_) {
            return fail(CREATE_FAIL);
        }
    }
    return ok(result);
}

/**
 * Handles one request line: five space separated lowercase hex fields, in the
 * order outpoint, input secret keys, taproot flags, scan keys, spend keys.
 */
function handleRequest(line: string): string {
    const fields = line.split(" ");
    if (fields.length !== 5) {
        return BAD_REQUEST;
    }
    const decoded: Uint8Array[] = [];
    for (const field of fields) {
        const bytes = fromHex(field);
        if (bytes === null) {
            return BAD_REQUEST;
        }
        decoded.push(bytes);
    }
    const outpoint = decoded[0] as Uint8Array;
    const inputSeckeys = decoded[1] as Uint8Array;
    const inputIsTaproot = decoded[2] as Uint8Array;
    const scanSeckeys = decoded[3] as Uint8Array;
    const spendSeckeys = decoded[4] as Uint8Array;

    const numInputs = inputIsTaproot.length;
    const numRecipients = scanSeckeys.length / SECKEY_LEN;
    if (
        outpoint.length !== OUTPOINT_LEN ||
        numInputs === 0 ||
        numRecipients === 0 ||
        inputSeckeys.length !== numInputs * SECKEY_LEN ||
        scanSeckeys.length % SECKEY_LEN !== 0 ||
        spendSeckeys.length !== numRecipients * SECKEY_LEN
    ) {
        return BAD_REQUEST;
    }

    return createOutputs(
        outpoint,
        inputSeckeys,
        inputIsTaproot,
        scanSeckeys,
        spendSeckeys,
    );
}

// One request per line, one response per line, in order. The wrapper keeps a
// single runner alive for the whole fuzzing session, so process startup and the
// WASM instantiation in tiny-secp256k1 are paid once rather than per input.
const rl = readline.createInterface({ input: process.stdin });
rl.on("line", (line: string) => {
    process.stdout.write(handleRequest(line) + "\n");
});
