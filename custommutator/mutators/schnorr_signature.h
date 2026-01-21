#ifndef BITCOINFUZZ_CUSTOMMUTATOR_SECP256K1_SIGNATURE_H
#define BITCOINFUZZ_CUSTOMMUTATOR_SECP256K1_SIGNATURE_H

#define template c_template

extern "C" {
#include <external/secp256k1/include/secp256k1.h>
#include <external/secp256k1/include/secp256k1_schnorrsig.h>
}

#undef template

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <span>

static constexpr size_t MIN_BUFFER_SIZE = 64;
static constexpr size_t MAX_SIGNATURE_DER_SIZE = 74;
static constexpr size_t SCHNORR_SIGNATURE_SIZE = 64;
static secp256k1_context *secp256k1_ctx = nullptr;

static secp256k1_context *get_secp256k1_context() {
  if (!secp256k1_ctx) {
    secp256k1_ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
  }
  return secp256k1_ctx;
}

extern "C" size_t LLVMFuzzerMutate(uint8_t *Data, size_t Size, size_t MaxSize);

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *fuzz_data, size_t size,
                                          size_t max_size, unsigned int seed) {
  size_t new_size = LLVMFuzzerMutate(fuzz_data, size, max_size);

  if (new_size < MIN_BUFFER_SIZE) {
    return new_size;
  }

  std::srand(seed);
  // 50% chance to skip signature generation and test invalid/malformed inputs
  if (std::rand() % 2 == 0) {
    return new_size;
  }

  secp256k1_context *ctx = get_secp256k1_context();
  if (!ctx) {
    return new_size;
  }

  unsigned char signature[64];
  secp256k1_keypair keypair;

  std::span<const uint8_t> buffer(fuzz_data, MIN_BUFFER_SIZE);
  std::span<const uint8_t> privkey = buffer.subspan(0, 32);
  std::span<const uint8_t> hash = buffer.subspan(32, 32);

  if (secp256k1_keypair_create(ctx, &keypair, privkey.data()) == 0) {
    return new_size;
  }

  if (secp256k1_schnorrsig_sign32(ctx, signature, hash.data(), &keypair, nullptr) == 0) {
    return new_size;
  }

  if (MIN_BUFFER_SIZE + SCHNORR_SIGNATURE_SIZE <= max_size) {
    memcpy(fuzz_data + MIN_BUFFER_SIZE, signature, SCHNORR_SIGNATURE_SIZE);
    return MIN_BUFFER_SIZE + SCHNORR_SIGNATURE_SIZE;
  }

  return new_size;
}

#endif // BITCOINFUZZ_CUSTOMMUTATOR_SECP256K1_SIGNATURE_H
