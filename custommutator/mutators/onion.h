/**
 * Custom Mutator for Lightning Network Onion Packets
 *
 * This custom mutator is designed for fuzzing Lightning Network onion routing
 * packets. It performs structure-aware mutations by decrypting the onion
 * packet, mutating the plaintext payload, and then re-encrypting and
 * re-authenticating the packet.
 *
 * TODO: Update custom mutator to be able to create onion blinded payloads.
 *
 * ONION PACKET STRUCTURE:
 * The fuzzer input contains an onion packet with the following format:
 *   [private_key:32][version:1][ephemeral_pubkey:33][hop_payloads:1300][hmac:32]
 *
 * Total size: 1398 bytes
 *
 * Fields:
 *   - private_key (32 bytes): Bob's private key used to decrypt the packet
 *   - version (1 byte): Packet version number (typically 0)
 *   - ephemeral_pubkey (33 bytes): Compressed secp256k1 public key for ECDH
 *   - hop_payloads (1300 bytes): Encrypted routing information and payment data
 *   - hmac (32 bytes): HMAC-SHA256 authentication tag
 *
 * HOP PAYLOADS FORMAT:
 * For legacy format:
 *   - length: 1 byte, always 0x00
 *   - payload: remaining bytes
 *
 * For TLV format (BigSize encoding):
 *   - If length < 0xfd: single byte length
 *   - If length >= 0xfd: 0xfd followed by be16(length)
 *   - payload: TLV stream
 *
 * MUTATION ALGORITHM:
 *
 * 1. Key Mutation (1% probability):
 *    - Generates a new random private key for Bob
 *    - Creates a new ephemeral keypair and updates the public key in the packet
 *
 * 2. Decryption Phase:
 *    a. Extract Bob's private key and the ephemeral public key from the packet
 *    b. Compute shared secret: ECDH(bob_private_key, ephemeral_public_key)
 *    c. Derive cryptographic keys from the shared secret using HMAC-SHA256:
 *       - rho: ChaCha20 encryption key
 *       - mu: HMAC authentication key
 *    d. Generate ChaCha20 keystream:
 *       - Key: rho
 *       - Nonce: 96-bit zero nonce (0x000000000000000000000000)
 *       - Counter: 0
 *       - Output: 1300 bytes of pseudo-random stream
 *    e. Decrypt hop_payloads by XORing with the keystream
 *
 * 3. Mutation Phase:
 *    a. Standard Mutation (50% default):
 *       - Mutate the entire decrypted payload
 *       - Tests parser robustness and length validation
 *    b. Structure-Aware TLV Mutation (50% default):
 *       - Parse TLV structure from decrypted payload
 *       - Apply structure-aware mutations: modify values, add/remove records,
 *         duplicate records, change types, or generate new valid payloads
 *       - Tests TLV-specific parsing logic and semantic validation
 *
 * 4. Re-encryption Phase:
 *    a. XOR the mutated payload with the same keystream to re-encrypt
 *    b. Calculate new HMAC-SHA256 over the encrypted payload using the mu key
 *    c. Reconstruct the complete packet with all components
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <custommutator/utils/crypto/chacha20.h>
#include <custommutator/utils/crypto/hmac_sha256.h>
#include <external/secp256k1/include/secp256k1.h>
#include <external/secp256k1/include/secp256k1_ecdh.h>
#include <vector>

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size,
                                          size_t max_size, unsigned int seed);

// Default onion packet template: a valid TLV-encoded onion packet containing
// forwarding instructions. Used as the starting point for mutation-based
// fuzzing.
constexpr uint8_t default_onion[1398] = {
    0x70, 0xe9, 0x3e, 0xa1, 0x41, 0xe1, 0xfc, 0x67, 0x3e, 0x01, 0x7e, 0x97,
    0xea, 0xdc, 0x6b, 0x96, 0x8f, 0x38, 0x5c, 0x2a, 0xec, 0xb0, 0x3b, 0xfb,
    0x32, 0xaf, 0x3c, 0x54, 0xec, 0x18, 0xdb, 0x5c, 0x00, 0x03, 0x51, 0x8f,
    0xed, 0xd2, 0x78, 0x4e, 0x90, 0xea, 0xf4, 0xd4, 0x9c, 0x80, 0x8c, 0x47,
    0xb0, 0x8a, 0xa3, 0x0e, 0xcc, 0x05, 0x8c, 0x31, 0x57, 0xce, 0x72, 0xc0,
    0x66, 0x8f, 0x23, 0xa4, 0x1d, 0x7e, 0x5c, 0xc7, 0x63, 0x19, 0x3b, 0x26,
    0xf6, 0xeb, 0x83, 0x22, 0xfe, 0x75, 0xed, 0x36, 0xb8, 0xba, 0x7c, 0x8a,
    0x33, 0x39, 0xad, 0x51, 0xd5, 0xc3, 0xa8, 0x57, 0x2b, 0x84, 0xa7, 0x6c,
    0x66, 0xb3, 0x92, 0xd8, 0x45, 0x7f, 0x02, 0x31, 0xd8, 0x23, 0x78, 0x28,
    0x1f, 0x8a, 0x91, 0xf2, 0xbd, 0x1d, 0xbf, 0x1e, 0xa7, 0xab, 0xda, 0x5f,
    0x31, 0x63, 0x53, 0x8e, 0xee, 0x63, 0xc1, 0xb8, 0x8b, 0x5f, 0x67, 0x7f,
    0x22, 0xaf, 0xbe, 0x4e, 0x43, 0x3c, 0x36, 0x31, 0x02, 0x4d, 0x9e, 0x57,
    0x6c, 0x3c, 0x0d, 0x2b, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x57, 0xdb,
    0x96, 0xd6, 0xe0, 0xb7, 0x51, 0xa7, 0xd1, 0x5f, 0x01, 0x5c, 0x9d, 0x27,
    0xda, 0xeb, 0xed, 0x74, 0x63, 0xe3, 0x4c, 0x7d, 0xbc, 0xad, 0x59, 0xa8,
    0xd8, 0x47, 0x5a, 0xce, 0x6f, 0x33};

constexpr size_t KEY_SIZE = 32;
constexpr size_t PUBLIC_KEY_SIZE = 33;
constexpr size_t VERSION_SIZE = 1;
constexpr size_t HOP_PAYLOADS_SIZE = 1300;
constexpr size_t HMAC_SIZE = 32;

constexpr size_t PACKET_SIZE =
    KEY_SIZE + VERSION_SIZE + PUBLIC_KEY_SIZE + HOP_PAYLOADS_SIZE + HMAC_SIZE;

static_assert(std::size(default_onion) == PACKET_SIZE);

// Offsets within the packet structure
constexpr size_t PRIVATE_KEY_OFFSET = 0;
constexpr size_t VERSION_OFFSET = KEY_SIZE;
constexpr size_t PUBLIC_KEY_OFFSET = VERSION_OFFSET + VERSION_SIZE;
constexpr size_t HOP_PAYLOADS_OFFSET = PUBLIC_KEY_OFFSET + PUBLIC_KEY_SIZE;
constexpr size_t HMAC_OFFSET = HOP_PAYLOADS_OFFSET + HOP_PAYLOADS_SIZE;

// Key mutation probability
constexpr unsigned int KEY_MUTATION_PROBABILITY = 1; // 1% chance

// Payload mutation strategy probabilities (should sum to 100)
constexpr unsigned int STANDARD_MUTATION_PROBABILITY = 50; // 50%: Raw mutation
constexpr unsigned int TLV_STRUCTURE_MUTATION_PROBABILITY =
    50; // 50%: Structure-aware TLV

constexpr unsigned int STANDARD_THRESHOLD = STANDARD_MUTATION_PROBABILITY;
constexpr unsigned int TLV_STRUCTURE_THRESHOLD =
    STANDARD_THRESHOLD + TLV_STRUCTURE_MUTATION_PROBABILITY;

// Static assertion to ensure probabilities sum to 100
static_assert(TLV_STRUCTURE_THRESHOLD == 100,
              "Mutation probabilities must sum to 100");

// BigSize encoding constants for TLV format
constexpr uint8_t BIGSIZE_ONE_BYTE_MAX =
    0xfc; // Maximum value for single-byte encoding
constexpr uint8_t BIGSIZE_TWO_BYTES =
    0xfd; // Marker for 3-byte encoding (0xfd + be16)
constexpr uint8_t BIGSIZE_FOUR_BYTES =
    0xfe; // Marker for 5-byte encoding (0xfe + be32)
constexpr uint8_t BIGSIZE_EIGHT_BYTES =
    0xff; // Marker for 9-byte encoding (0xff + be64)
constexpr uint16_t MIN_TWO_BYTE_VALUE =
    0xfd; // Minimum value requiring 3-byte encoding
constexpr uint16_t MIN_TLV_LENGTH = 2;
constexpr uint16_t MAX_TLV_LENGTH = 1265;

static uint64_t generate_random_tlv_type() {
  uint64_t value = 0;
  uint8_t *bytes = reinterpret_cast<uint8_t *>(&value);
  for (size_t i = 0; i < sizeof(value); i++) {
    bytes[i] = static_cast<uint8_t>(rand() & 0xFF);
  }
  LLVMFuzzerMutate(bytes, sizeof(value), sizeof(value));
  return value;
}

// Mutate an existing TLV type value
static uint64_t mutate_tlv_type_value(uint64_t existing_type) {
  uint64_t value = existing_type;
  LLVMFuzzerMutate(reinterpret_cast<uint8_t *>(&value), sizeof(value),
                   sizeof(value));
  return value;
}

// Common types array for random selection
static const uint64_t COMMON_TLV_TYPES[] = {2,  4,  6,  8,         10,
                                            12, 16, 18, 5482373484};
constexpr size_t NUM_COMMON_TYPES = std::size(COMMON_TLV_TYPES);

struct TLVRecord {
  uint64_t type;
  std::vector<uint8_t> value;

  // Calculate the total encoded size of this record
  size_t encoded_size() const {
    return bigsize_encoded_size(type) + bigsize_encoded_size(value.size()) +
           value.size();
  }

  static size_t bigsize_encoded_size(uint64_t val) {
    if (val <= BIGSIZE_ONE_BYTE_MAX)
      return 1;
    if (val <= 0xFFFF)
      return 3;
    if (val <= 0xFFFFFFFF)
      return 5;
    return 9;
  }
};

struct TLVHopPayload {
  size_t total_length; // BigSize encoded length at start
  std::vector<TLVRecord> records;
  bool valid;              // True if parsing succeeded
  size_t parse_end_offset; // Offset where parsing ended

  TLVHopPayload() : total_length(0), valid(false), parse_end_offset(0) {}
};

static secp256k1_context *get_secp256k1_context() {
  static secp256k1_context *g_secp256k1_ctx = secp256k1_context_create(
      SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
  return g_secp256k1_ctx;
}

struct BigSizeResult {
  bool valid;
  uint64_t value;
  size_t bytes_read;
};

// Decode a BigSize value from a buffer
static BigSizeResult decode_bigsize(const uint8_t *data, size_t available) {
  BigSizeResult result = {false, 0, 0};

  if (available < 1)
    return result;

  uint8_t first = data[0];

  if (first <= BIGSIZE_ONE_BYTE_MAX) {
    result.valid = true;
    result.value = first;
    result.bytes_read = 1;
  } else if (first == BIGSIZE_TWO_BYTES) {
    if (available < 3)
      return result;
    result.value = (static_cast<uint64_t>(data[1]) << 8) | data[2];
    // Canonical check: must be >= 0xfd
    if (result.value >= MIN_TWO_BYTE_VALUE) {
      result.valid = true;
      result.bytes_read = 3;
    }
  } else if (first == BIGSIZE_FOUR_BYTES) {
    if (available < 5)
      return result;
    result.value = (static_cast<uint64_t>(data[1]) << 24) |
                   (static_cast<uint64_t>(data[2]) << 16) |
                   (static_cast<uint64_t>(data[3]) << 8) | data[4];
    // Canonical check: must be > 0xFFFF
    if (result.value > 0xFFFF) {
      result.valid = true;
      result.bytes_read = 5;
    }
  } else if (first == BIGSIZE_EIGHT_BYTES) {
    if (available < 9)
      return result;
    result.value = (static_cast<uint64_t>(data[1]) << 56) |
                   (static_cast<uint64_t>(data[2]) << 48) |
                   (static_cast<uint64_t>(data[3]) << 40) |
                   (static_cast<uint64_t>(data[4]) << 32) |
                   (static_cast<uint64_t>(data[5]) << 24) |
                   (static_cast<uint64_t>(data[6]) << 16) |
                   (static_cast<uint64_t>(data[7]) << 8) | data[8];
    // Canonical check: must be > 0xFFFFFFFF
    if (result.value > 0xFFFFFFFF) {
      result.valid = true;
      result.bytes_read = 9;
    }
  }

  return result;
}

// Encode a BigSize value to a buffer, returns bytes written
static size_t encode_bigsize(uint8_t *dest, uint64_t value) {
  if (value <= BIGSIZE_ONE_BYTE_MAX) {
    dest[0] = static_cast<uint8_t>(value);
    return 1;
  } else if (value <= 0xFFFF) {
    dest[0] = BIGSIZE_TWO_BYTES;
    dest[1] = (value >> 8) & 0xFF;
    dest[2] = value & 0xFF;
    return 3;
  } else if (value <= 0xFFFFFFFF) {
    dest[0] = BIGSIZE_FOUR_BYTES;
    dest[1] = (value >> 24) & 0xFF;
    dest[2] = (value >> 16) & 0xFF;
    dest[3] = (value >> 8) & 0xFF;
    dest[4] = value & 0xFF;
    return 5;
  } else {
    dest[0] = BIGSIZE_EIGHT_BYTES;
    dest[1] = (value >> 56) & 0xFF;
    dest[2] = (value >> 48) & 0xFF;
    dest[3] = (value >> 40) & 0xFF;
    dest[4] = (value >> 32) & 0xFF;
    dest[5] = (value >> 24) & 0xFF;
    dest[6] = (value >> 16) & 0xFF;
    dest[7] = (value >> 8) & 0xFF;
    dest[8] = value & 0xFF;
    return 9;
  }
}

// Parse a TLV hop payload from decrypted data
static TLVHopPayload parse_tlv_hop_payload(const uint8_t *data,
                                           size_t available) {
  TLVHopPayload payload;
  size_t offset = 0;

  if (available < 1) {
    return payload;
  }

  // First, decode the payload length
  BigSizeResult length_result = decode_bigsize(data, available);
  if (!length_result.valid) {
    return payload;
  }

  // Check for legacy format
  if (length_result.value == 0) {
    payload.valid = false;
    return payload;
  }

  payload.total_length = length_result.value;
  offset = length_result.bytes_read;

  // Calculate the end of the TLV stream
  size_t tlv_end = offset + payload.total_length;
  if (tlv_end > available) {
    // Payload extends beyond available data - still try to parse what we can
    tlv_end = available;
  }

  uint64_t last_type = 0;
  bool first_record = true;

  // Parse TLV records
  while (offset < tlv_end) {
    // Decode type
    BigSizeResult type_result = decode_bigsize(data + offset, tlv_end - offset);
    if (!type_result.valid)
      break;
    offset += type_result.bytes_read;

    // Check type ordering (must be strictly increasing)
    if (!first_record && type_result.value <= last_type) {
      // Invalid ordering, but record what we have
      break;
    }
    last_type = type_result.value;
    first_record = false;

    // Decode length
    if (offset >= tlv_end)
      break;
    BigSizeResult value_len_result =
        decode_bigsize(data + offset, tlv_end - offset);
    if (!value_len_result.valid)
      break;
    offset += value_len_result.bytes_read;

    // Check if value fits (with overflow protection)
    if (value_len_result.value > tlv_end - offset)
      break;

    // Create the record
    TLVRecord record;
    record.type = type_result.value;
    record.value.assign(data + offset, data + offset + value_len_result.value);
    payload.records.push_back(record);

    offset += value_len_result.value;
  }

  payload.valid = !payload.records.empty();
  payload.parse_end_offset = offset;

  return payload;
}

// Serialize a TLV hop payload back to bytes
static size_t serialize_tlv_hop_payload(const TLVHopPayload &payload,
                                        uint8_t *dest, size_t max_size) {
  // First, calculate total TLV stream size
  size_t tlv_stream_size = 0;
  for (const auto &record : payload.records) {
    tlv_stream_size += record.encoded_size();
  }

  // Encode the length prefix
  size_t length_prefix_size = TLVRecord::bigsize_encoded_size(tlv_stream_size);
  if (length_prefix_size > max_size)
    return 0;
  size_t offset = encode_bigsize(dest, tlv_stream_size);
  if (offset >= max_size)
    return 0;

  // Encode each record
  for (const auto &record : payload.records) {
    // Type
    size_t type_size = TLVRecord::bigsize_encoded_size(record.type);
    if (offset + type_size > max_size)
      break;
    size_t written_type_size = encode_bigsize(dest + offset, record.type);
    offset += written_type_size;

    // Length
    size_t len_size = TLVRecord::bigsize_encoded_size(record.value.size());
    if (offset + len_size > max_size)
      break;
    size_t written_len_size =
        encode_bigsize(dest + offset, record.value.size());
    offset += written_len_size;

    // Value
    size_t value_to_copy = std::min(record.value.size(), max_size - offset);
    memcpy(dest + offset, record.value.data(), value_to_copy);
    offset += value_to_copy;

    if (offset >= max_size)
      break;
  }

  return offset;
}

static std::vector<uint8_t> generate_buffer(size_t size) {
  std::vector<uint8_t> vec(size);
  for (size_t i = 0; i < vec.size(); i++) {
    vec[i] = static_cast<uint8_t>(rand() & 0xFF);
  }
  LLVMFuzzerMutate(vec.data(), vec.size(), vec.size());
  return vec;
}

// Generate a random truncated unsigned integer (tu64/tu32)
static std::vector<uint8_t> generate_tu_int(size_t max_bytes) {
  size_t actual_bytes = 1 + (rand() % max_bytes);
  std::vector<uint8_t> result = generate_buffer(actual_bytes);

  // Strip leading zeros for minimal encoding
  while (result.size() > 1 && result[0] == 0) {
    result.erase(result.begin());
  }

  return result;
}

// Generate a valid value for a known TLV type
static std::vector<uint8_t> generate_value_for_type(uint64_t type) {
  switch (type) {
  case 2: // amt_to_forward (tu64)
    return generate_tu_int(8);

  case 4: // outgoing_cltv_value (tu32)
    return generate_tu_int(4);

  case 6: // short_channel_id (8 bytes exactly)
    return generate_buffer(8);

  case 8: { // payment_data (32 byte secret + tu64 total_msat)
    std::vector<uint8_t> value = generate_buffer(32);
    auto tu64 = generate_tu_int(8);
    value.insert(value.end(), tu64.begin(), tu64.end());
    return value;
  }

  case 12: { // blinding_point (33 bytes - compressed pubkey)
    std::vector<uint8_t> value(33);
    value[0] = (rand() % 2) ? 0x02 : 0x03;
    for (size_t i = 1; i < 33; i++) {
      value[i] = static_cast<uint8_t>(rand() & 0xFF);
    }
    LLVMFuzzerMutate(value.data() + 1, 32, 32);
    return value;
  }

  case 18: // total_amount_msat (tu64)
    return generate_tu_int(8);

  default: {
    // Unknown type
    constexpr size_t max_len = 256;
    std::vector<uint8_t> value(max_len);
    for (size_t i = 0; i < max_len; i++) {
      value[i] = static_cast<uint8_t>(rand() & 0xFF);
    }
    size_t seed_size = 1 + (rand() % max_len);
    size_t new_size = LLVMFuzzerMutate(value.data(), seed_size, max_len);
    value.resize(new_size);
    return value;
  }
  }
}

// Mutation strategy: Mutate a single TLV record's value
static void mutate_tlv_value(TLVRecord &record) {
  int strategy = rand() % 4;

  switch (strategy) {
  case 0:
    // Regenerate with correct format for known types
    record.value = generate_value_for_type(record.type);
    break;
  case 1:
  case 2:
  case 3:
    size_t old_size = record.value.size();
    record.value.resize(old_size + 32);
    size_t new_size =
        LLVMFuzzerMutate(record.value.data(), old_size, record.value.size());
    record.value.resize(new_size);
    break;
  }
}

// Mutation strategy: Add a new TLV record
static void add_tlv_record(TLVHopPayload &payload) {
  TLVRecord new_record;

  // Decide between known type and random type
  if (rand() % 2 == 0) {
    // Use a known type
    new_record.type = COMMON_TLV_TYPES[rand() % NUM_COMMON_TYPES];
  } else {
    // Use a random type
    new_record.type = generate_random_tlv_type();
  }

  new_record.value = generate_value_for_type(new_record.type);

  // Insert in sorted position
  auto it = payload.records.begin();
  while (it != payload.records.end() && it->type < new_record.type) {
    ++it;
  }

  payload.records.insert(it, new_record);
}

// Mutation strategy: Remove a TLV record
static void remove_tlv_record(TLVHopPayload &payload) {
  if (payload.records.size() > 1) {
    size_t idx = rand() % payload.records.size();
    payload.records.erase(payload.records.begin() + idx);
  }
}

// Mutation strategy: Change record type to create ordering violation
static void mutate_tlv_type(TLVHopPayload &payload) {
  if (!payload.records.empty()) {
    size_t idx = rand() % payload.records.size();
    int type_mutation = rand() % 4;

    switch (type_mutation) {
    case 0:
      // Change to a known type
      payload.records[idx].type = COMMON_TLV_TYPES[rand() % NUM_COMMON_TYPES];
      break;
    case 1:
      // Mutate existing type using fuzzer
      payload.records[idx].type =
          mutate_tlv_type_value(payload.records[idx].type);
      break;
    case 2:
      // Increment type
      payload.records[idx].type++;
      break;
    case 3:
      // Decrement type
      payload.records[idx].type--;
      break;
    }
  }
}

// Main structure-aware TLV mutation function
static void mutate_tlv_structure(TLVHopPayload &payload) {
  // Number of mutations to apply
  int num_mutations = 1 + (rand() % 3);

  for (int i = 0; i < num_mutations; i++) {
    int mutation_type = rand() % 10;

    switch (mutation_type) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
      // Mutate a record's value (50%)
      if (!payload.records.empty()) {
        size_t idx = rand() % payload.records.size();
        mutate_tlv_value(payload.records[idx]);
      }
      break;

    case 5:
    case 6:
    case 7:
      // Add a new record (30%)
      add_tlv_record(payload);
      break;

    case 8:
      // Remove a record (10%)
      remove_tlv_record(payload);
      break;

    case 9:
      // Mutate type field (10%)
      mutate_tlv_type(payload);
      break;
    }
  }
}

// Generate a completely new TLV payload
static TLVHopPayload generate_new_tlv_payload() {
  TLVHopPayload payload;
  payload.valid = true;

  // Generate 2-5 records
  int num_records = 2 + (rand() % 4);

  // Start with required types for a valid forwarding payload
  TLVRecord amt_record;
  amt_record.type = 2;
  amt_record.value = generate_value_for_type(2);
  payload.records.push_back(amt_record);

  TLVRecord cltv_record;
  cltv_record.type = 4;
  cltv_record.value = generate_value_for_type(4);
  payload.records.push_back(cltv_record);

  // Optionally add more records
  for (int i = 2; i < num_records; i++) {
    add_tlv_record(payload);
  }

  // Remove duplicates
  auto last = std::unique(
      payload.records.begin(), payload.records.end(),
      [](const TLVRecord &a, const TLVRecord &b) { return a.type == b.type; });
  payload.records.erase(last, payload.records.end());

  return payload;
}

// Derives a key using HMAC-SHA256(key=keyType, message=sharedSecret)
static void derive_key(const uint8_t *shared_secret, uint8_t *derived_key,
                       const char *key_type) {
  size_t key_type_len = strlen(key_type);
  CHMAC_SHA256 hmac(reinterpret_cast<const unsigned char *>(key_type),
                    key_type_len);
  hmac.Write(shared_secret, KEY_SIZE);
  hmac.Finalize(derived_key);
}

struct DerivedKeys {
  uint8_t rho[KEY_SIZE]; // Encryption key
  uint8_t mu[KEY_SIZE];  // HMAC key
};

static void derive_all_keys(const uint8_t *shared_secret, DerivedKeys *keys) {
  derive_key(shared_secret, keys->rho, "rho");
  derive_key(shared_secret, keys->mu, "mu");
}

// Compute ECDH shared secret
static bool compute_shared_secret(const uint8_t *private_key,
                                  const uint8_t *public_key_bytes,
                                  uint8_t *shared_secret) {
  secp256k1_context *ctx = get_secp256k1_context();
  if (!ctx) {
    return false;
  }

  secp256k1_pubkey public_key;
  if (!secp256k1_ec_pubkey_parse(ctx, &public_key, public_key_bytes,
                                 PUBLIC_KEY_SIZE)) {
    return false;
  }

  if (!secp256k1_ecdh(ctx, shared_secret, &public_key, private_key, nullptr,
                      nullptr)) {
    return false;
  }

  return true;
}

// Generate ChaCha20 keystream for encryption/decryption
static void generate_keystream(const uint8_t *rho_key, uint8_t *keystream) {
  std::span<const std::byte> key_span(
      reinterpret_cast<const std::byte *>(rho_key), KEY_SIZE);

  ChaCha20 chacha(key_span);
  ChaCha20::Nonce96 nonce = {0, 0};
  chacha.Seek(nonce, 0);

  std::span<std::byte> keystream_span(reinterpret_cast<std::byte *>(keystream),
                                      HOP_PAYLOADS_SIZE);
  chacha.Keystream(keystream_span);
}

// XOR operation for encryption/decryption
static void xor_buffers(uint8_t *dest, const uint8_t *src, const uint8_t *key,
                        size_t length) {
  for (size_t i = 0; i < length; i++) {
    dest[i] = src[i] ^ key[i];
  }
}

// Calculate HMAC over the payload
static void calculate_hmac(const uint8_t *mu_key, const uint8_t *payload,
                           size_t payload_size, uint8_t *hmac_output) {
  CHMAC_SHA256 hmac(mu_key, KEY_SIZE);
  hmac.Write(payload, payload_size);
  hmac.Finalize(hmac_output);
}

static size_t build_template_packet(uint8_t *fuzz_data, size_t max_size) {
  if (max_size < PACKET_SIZE) {
    return 0;
  }
  memcpy(fuzz_data, default_onion, PACKET_SIZE);
  return PACKET_SIZE;
}

size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
                               unsigned int seed) {

  // Generate template packet if input is too small
  if (size < PACKET_SIZE) {
    return build_template_packet(fuzz_data, max_size);
  }

  srand(seed);

  // Optionally mutate keys
  bool should_mutate_keys =
      static_cast<unsigned int>(rand() % 100) < KEY_MUTATION_PROBABILITY;
  if (should_mutate_keys) {
    // Mutate the private key
    LLVMFuzzerMutate(fuzz_data + PRIVATE_KEY_OFFSET, KEY_SIZE, KEY_SIZE);
    // Mutate the ephemeral public key
    LLVMFuzzerMutate(fuzz_data + PUBLIC_KEY_OFFSET, PUBLIC_KEY_SIZE,
                     PUBLIC_KEY_SIZE);
  }

  // Step 1: Compute shared secret via ECDH
  uint8_t shared_secret[KEY_SIZE];
  if (!compute_shared_secret(fuzz_data + PRIVATE_KEY_OFFSET,
                             fuzz_data + PUBLIC_KEY_OFFSET, shared_secret)) {
    return size; // Cannot proceed without valid shared secret
  }

  // Step 2: Derive encryption and authentication keys
  DerivedKeys keys;
  derive_all_keys(shared_secret, &keys);

  // Step 3: Generate ChaCha20 keystream for decryption
  uint8_t keystream[HOP_PAYLOADS_SIZE];
  generate_keystream(keys.rho, keystream);

  // Step 4: Decrypt the hop payloads
  uint8_t decrypted_payload[HOP_PAYLOADS_SIZE] = {0};

  xor_buffers(decrypted_payload, fuzz_data + HOP_PAYLOADS_OFFSET, keystream,
              HOP_PAYLOADS_SIZE);

  // Step 5: Choose mutation strategy
  unsigned int mutation_choice = rand() % 100;

  if (mutation_choice < STANDARD_THRESHOLD) {
    // Strategy 1: Standard mutation - mutate entire payload (50%)
    LLVMFuzzerMutate(decrypted_payload, HOP_PAYLOADS_SIZE, HOP_PAYLOADS_SIZE);
  } else {
    // Strategy 2: Structure-aware TLV mutation (50%)
    TLVHopPayload parsed =
        parse_tlv_hop_payload(decrypted_payload, HOP_PAYLOADS_SIZE);

    if (parsed.valid && !parsed.records.empty()) {
      // Mutate the parsed structure
      mutate_tlv_structure(parsed);

      // Serialize back to buffer
      size_t serialized_size = serialize_tlv_hop_payload(
          parsed, decrypted_payload, HOP_PAYLOADS_SIZE);

      // Zero-pad remaining space
      if (serialized_size < HOP_PAYLOADS_SIZE) {
        memset(decrypted_payload + serialized_size, 0,
               HOP_PAYLOADS_SIZE - serialized_size);
      }
    } else {
      // Parsing failed - generate a new valid TLV payload
      TLVHopPayload new_payload = generate_new_tlv_payload();
      size_t serialized_size = serialize_tlv_hop_payload(
          new_payload, decrypted_payload, HOP_PAYLOADS_SIZE);

      if (serialized_size < HOP_PAYLOADS_SIZE) {
        memset(decrypted_payload + serialized_size, 0,
               HOP_PAYLOADS_SIZE - serialized_size);
      }
    }
  }

  // Step 6: Re-encrypt the mutated payload
  uint8_t encrypted_payload[HOP_PAYLOADS_SIZE] = {0};
  xor_buffers(encrypted_payload, decrypted_payload, keystream,
              HOP_PAYLOADS_SIZE);

  // Step 7: Calculate HMAC over the encrypted payload
  uint8_t hmac[HMAC_SIZE];
  calculate_hmac(keys.mu, encrypted_payload, HOP_PAYLOADS_SIZE, hmac);

  // Step 8: Reconstruct the complete packet
  memcpy(fuzz_data + HOP_PAYLOADS_OFFSET, encrypted_payload, HOP_PAYLOADS_SIZE);
  memcpy(fuzz_data + HMAC_OFFSET, hmac, HMAC_SIZE);
  return PACKET_SIZE;
}