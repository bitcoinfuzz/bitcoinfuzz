#include "module.h"
#include "NBitcoinSecp256k1/nbitcoinsecp256k1_lib.h"
#include <span>

namespace bitcoinfuzz {
namespace module {
NBitcoin_secp256k1::NBitcoin_secp256k1(void)
    : BaseModule("NBitcoin_secp256k1") {}
std::optional<std::string> NBitcoin_secp256k1::private_to_public_key(
    std::span<const uint8_t> buffer) const {
  return TakeResult(nbitcoinsecp256k1_private_to_public_key(buffer.data()));
}
std::optional<std::string>
NBitcoin_secp256k1::sign_compact(std::span<const uint8_t> buffer,
                                 std::span<const uint8_t> hash) const {
  return TakeResult(nbitcoinsecp256k1_sign_compact(buffer.data(), hash.data()));
}
std::optional<std::string>
NBitcoin_secp256k1::sign_der(std::span<const uint8_t> buffer,
                             std::span<const uint8_t> hash) const {
  return TakeResult(nbitcoinsecp256k1_sign_der(buffer.data(), hash.data()));
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
  return TakeResult(nbitcoinsecp256k1_ecdh(buffer.data(), pubkey.data()));
}
std::optional<std::string>
NBitcoin_secp256k1::schnorr_verify(std::span<const uint8_t> privkey,
                                   std::span<const uint8_t> hash,
                                   std::span<const uint8_t> sign) const {
  return TakeResult(nbitcoinsecp256k1_schnorr_verify(privkey.data(),
                                                     hash.data(), sign.data()));
}
} // namespace module
} // namespace bitcoinfuzz
