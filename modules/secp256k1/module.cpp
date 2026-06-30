#define template c_template // avoid C++ keyword conflict just during includes

extern "C" {
#include <secp256k1.h>
#include <secp256k1_ecdh.h>
#include <secp256k1_ellswift.h>
#include <secp256k1_extrakeys.h>
#include <secp256k1_musig.h>
#include <secp256k1_schnorrsig.h>
}

#undef template

#include "module.h"
#include <algorithm>
#include <array>
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

  if (!secp256k1_ec_pubkey_parse(secp256k1_ctx, &pubkey, pubkey_buf.data(),
                                 pubkey_buf.size())) {
    return std::nullopt;
  }
  ret = secp256k1_ecdh(secp256k1_ctx, shared_secret.data(), &pubkey, privkey,
                       nullptr, nullptr);
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

std::optional<std::string>
secp256k1_decode_ellswift(std::span<const uint8_t> buffer) {
  static size_t pubkey_len = SECP256K1_PUBKEY_COMPRESSED_LEN;
  std::vector<uint8_t> pubkey_compressed(pubkey_len);
  secp256k1_pubkey pubkey;
  secp256k1_ellswift_decode(secp256k1_ctx, &pubkey, buffer.data());

  secp256k1_ec_pubkey_serialize(secp256k1_ctx, pubkey_compressed.data(),
                                &pubkey_len, &pubkey, SECP256K1_EC_COMPRESSED);

  return hex_encode(pubkey_compressed.data(), pubkey_len);
}

std::optional<std::string>
secp256k1_roundtrip_ellswift(std::span<const uint8_t> privkey) {
  if (!secp256k1_ec_seckey_verify(secp256k1_ctx, privkey.data())) {
    return std::nullopt;
  }

  uint8_t ell64[64];
  if (!secp256k1_ellswift_create(secp256k1_ctx, ell64, privkey.data(),
                                 nullptr)) {
    return std::nullopt;
  }

  secp256k1_pubkey pubkey;
  secp256k1_ellswift_decode(secp256k1_ctx, &pubkey, ell64);

  uint8_t pubkey_compressed[SECP256K1_PUBKEY_COMPRESSED_LEN];
  size_t pubkey_len = SECP256K1_PUBKEY_COMPRESSED_LEN;
  secp256k1_ec_pubkey_serialize(secp256k1_ctx, pubkey_compressed, &pubkey_len,
                                &pubkey, SECP256K1_EC_COMPRESSED);

  return hex_encode(pubkey_compressed, pubkey_len);
}

bool secp256k1_musig2_prepare_keys(
    std::span<const uint8_t> seckey_buffer,
    std::vector<secp256k1_pubkey> &pubkeys,
    std::vector<const secp256k1_pubkey *> &pubkey_ptrs,
    std::vector<secp256k1_keypair> *keypairs) {
  const size_t num_keys = seckey_buffer.size() / 32;
  if (pubkeys.size() != num_keys || pubkey_ptrs.size() != num_keys ||
      (keypairs != nullptr && keypairs->size() != num_keys)) {
    return false;
  }

  for (size_t i = 0; i < num_keys; ++i) {
    const uint8_t *seckey = seckey_buffer.data() + (i * 32);
    if (!secp256k1_ec_seckey_verify(secp256k1_ctx, seckey) ||
        !secp256k1_ec_pubkey_create(secp256k1_ctx, &pubkeys[i], seckey) ||
        (keypairs != nullptr &&
         !secp256k1_keypair_create(secp256k1_ctx, &(*keypairs)[i], seckey))) {
      return false;
    }
    pubkey_ptrs[i] = &pubkeys[i];
  }

  return true;
}

std::optional<std::string>
secp256k1_schnorr_verify(std::span<const uint8_t> privkey,
                         std::span<const uint8_t> hash,
                         std::span<const uint8_t> sig) {
  secp256k1_pubkey pubkey;
  secp256k1_xonly_pubkey xonly_pubkey;
  int pk_parity;

  if (!secp256k1_ec_pubkey_create(secp256k1_ctx, &pubkey, privkey.data())) {
    return "INVALID";
  }
  secp256k1_xonly_pubkey_from_pubkey(secp256k1_ctx, &xonly_pubkey, &pk_parity,
                                     &pubkey);
  if (secp256k1_schnorrsig_verify(secp256k1_ctx, sig.data(), hash.data(),
                                  hash.size(), &xonly_pubkey) == 0) {
    return "INVALID";
  }
  return "VALID";
}

std::optional<std::string>
secp256k1_musig2_key_agg(std::span<const uint8_t> seckey_buffer) {
  // Input is num_keys concatenated 32-byte private keys. The corresponding
  // public keys are derived here so that nearly every input reaches the
  // aggregation step (a random pubkey rarely lands on the curve).
  size_t num_keys = seckey_buffer.size() / 32;

  // Derive a pubkey for each scalar, preserving input order: BIP-327 key
  // aggregation is order-sensitive (sorting is a separate, optional step), so
  // neither this module nor the differential peer must reorder the keys.
  std::vector<secp256k1_pubkey> pubkeys(num_keys);
  std::vector<const secp256k1_pubkey *> pubkey_ptrs(num_keys);
  if (!secp256k1_musig2_prepare_keys(seckey_buffer, pubkeys, pubkey_ptrs,
                                     nullptr)) {
    return std::nullopt;
  }

  // Perform key aggregation with cache to get full pubkey
  secp256k1_xonly_pubkey agg_xonly_pk;
  secp256k1_musig_keyagg_cache keyagg_cache;
  if (!secp256k1_musig_pubkey_agg(secp256k1_ctx, &agg_xonly_pk, &keyagg_cache,
                                  pubkey_ptrs.data(), num_keys)) {
    // Aggregation rejected: report a sentinel (not nullopt) so the driver
    // compares it and an accept-vs-reject disagreement becomes a finding.
    return "AGG_FAIL";
  }

  // Get full pubkey (with parity) from cache
  secp256k1_pubkey agg_pk;
  if (!secp256k1_musig_pubkey_get(secp256k1_ctx, &agg_pk, &keyagg_cache)) {
    return std::nullopt;
  }

  // Serialize as compressed (33 bytes with 02/03 prefix for parity)
  size_t agg_pk_len = 33;
  std::vector<uint8_t> agg_pk_serialized(33);
  if (!secp256k1_ec_pubkey_serialize(secp256k1_ctx, agg_pk_serialized.data(),
                                     &agg_pk_len, &agg_pk,
                                     SECP256K1_EC_COMPRESSED)) {
    return std::nullopt;
  }

  return hex_encode(agg_pk_serialized.data(), 33);
}

std::optional<std::string> secp256k1_musig2_sign_session(
    const bitcoinfuzz::Musig2SignSessionInput &input) {
  const size_t num_keys = input.seckeys.size() / 32;
  if (num_keys == 0 || input.seckeys.size() != num_keys * 32 ||
      input.msg32.size() != 32 || input.nonce_seeds.size() != num_keys * 32) {
    return std::nullopt;
  }
  if ((input.use_xonly_tweak && input.xonly_tweak.size() != 32) ||
      (input.use_plain_tweak && input.plain_tweak.size() != 32)) {
    return std::nullopt;
  }

  std::vector<secp256k1_pubkey> pubkeys(num_keys);
  std::vector<const secp256k1_pubkey *> pubkey_ptrs(num_keys);
  std::vector<secp256k1_keypair> keypairs(num_keys);
  if (!secp256k1_musig2_prepare_keys(input.seckeys, pubkeys, pubkey_ptrs,
                                     &keypairs)) {
    return std::nullopt;
  }

  secp256k1_xonly_pubkey aggregate_xonly_pubkey;
  secp256k1_musig_keyagg_cache keyagg_cache;
  if (!secp256k1_musig_pubkey_agg(secp256k1_ctx, &aggregate_xonly_pubkey,
                                  &keyagg_cache, pubkey_ptrs.data(),
                                  num_keys)) {
    return "AGG_FAIL";
  }

  std::vector<secp256k1_musig_secnonce> secnonces(num_keys);
  std::vector<secp256k1_musig_pubnonce> pubnonces(num_keys);
  std::vector<const secp256k1_musig_pubnonce *> pubnonce_ptrs(num_keys);
  for (size_t i = 0; i < num_keys; ++i) {
    const uint8_t *seckey = input.seckeys.data() + (i * 32);
    // nonce_gen takes a mutable session_secrand32 (it clears the seed after
    // use), so copy the seed into a local writable buffer.
    std::array<unsigned char, 32> nonce_seed{};
    std::copy_n(input.nonce_seeds.data() + (i * 32), nonce_seed.size(),
                nonce_seed.begin());
    if (!secp256k1_musig_nonce_gen(
            secp256k1_ctx, &secnonces[i], &pubnonces[i], nonce_seed.data(),
            seckey, &pubkeys[i], input.msg32.data(), &keyagg_cache, nullptr)) {
      return "NONCE_GEN_FAIL";
    }
    pubnonce_ptrs[i] = &pubnonces[i];
  }

  secp256k1_musig_aggnonce aggnonce;
  if (!secp256k1_musig_nonce_agg(secp256k1_ctx, &aggnonce, pubnonce_ptrs.data(),
                                 num_keys)) {
    return "NONCE_AGG_FAIL";
  }

  if (input.use_xonly_tweak &&
      !secp256k1_musig_pubkey_xonly_tweak_add(
          secp256k1_ctx, nullptr, &keyagg_cache, input.xonly_tweak.data())) {
    return "TWEAK_FAIL";
  }
  if (input.use_plain_tweak &&
      !secp256k1_musig_pubkey_ec_tweak_add(
          secp256k1_ctx, nullptr, &keyagg_cache, input.plain_tweak.data())) {
    return "TWEAK_FAIL";
  }

  secp256k1_pubkey final_pubkey;
  secp256k1_xonly_pubkey final_xonly_pubkey;
  if (!secp256k1_musig_pubkey_get(secp256k1_ctx, &final_pubkey,
                                  &keyagg_cache) ||
      !secp256k1_xonly_pubkey_from_pubkey(secp256k1_ctx, &final_xonly_pubkey,
                                          nullptr, &final_pubkey)) {
    return "PUBKEY_GET_FAIL";
  }

  secp256k1_musig_session session;
  if (!secp256k1_musig_nonce_process(secp256k1_ctx, &session, &aggnonce,
                                     input.msg32.data(), &keyagg_cache)) {
    return "NONCE_PROCESS_FAIL";
  }

  std::vector<secp256k1_musig_partial_sig> partial_sigs(num_keys);
  std::vector<const secp256k1_musig_partial_sig *> partial_sig_ptrs(num_keys);
  for (size_t i = 0; i < num_keys; ++i) {
    if (!secp256k1_musig_partial_sign(secp256k1_ctx, &partial_sigs[i],
                                      &secnonces[i], &keypairs[i],
                                      &keyagg_cache, &session)) {
      return "PARTIAL_SIGN_FAIL";
    }
    // An honest signer's partial signature must verify.
    assert(secp256k1_musig_partial_sig_verify(secp256k1_ctx, &partial_sigs[i],
                                              &pubnonces[i], &pubkeys[i],
                                              &keyagg_cache, &session) == 1);
    partial_sig_ptrs[i] = &partial_sigs[i];
  }

  std::array<uint8_t, SECP256K1_SIGNATURE_COMPACT_LEN> final_sig{};
  if (!secp256k1_musig_partial_sig_agg(secp256k1_ctx, final_sig.data(),
                                       &session, partial_sig_ptrs.data(),
                                       num_keys)) {
    return "PARTIAL_SIG_AGG_FAIL";
  }

  // An honest session must yield a signature valid under the (tweaked)
  // aggregate key.
  assert(secp256k1_schnorrsig_verify(secp256k1_ctx, final_sig.data(),
                                     input.msg32.data(), input.msg32.size(),
                                     &final_xonly_pubkey) == 1);

  return hex_encode(final_sig.data(), final_sig.size());
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

std::optional<std::string>
Secp256k1::decode_ellswift(std::span<const uint8_t> buffer) const {
  return secp256k1_decode_ellswift(buffer);
}
std::optional<std::string>
Secp256k1::roundtrip_ellswift(std::span<const uint8_t> privkey) const {
  return secp256k1_roundtrip_ellswift(privkey);
}
std::optional<std::string>
Secp256k1::schnorr_verify(std::span<const uint8_t> privkey,
                          std::span<const uint8_t> hash,
                          std::span<const uint8_t> sign) const {
  return secp256k1_schnorr_verify(privkey, hash, sign);
}

std::optional<std::string>
Secp256k1::musig2_key_agg(std::span<const uint8_t> seckeys) const {
  return secp256k1_musig2_key_agg(seckeys);
}

std::optional<std::string>
Secp256k1::musig2_sign_session(const Musig2SignSessionInput &input) const {
  return secp256k1_musig2_sign_session(input);
}

} // namespace module
} // namespace bitcoinfuzz
