extern "C" {
#include "common/sphinx.h"
#include <sodium/crypto_stream_chacha20.h>
}

struct keyset {
  struct secret pi, mu, rho, gamma;
};

static void generate_key_set(const struct secret *secret, struct keyset *keys) {
  subkey_from_hmac("rho", secret, &keys->rho);
  subkey_from_hmac("pi", secret, &keys->pi);
  subkey_from_hmac("mu", secret, &keys->mu);
  subkey_from_hmac("gamma", secret, &keys->gamma);
}

/* xor cipher stream into dst */
static void xor_cipher_stream(void *dst, const struct secret *k,
                              size_t dstlen) {
  const u8 nonce[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  crypto_stream_chacha20_xor(static_cast<unsigned char *>(dst),
                             static_cast<const unsigned char *>(dst), dstlen,
                             nonce, k->data);
}

#undef VERSION_SIZE
#undef HMAC_SIZE

#include "../mutators/onion.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Stub for LLVMFuzzerMutate
extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize) {
  return Size;
}

// Generate random bytes
static void generate_random_bytes(uint8_t *data, size_t size) {
  for (size_t i = 0; i < size; i++) {
    data[i] = rand() % 256;
  }
}

static bool generate_keypair(uint8_t *private_key, uint8_t *public_key) {
  generate_random_bytes(private_key, KEY_SIZE);

  secp256k1_context *ctx = get_secp256k1_context();
  if (!ctx) {
    return false;
  }

  secp256k1_pubkey pubkey;
  if (!secp256k1_ec_pubkey_create(ctx, &pubkey, private_key)) {
    return false;
  }

  size_t len = PUBLIC_KEY_SIZE;
  return secp256k1_ec_pubkey_serialize(ctx, public_key, &len, &pubkey,
                                       SECP256K1_EC_COMPRESSED);
}

// Verify encryption/decryption roundtrip
static bool test_encryption_decryption_roundtrip() {
  printf("[TEST] Encryption/Decryption Roundtrip...\n");

  uint8_t private_key[KEY_SIZE];
  uint8_t public_key[PUBLIC_KEY_SIZE];

  if (!generate_keypair(private_key, public_key)) {
    printf("  [FAIL] Failed to generate keypair\n");
    return false;
  }

  uint8_t shared_secret[KEY_SIZE];
  if (!compute_shared_secret(private_key, public_key, shared_secret)) {
    printf("  [FAIL] Failed to compute shared secret\n");
    return false;
  }

  DerivedKeys keys;
  derive_all_keys(shared_secret, &keys);

  uint8_t keystream[HOP_PAYLOADS_SIZE];
  generate_keystream(keys.rho, keystream);

  uint8_t plaintext[HOP_PAYLOADS_SIZE];
  generate_random_bytes(plaintext, HOP_PAYLOADS_SIZE);

  uint8_t ciphertext[HOP_PAYLOADS_SIZE];
  xor_buffers(ciphertext, plaintext, keystream, HOP_PAYLOADS_SIZE);

  uint8_t decrypted[HOP_PAYLOADS_SIZE];
  xor_buffers(decrypted, ciphertext, keystream, HOP_PAYLOADS_SIZE);

  secret rho;
  memcpy(&rho, keys.rho, KEY_SIZE);
  uint8_t c_plaintext[HOP_PAYLOADS_SIZE];
  memcpy(c_plaintext, plaintext, HOP_PAYLOADS_SIZE);
  xor_cipher_stream(c_plaintext, &rho, HOP_PAYLOADS_SIZE);

  if (memcmp(c_plaintext, ciphertext, HOP_PAYLOADS_SIZE) != 0) {
    printf("  [FAIL] Error on encryption\n");
    return false;
  }

  xor_cipher_stream(c_plaintext, &rho, HOP_PAYLOADS_SIZE);
  if (memcmp(c_plaintext, decrypted, HOP_PAYLOADS_SIZE) != 0) {
    printf("  [FAIL] Error on decryption\n");
    return false;
  }

  if (memcmp(plaintext, decrypted, HOP_PAYLOADS_SIZE) != 0) {
    printf("  [FAIL] Decrypted data doesn't match original plaintext!\n");
    return false;
  }

  printf("  [PASS] Encryption/decryption roundtrip successful\n");
  return true;
}

// Verify HMAC calculation produces same hmac as reference implementation
static bool test_hmac() {
  printf("[TEST] HMAC Calculation...\n");

  uint8_t key[KEY_SIZE];
  uint8_t data[HOP_PAYLOADS_SIZE];

  generate_random_bytes(key, KEY_SIZE);
  generate_random_bytes(data, HOP_PAYLOADS_SIZE);

  uint8_t hmac1[HMAC_SIZE];
  calculate_hmac(key, data, HOP_PAYLOADS_SIZE, hmac1);

  // Calculate HMAC using reference implementation (core lightning)
  struct hmac c_hmac;
  hmac(data, sizeof(data), key, sizeof(key), &c_hmac);

  if (memcmp(hmac1, c_hmac.bytes, HMAC_SIZE) != 0) {
    printf("  [FAIL] HMAC calculations are incorrect!\n");
    return false;
  }

  printf("  [PASS] HMAC calculations are correct\n");
  return true;
}

// Verify key derivation produces same keys as reference implementation
static bool test_key_derivation() {
  printf("[TEST] Key Derivation...\n");

  uint8_t shared_secret[KEY_SIZE];
  generate_random_bytes(shared_secret, KEY_SIZE);
  DerivedKeys keys;
  derive_all_keys(shared_secret, &keys);

  // Derive keys using reference implementation (core lightning)
  keyset ref_keyset;
  secret s;
  memcpy(s.data, shared_secret, KEY_SIZE);
  generate_key_set(&s, &ref_keyset);

  if (memcmp(keys.rho, ref_keyset.rho.data, KEY_SIZE) != 0) {
    printf("  [FAIL] rho key doesn't match reference implementation!\n");
    return false;
  }

  if (memcmp(keys.mu, ref_keyset.mu.data, KEY_SIZE) != 0) {
    printf("  [FAIL] mu key doesn't match reference implementation!\n");
    return false;
  }

  if (memcmp(keys.rho, keys.mu, KEY_SIZE) == 0) {
    printf("  [FAIL] rho and mu are identical (should be different)!\n");
    return false;
  }

  printf(
      "  [PASS] Key derivation matches reference and produces distinct keys\n");
  return true;
}

int main() {
  printf("Onion Custom Mutator Test\n\n");

  srand(42);

  int passed = 0;
  int total = 0;

  total++;
  if (test_encryption_decryption_roundtrip())
    passed++;
  total++;
  if (test_hmac())
    passed++;
  total++;
  if (test_key_derivation())
    passed++;

  printf("\nTest Results: %d/%d tests passed\n", passed, total);

  return (passed == total) ? 0 : 1;
}
