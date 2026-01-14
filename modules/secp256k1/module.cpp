#define template c_template // avoid C++ keyword conflict just during includes

extern "C" {
#include <secp256k1.h>
#include <secp256k1_ecdh.h>
#include <secp256k1_extrakeys.h>
#include <secp256k1_schnorrsig.h>
}

#undef template

#include "module.h"
#include <assert.h>
#include <iomanip>
#include <sstream>

#define SECP256K1_PUBKEY_COMPRESSED_LEN 33
#define SECP256K1_SIGNATURE_COMPACT_LEN 64
#define SECP256K1_SIGNATURE_DER_MAX_LEN 74
#define SECP256K1_SHARED_SECRET_LEN 32

secp256k1_context *secp256k1_ctx;

void init(int *argc, char ***argv) {
  if (!secp256k1_ctx) {
    secp256k1_ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
    assert(secp256k1_ctx);
  }
}

std::string hex_encode(const unsigned char *data, size_t len) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (size_t i = 0; i < len; ++i) {
    oss << std::setw(2) << static_cast<int>(data[i]);
  }
  return oss.str();
}

std::optional<std::string>
secp256k1_private_to_public_key(std::span<const uint8_t> buffer) {
  int ret = 0;
  secp256k1_pubkey pubkey;
  size_t pubkey_len = SECP256K1_PUBKEY_COMPRESSED_LEN;
  std::vector<uint8_t> pubkey_compressed(pubkey_len);
  const uint8_t *privkey = buffer.data();

  if (!secp256k1_ec_seckey_verify(secp256k1_ctx, privkey)) {
    return std::nullopt;
  }

  ret = secp256k1_ec_pubkey_create(secp256k1_ctx, &pubkey, privkey);
  ret = ret && secp256k1_ec_pubkey_serialize(
                   secp256k1_ctx, pubkey_compressed.data(), &pubkey_len,
                   &pubkey, SECP256K1_EC_COMPRESSED);
  if (!ret) {
    return "";
  }

  return hex_encode(pubkey_compressed.data(), pubkey_len);
}

std::optional<std::string>
secp256k1_sign_compact(std::span<const uint8_t> buffer,
                       std::span<const uint8_t> hash) {
  int ret = 0;
  secp256k1_ecdsa_signature signature;
  std::vector<uint8_t> signature_compact(SECP256K1_SIGNATURE_COMPACT_LEN);
  const uint8_t *privkey = buffer.data();

  if (!secp256k1_ec_seckey_verify(secp256k1_ctx, privkey)) {
    return std::nullopt;
  }

  ret = secp256k1_ecdsa_sign(secp256k1_ctx, &signature, hash.data(), privkey,
                             nullptr, nullptr);
  ret = ret && secp256k1_ecdsa_signature_serialize_compact(
                   secp256k1_ctx, signature_compact.data(), &signature);
  if (!ret) {
    return "";
  }

  return hex_encode(signature_compact.data(), SECP256K1_SIGNATURE_COMPACT_LEN);
}

std::optional<std::string> secp256k1_sign_der(std::span<const uint8_t> buffer,
                                              std::span<const uint8_t> hash) {
  int ret = 0;
  secp256k1_ecdsa_signature signature;
  size_t signature_der_len = SECP256K1_SIGNATURE_DER_MAX_LEN;
  std::vector<uint8_t> signature_der(signature_der_len);
  const uint8_t *privkey = buffer.data();

  if (!secp256k1_ec_seckey_verify(secp256k1_ctx, privkey)) {
    return std::nullopt;
  }

  ret = secp256k1_ecdsa_sign(secp256k1_ctx, &signature, hash.data(), privkey,
                             nullptr, nullptr);
  ret = ret && secp256k1_ecdsa_signature_serialize_der(
                   secp256k1_ctx, signature_der.data(), &signature_der_len,
                   &signature);
  if (!ret) {
    return "";
  }

  return hex_encode(signature_der.data(), signature_der_len);
}

std::optional<bool> secp256k1_sign_verify(std::span<const uint8_t> buffer,
                                          std::span<const uint8_t> hash,
                                          std::span<const uint8_t> sign) {
  int ret = 0;
  secp256k1_ecdsa_signature signature;
  secp256k1_pubkey pubkey;
  const uint8_t *privkey = buffer.data();

  if (!secp256k1_ec_seckey_verify(secp256k1_ctx, privkey)) {
    return std::nullopt;
  }

  ret = secp256k1_ec_pubkey_create(secp256k1_ctx, &pubkey, privkey);
  ret = ret && secp256k1_ecdsa_signature_parse_der(secp256k1_ctx, &signature,
                                                   sign.data(), sign.size());
  ret = ret &&
        secp256k1_ecdsa_verify(secp256k1_ctx, &signature, hash.data(), &pubkey);

  return ret == 1;
}

std::optional<std::string>
secp256k1_ecdh_generate(std::span<const uint8_t> buffer,
                        std::span<const uint8_t> pubkey_buf) {
  int ret = 0;
  secp256k1_pubkey pubkey;
  std::vector<uint8_t> shared_secret(SECP256K1_SHARED_SECRET_LEN);
  const uint8_t *privkey = buffer.data();

  if (!secp256k1_ec_seckey_verify(secp256k1_ctx, privkey)) {
    return std::nullopt;
  }

  ret = ret = secp256k1_ec_pubkey_parse(secp256k1_ctx, &pubkey,
                                        pubkey_buf.data(), pubkey_buf.size());
  ret = ret && secp256k1_ecdh(secp256k1_ctx, shared_secret.data(), &pubkey,
                              privkey, nullptr, nullptr);
  if (!ret) {
    return "";
  }

  return hex_encode(shared_secret.data(), SECP256K1_SHARED_SECRET_LEN);
}

std::optional<std::string>
secp256k1_sign_schnorr(std::span<const uint8_t> buffer,
                       std::span<const uint8_t> hash,
                       std::span<const uint8_t> aux) {
  int ret = 0;
  secp256k1_keypair keypair;
  std::vector<uint8_t> signature(64);
  const uint8_t *privkey = buffer.data();

  if (!secp256k1_ec_seckey_verify(secp256k1_ctx, privkey)) {
    return std::nullopt;
  }

  ret = secp256k1_keypair_create(secp256k1_ctx, &keypair, privkey);
  if (!ret)
    return "";

  ret = secp256k1_schnorrsig_sign32(secp256k1_ctx, signature.data(),
                                    hash.data(), &keypair, aux.data());

  if (!ret) {
    return "";
  }

  return hex_encode(signature.data(), 64);
}

namespace bitcoinfuzz {
namespace module {
Secp256k1::Secp256k1(void) : BaseModule("Secp256k1") { init(nullptr, nullptr); }

std::optional<std::string>
Secp256k1::private_to_public_key(std::span<const uint8_t> buffer) const {
  return secp256k1_private_to_public_key(buffer);
}

std::optional<std::string>
Secp256k1::sign_compact(std::span<const uint8_t> buffer,
                        std::span<const uint8_t> hash) const {
  return secp256k1_sign_compact(buffer, hash);
}

std::optional<std::string>
Secp256k1::sign_der(std::span<const uint8_t> buffer,
                    std::span<const uint8_t> hash) const {
  return secp256k1_sign_der(buffer, hash);
}

std::optional<bool>
Secp256k1::sign_verify(std::span<const uint8_t> buffer,
                       std::span<const uint8_t> hash,
                       std::span<const uint8_t> sign) const {
  return secp256k1_sign_verify(buffer, hash, sign);
}

std::optional<std::string>
Secp256k1::ecdh(std::span<const uint8_t> buffer,
                std::span<const uint8_t> pubkey) const {
  return secp256k1_ecdh_generate(buffer, pubkey);
}

std::optional<std::string>
Secp256k1::sign_schnorr(std::span<const uint8_t> buffer,
                        std::span<const uint8_t> hash,
                        std::span<const uint8_t> aux) const {
  return secp256k1_sign_schnorr(buffer, hash, aux);
}

} // namespace module
} // namespace bitcoinfuzz
