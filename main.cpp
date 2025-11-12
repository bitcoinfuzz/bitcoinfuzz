#include <algorithm>
#include <memory>
#include <cstring>
#include "driver.h"
#include <bitcoinfuzz/basemodule.h>

#ifdef BITCOIN_CORE
#include <modules/bitcoin/module.h>
#endif

#ifdef RUST_BITCOIN
#include <modules/rustbitcoin/module.h>
#endif

#ifdef RUST_MINISCRIPT
#include <modules/rustminiscript/module.h>
#endif

#ifdef BTCD
#include <modules/btcd/module.h>
#endif

#ifdef NBITCOIN
#include <modules/nbitcoin/module.h>
#endif

#ifdef LND
#include <modules/lnd/module.h>
#endif

#ifdef LDK
#include <modules/ldk/module.h>
#endif

#ifdef ECLAIR
#include <modules/eclair/module.h>
#endif

#ifdef NLIGHTNING
#include <modules/nlightning/module.h>
#endif

#ifdef EMBIT
#include <modules/embit/module.h>
#endif

#ifdef CLIGHTNING
#include <modules/clightning/module.h>
#endif

#ifdef LIGHTNING_KMP
#include <modules/lightningkmp/module.h>
#endif

#if defined(CUSTOM_MUTATOR_BOLT11) || defined(CUSTOM_MUTATOR_BOLT12_OFFER)
#include <modules/custommutator/bech32.h>
#include <modules/custommutator/customcrossover.h>
#endif

std::shared_ptr<bitcoinfuzz::Driver> driver = nullptr;

static bitcoinfuzz::ModuleLogger module_logger;

#ifdef CUSTOM_MUTATOR_BOLT12_OFFER
constexpr std::string_view bech32_hrp = "lno";
constexpr std::string_view dummy_initial_input = "lno1";

// A custom mutator that decodes the bech32 input, mutates the decoded input,
// and then re-encodes the mutated input. This produces an input corpus that
// consists entirely of correctly encoded bech32 strings, enabling efficient
// fuzzing of the bolt12 decoding logic without the fuzzer getting stuck on
// fuzzing the bech32 decoding logic. This custom mutator is originally from
// core-lightning:
// https://github.com/ElementsProject/lightning/blob/release-v25.02.2/tests/fuzz/bolt12.h#L54
extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
                                          unsigned int seed);
extern "C" size_t LLVMFuzzerCustomCrossOver(const uint8_t *in1, size_t in1_size, const uint8_t *in2,
                                            size_t in2_size, uint8_t *out, size_t max_out_size,
                                            unsigned seed);

/* Encodes a dummy bolt12 offer/invoice-request/invoice into fuzz_data and
 * returns the size of the encoded string. */
static size_t initial_input(uint8_t *fuzz_data, size_t max_size)
{
    size_t output_size = std::min(max_size, dummy_initial_input.size());
    std::memcpy(fuzz_data, dummy_initial_input.data(), output_size);

    return output_size;
}

/* A custom mutator that decodes the bech32 input, mutates the decoded input,
 * and then re-encodes the mutated input. This produces an input corpus that
 * consists entirely of correctly encoded bech32 strings, enabling efficient
 * fuzzing of the bolt12 decoding logic without the fuzzer getting stuck on
 * fuzzing the bech32 decoding logic. */
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size,
                                          size_t max_size, unsigned int seed)
{
    std::string_view input_str(reinterpret_cast<char *>(fuzz_data), size);

    // Decode the input
    bech32::DecodeResult decoded_result = bech32::DecodeNoChecksum(input_str, bech32::CharLimit::CUSTOM_MUTATOR);
    if (decoded_result.encoding != bech32::Encoding::BECH32) {
        // Decoding failed, this should only happen when starting from
        // an empty corpus.
        return initial_input(fuzz_data, max_size);
    }
    std::vector<uint8_t> &decoded_data = decoded_result.data;

    if (decoded_data.size() > max_size) {
        return initial_input(fuzz_data, max_size);
    }

    // Resize the vector to the maximum possible size BEFORE mutation.
    size_t decoded_data_size = decoded_data.size();
    decoded_data.resize(max_size);

    size_t mutated_size = LLVMFuzzerMutate(decoded_data.data(),
                                           decoded_data_size,
                                           max_size);
    decoded_data.resize(mutated_size);

    // It ensures that all values remain valid 5-bit values
    for (uint8_t& val : decoded_data) {
        val &= 0x1F;
    }

    // Encode the mutated input
    std::string encoded_data = bech32::EncodeNoChecksum(bech32_hrp, decoded_data);

    if (encoded_data.length() > max_size) {
        return initial_input(fuzz_data, max_size);
    }

    std::memcpy(fuzz_data, encoded_data.data(), encoded_data.length());

    return encoded_data.length();
}

/* A custom cross-over mutator that decodes the bech32 inputs before cross-over
 * mutating them. Like LLVMFuzzerCustomMutator, this enables more efficient
 * fuzzing of bolt12 offers, invoice requests, and invoices. */
extern "C" size_t LLVMFuzzerCustomCrossOver(const uint8_t *in1, size_t in1_size,
                                            const uint8_t *in2, size_t in2_size,
                                            uint8_t *out, size_t max_out_size,
                                            unsigned seed)
{
    // Decode first input
    std::string_view input1_str(reinterpret_cast<const char*>(in1), in1_size);
    bech32::DecodeResult decoded_result1 = bech32::DecodeNoChecksum(input1_str, bech32::CharLimit::CUSTOM_MUTATOR);

    // Decode second input
    std::string_view input2_str(reinterpret_cast<const char*>(in2), in2_size);
    bech32::DecodeResult decoded_result2 = bech32::DecodeNoChecksum(input2_str, bech32::CharLimit::CUSTOM_MUTATOR);

    // Perform cross-over on decoded data
    std::vector<uint8_t> crossed_data = cross_over(decoded_result1.data, decoded_result2.data,
                                                    max_out_size, seed);

    // Encode the crossed-over data
    std::string encoded_data = bech32::EncodeNoChecksum(bech32_hrp, crossed_data);

    if (encoded_data.length() > max_out_size) {
        return 0;
    }

    std::memcpy(out, encoded_data.data(), encoded_data.length());
    return encoded_data.length();
}
#endif


#ifdef CUSTOM_MUTATOR_P2P_MESSAGE
// Custom mutator for Bitcoin P2P messages that maintains valid checksums.
// This enables efficient fuzzing of Bitcoin message parsing logic without
// the fuzzer getting stuck on checksum validation failures.
//
// This custom mutator does the following:
//   1. Mutate the input using libFuzzer's default mutator `LLVMFuzzerMutate`
//   2. If the data is large enough to contain a header, recalculate and fix the checksum
//   3. Return the mutated data with valid checksum

#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <modules/custommutator/crypto/sha256.h>

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
                                         unsigned int seed);

// Bitcoin message constants
static constexpr size_t BITCOIN_HEADER_SIZE = 24;
static constexpr size_t PAYLOAD_LENGTH_OFFSET = 16;
static constexpr size_t CHECKSUM_OFFSET = 20;

// Bitcoin double SHA256 using Bitcoin Core's CSHA256 class
static void bitcoin_double_sha256(const uint8_t* data, size_t len, uint8_t* hash) {
    CSHA256 hasher;
    hasher.Write(data, len);
    unsigned char first_hash[CSHA256::OUTPUT_SIZE];
    hasher.Finalize(first_hash);

    hasher.Reset();
    hasher.Write(first_hash, CSHA256::OUTPUT_SIZE);
    hasher.Finalize(hash);
}

// Fix the checksum in a Bitcoin P2P message
static void fix_bitcoin_checksum(uint8_t* data, size_t size) {
    if (size < BITCOIN_HEADER_SIZE) {
        return;
    }

    uint32_t payload_length;
    memcpy(&payload_length, data + PAYLOAD_LENGTH_OFFSET, 4);

    // Validate that we have the complete message
    if (size < BITCOIN_HEADER_SIZE + payload_length) {
        return;
    }

    // Calculate checksum for the payload
    const uint8_t* payload = data + BITCOIN_HEADER_SIZE;
    uint8_t calculated_checksum[32];
    bitcoin_double_sha256(payload, payload_length, calculated_checksum);

    // Update checksum in header (first 4 bytes of hash)
    memcpy(data + CHECKSUM_OFFSET, calculated_checksum, 4);
}

size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
                               unsigned int seed) {
    // First, mutate the data using LibFuzzer's default mutator
    size_t new_size = LLVMFuzzerMutate(fuzz_data, size, max_size);

    // Then fix the checksum if the data looks like it could be a Bitcoin message
    fix_bitcoin_checksum(fuzz_data, new_size);

    return new_size;
}
#endif

#ifdef CUSTOM_MUTATOR_ONION_LEGACY

/**
 * Custom Mutator for Lightning Network Legacy Onion Packets
 * 
 * This custom mutator is designed for fuzzing legacy Lightning Network onion routing packets.
 * It performs structure-aware mutations by decrypting the onion packet, mutating the
 * plaintext payload, and then re-encrypting and re-authenticating the packet.
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
 *       - um: Error reporting key (unused in this implementation)
 *       - pad: Padding key (unused in this implementation)
 *    d. Generate ChaCha20 keystream:
 *       - Key: rho
 *       - Nonce: 96-bit zero nonce (0x000000000000000000000000)
 *       - Counter: 0
 *       - Output: 1300 bytes of pseudo-random stream
 *    e. Decrypt hop_payloads by XORing with the keystream
 * 
 * 3. Mutation Phase:
 *    - Apply LLVMFuzzerMutate to the decrypted plaintext payload
 *    - This allows standard fuzzing mutations on the actual routing data
 * 
 * 4. Re-encryption Phase:
 *    a. XOR the mutated payload with the same keystream to re-encrypt
 *    b. Calculate new HMAC-SHA256 over the encrypted payload using the mu key
 *    c. Reconstruct the complete packet with all components
 */

#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <modules/custommutator/secp256k1/include/secp256k1.h> 
#include <modules/custommutator/secp256k1/include/secp256k1_ecdh.h>
#include <modules/custommutator/crypto/hmac_sha256.h>
#include <modules/custommutator/crypto/chacha20.h>

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
                                         unsigned int seed);

// ============================================================================
// Constants
// ============================================================================

constexpr size_t KEY_SIZE = 32;
constexpr size_t PUBLIC_KEY_SIZE = 33;
constexpr size_t VERSION_SIZE = 1;
constexpr size_t HOP_PAYLOADS_SIZE = 1300;
constexpr size_t HMAC_SIZE = 32;

constexpr size_t PACKET_SIZE = KEY_SIZE + VERSION_SIZE + PUBLIC_KEY_SIZE + 
                                HOP_PAYLOADS_SIZE + HMAC_SIZE;

// Offsets within the packet structure
constexpr size_t PRIVATE_KEY_OFFSET = 0;
constexpr size_t VERSION_OFFSET = KEY_SIZE;
constexpr size_t PUBLIC_KEY_OFFSET = VERSION_OFFSET + VERSION_SIZE;
constexpr size_t HOP_PAYLOADS_OFFSET = PUBLIC_KEY_OFFSET + PUBLIC_KEY_SIZE;

constexpr unsigned int KEY_MUTATION_PROBABILITY = 1; // 1% chance

// ============================================================================
// SECP256k1 Context Management
// ============================================================================

static secp256k1_context* g_secp256k1_ctx = nullptr;

static secp256k1_context* get_secp256k1_context() {
    if (!g_secp256k1_ctx) {
        g_secp256k1_ctx = secp256k1_context_create(
            SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY
        );
    }
    return g_secp256k1_ctx;
}

// ============================================================================
// Key Derivation Functions
// ============================================================================

// Derives a key using HMAC-SHA256(key=keyType, message=sharedSecret)
static void derive_key(const uint8_t* shared_secret, 
                      uint8_t* derived_key, 
                      const char* key_type) {
    size_t key_type_len = strlen(key_type);
    CHMAC_SHA256 hmac(
        reinterpret_cast<const unsigned char*>(key_type), 
        key_type_len
    );
    hmac.Write(shared_secret, KEY_SIZE);
    hmac.Finalize(derived_key);
}

struct DerivedKeys {
    uint8_t rho[KEY_SIZE];  // Encryption key
    uint8_t mu[KEY_SIZE];   // HMAC key
    uint8_t um[KEY_SIZE];   // Error reproduction key
    uint8_t pad[KEY_SIZE];  // Padding key
};

static void derive_all_keys(const uint8_t* shared_secret, DerivedKeys* keys) {
    derive_key(shared_secret, keys->rho, "rho");
    derive_key(shared_secret, keys->mu, "mu");
    derive_key(shared_secret, keys->um, "um");
    derive_key(shared_secret, keys->pad, "pad");
}

// ============================================================================
// Cryptographic Operations
// ============================================================================

// Compute ECDH shared secret
static bool compute_shared_secret(const uint8_t* private_key,
                                  const uint8_t* public_key_bytes,
                                  uint8_t* shared_secret) {
    secp256k1_context* ctx = get_secp256k1_context();
    if (!ctx) {
        return false;
    }
    
    secp256k1_pubkey public_key;
    if (!secp256k1_ec_pubkey_parse(ctx, &public_key, public_key_bytes, PUBLIC_KEY_SIZE)) {
        return false;
    }
    
    if (!secp256k1_ecdh(ctx, shared_secret, &public_key, private_key, nullptr, nullptr)) {
        return false;
    }
    
    return true;
}

// Generate ChaCha20 keystream for encryption/decryption
static void generate_keystream(const uint8_t* rho_key, uint8_t* keystream) {
    std::span<const std::byte> key_span(
        reinterpret_cast<const std::byte*>(rho_key), 
        KEY_SIZE
    );
    
    ChaCha20 chacha(key_span);
    ChaCha20::Nonce96 nonce = {0, 0};
    chacha.Seek(nonce, 0);
    
    std::span<std::byte> keystream_span(
        reinterpret_cast<std::byte*>(keystream), 
        HOP_PAYLOADS_SIZE
    );
    chacha.Keystream(keystream_span);
}

// XOR operation for encryption/decryption
static void xor_buffers(uint8_t* dest, 
                       const uint8_t* src, 
                       const uint8_t* key, 
                       size_t length) {
    for (size_t i = 0; i < length; i++) {
        dest[i] = src[i] ^ key[i];
    }
}

// Calculate HMAC over the payload
static void calculate_hmac(const uint8_t* mu_key,
                          const uint8_t* payload,
                          size_t payload_size,
                          uint8_t* hmac_output) {
    CHMAC_SHA256 hmac(mu_key, KEY_SIZE);
    hmac.Write(payload, payload_size);
    hmac.Finalize(hmac_output);
}

// ============================================================================
// Key Generation Functions
// ============================================================================

static void generate_random_bytes(uint8_t* buffer, size_t size, unsigned int seed) {
    srand(seed);
    for (size_t i = 0; i < size; i++) {
        buffer[i] = rand() % 256;
    }
}

static bool generate_keypair(uint8_t* private_key,
                             uint8_t* public_key,
                             unsigned int seed) {
    generate_random_bytes(private_key, KEY_SIZE, seed);
    
    secp256k1_context* ctx = get_secp256k1_context();
    if (!ctx) {
        return false;
    }
    
    secp256k1_pubkey pubkey;
    if (!secp256k1_ec_pubkey_create(ctx, &pubkey, private_key)) {
        return false;
    }
    
    size_t len = PUBLIC_KEY_SIZE;
    return secp256k1_ec_pubkey_serialize(ctx, public_key, &len, 
                                        &pubkey, SECP256K1_EC_COMPRESSED);
}

// ============================================================================
// Packet Building Functions
// ============================================================================

static size_t build_template_packet(uint8_t* buffer, size_t max_size) {
    if (max_size < PACKET_SIZE) {
        return 0;
    }
    
    memset(buffer, 0, PACKET_SIZE);
    buffer[VERSION_OFFSET] = 0; // Version 0
    
    return LLVMFuzzerMutate(buffer, PACKET_SIZE, max_size);
}

static size_t reconstruct_packet(uint8_t* buffer,
                                 const uint8_t* private_key,
                                 uint8_t version,
                                 const uint8_t* public_key,
                                 const uint8_t* encrypted_payload,
                                 size_t payload_size,
                                 const uint8_t* hmac,
                                 size_t max_size) {
    size_t required_size = KEY_SIZE + VERSION_SIZE + PUBLIC_KEY_SIZE + 
                          payload_size + HMAC_SIZE;
    
    if (max_size < required_size) {
        required_size = max_size;
    }
    
    size_t offset = 0;
    
    // Copy private key
    memcpy(buffer + offset, private_key, KEY_SIZE);
    offset += KEY_SIZE;
    
    // Copy version
    if (offset < max_size) {
        buffer[offset] = version;
        offset += VERSION_SIZE;
    }
    
    // Copy public key
    if (offset + PUBLIC_KEY_SIZE <= max_size) {
        memcpy(buffer + offset, public_key, PUBLIC_KEY_SIZE);
        offset += PUBLIC_KEY_SIZE;
    }
    
    // Copy encrypted payload
    size_t remaining = max_size - offset;
    size_t payload_to_copy = std::min(payload_size, remaining - HMAC_SIZE);
    if (payload_to_copy > 0) {
        memcpy(buffer + offset, encrypted_payload, payload_to_copy);
        offset += payload_to_copy;
    }
    
    // Copy HMAC
    if (offset + HMAC_SIZE <= max_size) {
        memcpy(buffer + offset, hmac, HMAC_SIZE);
        offset += HMAC_SIZE;
    }
    
    return offset;
}

static void mutate_packet_keys(uint8_t* fuzz_data, unsigned int seed) {
    // Mutate the private key
    generate_random_bytes(fuzz_data + PRIVATE_KEY_OFFSET, KEY_SIZE, seed);
    
    // Generate new ephemeral keypair
    uint8_t ephemeral_private[KEY_SIZE];
    uint8_t ephemeral_public[PUBLIC_KEY_SIZE];
    
    if (generate_keypair(ephemeral_private, ephemeral_public, seed)) {
        memcpy(fuzz_data + PUBLIC_KEY_OFFSET, ephemeral_public, PUBLIC_KEY_SIZE);
    }
}

// ============================================================================
// Main Mutator Logic
// ============================================================================

size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
                               unsigned int seed) {

    // Generate template packet if input is too small
    if (size < PACKET_SIZE) {
        return build_template_packet(fuzz_data, max_size);
    }

    // Optionally mutate keys
    bool should_mutate_keys = (seed % 100) < KEY_MUTATION_PROBABILITY;
    if (should_mutate_keys) {
        mutate_packet_keys(fuzz_data, seed);
    }

    // Step 1: Compute shared secret via ECDH
    uint8_t shared_secret[KEY_SIZE];
    if (!compute_shared_secret(
            fuzz_data + PRIVATE_KEY_OFFSET,
            fuzz_data + PUBLIC_KEY_OFFSET,
            shared_secret)) {
        return size; // Cannot proceed without valid shared secret
    }

    // Step 2: Derive encryption and authentication keys
    DerivedKeys keys;
    derive_all_keys(shared_secret, &keys);

    // Step 3: Generate ChaCha20 keystream for decryption
    uint8_t keystream[HOP_PAYLOADS_SIZE];
    generate_keystream(keys.rho, keystream);

    // Step 4: Decrypt the hop payloads
    size_t available_payload = 0;
    uint8_t decrypted_payload[HOP_PAYLOADS_SIZE] = {0};

    if (size >= HOP_PAYLOADS_OFFSET + HOP_PAYLOADS_SIZE) {
        available_payload = HOP_PAYLOADS_SIZE;
    } else if (size > HOP_PAYLOADS_OFFSET) {
        available_payload = size - HOP_PAYLOADS_OFFSET;
    } else {
        return 0;
    }

    xor_buffers(
        decrypted_payload,
        fuzz_data + HOP_PAYLOADS_OFFSET,
        keystream,
        available_payload
    );

    // Step 5: Mutate the decrypted payload
    size_t mutated_size = LLVMFuzzerMutate(
        decrypted_payload, 
        available_payload, 
        HOP_PAYLOADS_SIZE
    );

    // Step 6: Re-encrypt the mutated payload
    uint8_t encrypted_payload[HOP_PAYLOADS_SIZE] = {0};
    xor_buffers(
        encrypted_payload,
        decrypted_payload,
        keystream,
        mutated_size
    );

    // Step 7: Calculate HMAC over the encrypted payload
    uint8_t hmac[HMAC_SIZE];
    calculate_hmac(
        keys.mu,
        encrypted_payload,
        HOP_PAYLOADS_SIZE,
        hmac
    );

    // Step 8: Reconstruct the complete packet
    return reconstruct_packet(
        fuzz_data,
        fuzz_data + PRIVATE_KEY_OFFSET,
        fuzz_data[VERSION_OFFSET],
        fuzz_data + PUBLIC_KEY_OFFSET,
        encrypted_payload,
        mutated_size,
        hmac,
        max_size
    );
}
#endif

#ifdef CUSTOM_MUTATOR_P2P_LIGHTNING_MESSAGE
// Custom mutator for Lightning P2P messages that creates lightning messages 
// with a specified message type.
//
// This custom mutator does the following:
//   1. Mutate the input using libFuzzer's default mutator `LLVMFuzzerMutate`
//   2. Update the message type to the specified message type
//   3. Return the mutated data with the specified message type

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <random>
#include <optional>
#include <algorithm>

static const std::unordered_map<std::string, uint16_t> message_types = {
        {"warning", 1},
        {"error", 17},
        {"ping", 18},
        {"pong", 19},
        {"funding_created", 34},
        {"open_channel", 32},
        {"funding_signed", 35},
        {"channel_ready", 36},
        {"shutdown", 38},
        {"closing_signed", 39},
        {"closing_complete", 40},
        {"update_add_htlc", 128},
    };

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
                                         unsigned int seed);

size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
                               unsigned int seed) {
    // First, mutate the data using LibFuzzer's default mutator
    size_t new_size = LLVMFuzzerMutate(fuzz_data, size, max_size);

    const char* mt_env = std::getenv("P2P_LIGHTNING_MESSAGE_TYPE");
    std::string message_type_env = mt_env ? std::string(mt_env) : std::string();
    uint16_t message_type = 0;

    if (auto it = message_types.find(message_type_env); it != message_types.end()) {
        message_type = it->second;
    } 

    if (message_type == 0) {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<size_t> dist(0, message_types.size() - 1);
        auto it = message_types.begin();
        std::advance(it, dist(rng));
        message_type = it->second;
    }

    if (new_size < 2) {
        if (max_size < 2) {
            return new_size;
        }
        new_size = 2;
    }

    // Update the message type
    fuzz_data[0] = message_type >> 8;
    fuzz_data[1] = message_type & 0xFF;

    return new_size;
}
#endif

#ifdef CUSTOM_MUTATOR_BOLT11
// We use a custom mutator to produce an input corpus that consists entirely of
// correctly encoded bech32 strings. This enables us to efficiently fuzz the
// bolt11 decoding logic without the fuzzer getting stuck on fuzzing the bech32
// decoding/encoding logic. This custom mutator is originally from core-lightning:
// https://github.com/ElementsProject/lightning/blob/3a7a1fad4eb56522b6ab590e53c695e2fb08e7e2/tests/fuzz/fuzz-bolt11.c#L59
//
// This custom mutator does the following things:
//   1. Attempt to bech32 decode the given input (returns the encoded dummy
//      invoice on failure).
//   2. Mutate either the human readable or data part of the invoice using
//      libFuzzer's default mutator `LLVMFuzzerMutate`.
//   3. Attempt to bech32 encode the mutated hrp and data (returns the endcoded
//      dummy on failure).
//   4. Write the encoded result to `fuzz_data` if its size does not exceed
//      `max_size`, otherwise return the encoded dummy invoice.
extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);
extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
			       unsigned int seed);
extern "C" size_t LLVMFuzzerCustomCrossOver(const uint8_t *in1, size_t in1_size, const uint8_t *in2,
				 size_t in2_size, uint8_t *out, size_t max_out_size,
				 unsigned seed);

// Encodes a dummy bolt11 invoice into `fuzz_data` and returns the size of the
// encoded string.
static size_t initial_input(uint8_t *fuzz_data, size_t max_size)
{
    constexpr std::string_view dummy =
        "lnbc16lta047pp5h6lta047h6lta047h6lta047h6lta047h6lta047h6lta047"
	    "h6lqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
	    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqxnht6w";

    size_t output_size = std::min(max_size, dummy.size());
    std::memcpy(fuzz_data, dummy.data(), output_size);
    return output_size;
}

size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size, size_t max_size,
                               unsigned int seed)
{
    // A minimum size of 9 prevents hrp_maxlen <= 0 and data_maxlen <= 0.
    if (size < 9)
        return initial_input(fuzz_data, max_size);

    // Interpret fuzz input as string
    std::string input(reinterpret_cast<char*>(fuzz_data), size);

    size_t data_maxlen = input.size() - 8;
    size_t hrp_maxlen = input.size() - 6;

    // Attempt to bech32 decode the input
    bech32::DecodeResult decoded = bech32::Decode(input, bech32::CharLimit::CUSTOM_MUTATOR);
    if (decoded.encoding != bech32::Encoding::BECH32) {
        // Decoding failed, this should only happen when starting from
        // an empty corpus.
        return initial_input(fuzz_data, max_size);
    }

    auto data = decoded.data;
    auto hrp = decoded.hrp;

    // Mutate either the hrp or data
    std::srand(seed);
    switch (std::rand() % 2) {
        case 0: { // Mutate hrp
            // Make sure we have a buffer that's large enough for mutation
            std::vector<uint8_t> hrp_buffer(hrp.begin(), hrp.end());
            // Reserve enough space for the maximum mutation size
            hrp_buffer.resize(hrp_maxlen);

            // Mutate the buffer
            size_t new_len = LLVMFuzzerMutate(hrp_buffer.data(),
                                             hrp.size(),
                                             hrp_maxlen);

            // Convert back to string with the new length
            hrp = std::string(reinterpret_cast<char*>(hrp_buffer.data()), new_len);

            // Sanitize hrp - ensure only valid ASCII characters (33-126) and not uppercase
            for (char& c : hrp) {
                if (c < 33 || c > 126) {
                    c = 'a' + (c % 26); // Replace with a valid lowercase letter
                } else if (c >= 'A' && c <= 'Z') {
                    c = c - 'A' + 'a'; // Convert uppercase to lowercase
                }
            }
            break;
        }
        case 1: { // Mutate data
            size_t original_data_size = data.size();
            // Make sure we have a buffer that's large enough for mutation
            data.resize(std::max(data.size(), data_maxlen));

            size_t new_len = LLVMFuzzerMutate(data.data(),
                                                original_data_size,
                                                data_maxlen);

            // It ensures that all values remain valid 5-bit values
            for (auto& val : data) {
                val &= 0x1F;
            }
            break;
        }
    }

    if (!hrp.empty()) {
        for (const char& c : hrp) {
            if (c >= 'A' && c <= 'Z') {
                return initial_input(fuzz_data, max_size);
            }
        }
    }

    std::string output = bech32::Encode(bech32::Encoding::BECH32, hrp, data);

    if (output.length() > max_size) {
        return initial_input(fuzz_data, max_size);
    }

    std::memcpy(fuzz_data, output.data(), output.length());
    return output.length();
}

size_t LLVMFuzzerCustomCrossOver(const uint8_t *in1, size_t in1_size, const uint8_t *in2,
                               size_t in2_size, uint8_t *out, size_t max_out_size,
                               unsigned seed)
{
    if (in1_size < 9 || in2_size < 9)
        return 0;

    // Interpret fuzz inputs as strings
    std::string input1(reinterpret_cast<const char*>(in1), in1_size);
    std::string input2(reinterpret_cast<const char*>(in2), in2_size);

    // Attempt to bech32 decode the inputs
    bech32::DecodeResult result1 = bech32::Decode(input1, bech32::CharLimit::CUSTOM_MUTATOR);
    if (result1.encoding != bech32::Encoding::BECH32) {
        // Decoding failed
        return 0;
    }

    bech32::DecodeResult result2 = bech32::Decode(input2, bech32::CharLimit::CUSTOM_MUTATOR);
    if (result2.encoding != bech32::Encoding::BECH32) {
        // Decoding failed
        return 0;
    }

    std::string hrp1 = result1.hrp;
    std::string hrp2 = result2.hrp;
    std::vector<uint8_t> data1 = result1.data;
    std::vector<uint8_t> data2 = result2.data;

    std::srand(seed);
    std::string out_hrp;
    std::vector<uint8_t> out_data;

    if (std::rand() % 2) {
        // Cross-over the HRP
        out_data = data1;

        std::span<const uint8_t> hrp1_span(reinterpret_cast<const uint8_t*>(hrp1.data()), hrp1.size());
        std::span<const uint8_t> hrp2_span(reinterpret_cast<const uint8_t*>(hrp2.data()), hrp2.size());

        size_t max_out_data_size = max_out_size - data1.size() - 8;

        auto out_hrp_vec = cross_over(
            hrp1_span,
            hrp2_span,
            max_out_data_size,
            static_cast<unsigned>(std::rand()));

        // Convert back to string and ensure null termination
        out_hrp = std::string(out_hrp_vec.begin(), out_hrp_vec.begin() + out_hrp_vec.size());
    } else {
        // Cross-over the data part
        out_hrp = hrp1;

        size_t max_out_data_size = max_out_size - hrp1.size() - 8;

        out_data = cross_over(
            data1,
            data2,
            max_out_data_size,
            static_cast<unsigned>(std::rand()));
    }

    // Encode the output
    std::string encoded = bech32::Encode(bech32::Encoding::BECH32, out_hrp, out_data);

    if (encoded.size() > max_out_size) {
        return 0;
    }

    // Copy the result to out buffer
    std::memcpy(out, encoded.data(), encoded.size());
    return encoded.size();
}
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size)
{
  const char* target = std::getenv("FUZZ");
  driver = std::make_shared<bitcoinfuzz::Driver>(module_logger);
#ifdef BITCOIN_CORE
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Bitcoin>());
#endif
#ifdef RUST_BITCOIN
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Rustbitcoin>());
#endif
#ifdef RUST_MINISCRIPT
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Rustminiscript>());
#endif
#ifdef BTCD
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Btcd>());
#endif
#ifdef NBITCOIN
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::NBitcoin>());
#endif
#ifdef LND
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Lnd>());
#endif
#ifdef LDK
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Ldk>());
#endif
#ifdef NLIGHTNING
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::NLightning>());
#endif
#ifdef EMBIT
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Embit>());
#endif
#ifdef CLIGHTNING
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::CLightning>());
#endif
#ifdef ECLAIR
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::Eclair>());
#endif
#ifdef LIGHTNING_KMP
  driver->LoadModule(std::make_shared<bitcoinfuzz::module::LightningKmp>());
#endif

#ifdef CUSTOM_MUTATOR_BOLT11
  module_logger.addCustomMutator("BOLT11 Bech32 Custom Mutator");
#endif

#ifdef CUSTOM_MUTATOR_BOLT12_OFFER
  module_logger.addCustomMutator("BOLT12 Offer/Invoice Custom Mutator");
#endif

#ifdef CUSTOM_MUTATOR_P2P_MESSAGE
  module_logger.addCustomMutator("Bitcoin P2P Message Custom Mutator");
#endif

#ifdef CUSTOM_MUTATOR_P2P_LIGHTNING_MESSAGE
  module_logger.addCustomMutator("Lightning P2P Message Custom Mutator");
#endif

#ifdef CUSTOM_MUTATOR_ONION_LEGACY
  module_logger.addCustomMutator("Onion Custom Mutator Legacy");
#endif


  module_logger.logModules();
  driver->Run(Data, Size, target);
  return 0;
}
