#include "module.h"
#include "NBitcoinSecp256k1/nbitcoinsecp256k1_lib.h"
#include <span>

namespace bitcoinfuzz {
namespace module {
NBitcoin_secp256k1::NBitcoin_secp256k1(void)
    : BaseModule("NBitcoin_secp256k1") {}
std::optional<std::string> NBitcoin_secp256k1::private_to_public_key(
    std::span<const uint8_t> buffer) const {
  char *p = nbitcoinsecp256k1_private_to_public_key(buffer.data());
  if (!p)
    return std::nullopt;
  std::string s(p);
  nbitcoinsecp256k1_free_c_string(p);
  return s;
}
std::optional<std::string>
NBitcoin_secp256k1::sign_compact(std::span<const uint8_t> buffer,
                                 std::span<const uint8_t> hash) const {
  char *p = nbitcoinsecp256k1_sign_compact(buffer.data(), hash.data());
  if (!p)
    return std::nullopt;
  std::string s(p);
  nbitcoinsecp256k1_free_c_string(p);
  return s;
}
std::optional<std::string>
NBitcoin_secp256k1::sign_der(std::span<const uint8_t> buffer,
                             std::span<const uint8_t> hash) const {
  char *p = nbitcoinsecp256k1_sign_der(buffer.data(), hash.data());
  if (!p)
    return std::nullopt;
  std::string s(p);
  nbitcoinsecp256k1_free_c_string(p);
  return s;
}
std::optional<bool>
NBitcoin_secp256k1::sign_verify(std::span<const uint8_t> buffer,
                                std::span<const uint8_t> hash,
                                std::span<const uint8_t> sign) const {
  return nbitcoinsecp256k1_sign_verify(buffer.data(), hash.data(), sign.data(),
                                       sign.size());
}
std::optional<std::string>
NBitcoin_secp256k1::ecdh(std::span<const uint8_t> buffer,
                         std::span<const uint8_t> pubkey) const {
  char *p = nbitcoinsecp256k1_ecdh(buffer.data(), pubkey.data());
  if (!p)
    return std::nullopt;
  std::string s(p);
  nbitcoinsecp256k1_free_c_string(p);
  return s;
}
} // namespace module
} // namespace bitcoinfuzz
