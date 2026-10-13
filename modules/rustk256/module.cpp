#include "module.h"
#include "k256_lib/k256_lib.h"
#include <cassert>
#include <span>

namespace bitcoinfuzz {
namespace module {
K256::K256(void) : BaseModule("K256") {}

std::optional<std::string>
K256::private_to_public_key(std::span<const uint8_t> buffer) const {
  return TakeResult(k256_private_to_public_key(buffer.data()));
}

std::optional<std::string>
K256::pubkey_parse(std::span<const uint8_t> buffer) const {
  // Unlike the other entry points in this library, k256_pubkey_parse never
  // skips: rejection is reported as "ERR". Assert rather than returning
  // std::nullopt, which the driver reads as "module does not implement this
  // target" and would silently drop this module from the comparison.
  bf_result r = k256_pubkey_parse(buffer.data(), buffer.size());
  assert(r.status != BF_SKIP);
  return TakeResult(r);
}

std::optional<std::string>
K256::sign_compact(std::span<const uint8_t> buffer,
                   std::span<const uint8_t> hash) const {
  return TakeResult(k256_sign_compact(buffer.data(), hash.data()));
}

std::optional<std::string> K256::sign_der(std::span<const uint8_t> buffer,
                                          std::span<const uint8_t> hash) const {
  return TakeResult(k256_sign_der(buffer.data(), hash.data()));
}

std::optional<bool> K256::sign_verify(std::span<const uint8_t> buffer,
                                      std::span<const uint8_t> hash,
                                      std::span<const uint8_t> sign) const {
  return k256_sign_verify(buffer.data(), hash.data(), sign.data(), sign.size());
}

std::optional<std::string> K256::ecdh(std::span<const uint8_t> buffer,
                                      std::span<const uint8_t> pubkey) const {
  return TakeResult(k256_ecdh(buffer.data(), pubkey.data()));
}

std::optional<std::string>
K256::sign_schnorr(std::span<const uint8_t> buffer,
                   std::span<const uint8_t> hash,
                   std::span<const uint8_t> aux) const {
  return TakeResult(k256_sign_schnorr(buffer.data(), hash.data(), aux.data()));
}

} // namespace module
} // namespace bitcoinfuzz
