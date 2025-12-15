#ifndef BITCOINFUZZ_CUSTOMMUTATOR_SECP256K1_SIGNATURE_H
#define BITCOINFUZZ_CUSTOMMUTATOR_SECP256K1_SIGNATURE_H

#define template c_template

extern "C" {
#include <external/secp256k1/include/secp256k1.h>
}

#undef template

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

static constexpr size_t MIN_BUFFER_SIZE = 64;
static constexpr size_t MAX_SIGNATURE_DER_SIZE = 74;
static secp256k1_context *secp256k1_ctx = nullptr;

static secp256k1_context *get_secp256k1_context() {
  if (!secp256k1_ctx) {
    secp256k1_ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN |
                                             SECP256K1_CONTEXT_VERIFY);
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

  secp256k1_context *ctx = get_secp256k1_context();
  if (!ctx) {
    return new_size;
  }

  secp256k1_ecdsa_signature signature;
  size_t signature_der_len = MAX_SIGNATURE_DER_SIZE;
  unsigned char signature_der[signature_der_len];
  int ret = 0;

  std::span<const uint8_t> buffer(fuzz_data, MIN_BUFFER_SIZE);
  std::span<const uint8_t> privkey = buffer.subspan(0, 32);
  std::span<const uint8_t> hash = buffer.subspan(32, 32);

  ret = secp256k1_ec_seckey_verify(ctx, privkey.data());
  ret = ret && secp256k1_ecdsa_sign(ctx, &signature, hash.data(),
                                    privkey.data(), nullptr, nullptr);
  ret = ret && secp256k1_ecdsa_signature_serialize_der(
                   ctx, signature_der, &signature_der_len, &signature);

  if (ret && (MIN_BUFFER_SIZE + signature_der_len <= max_size)) {
    memcpy(fuzz_data + MIN_BUFFER_SIZE, signature_der, signature_der_len);
    return MIN_BUFFER_SIZE + signature_der_len;
  }

  return new_size;
}

#endif // BITCOINFUZZ_CUSTOMMUTATOR_SECP256K1_SIGNATURE_H