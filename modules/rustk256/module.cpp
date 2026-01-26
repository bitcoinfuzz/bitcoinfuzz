#include "module.h"
#include "k256_lib/k256_lib.h"
#include <span>

namespace bitcoinfuzz {
namespace module {
K256::K256(void) : BaseModule("K256") {}

std::optional<std::string>
K256::private_to_public_key(std::span<const uint8_t> buffer) const {
  char *p = k256_private_to_public_key(buffer.data());
  if (!p)
    return std::nullopt;
  std::string s(p);
  k256_free_string(p);
  return s;
}

std::optional<std::string>
K256::sign_compact(std::span<const uint8_t> buffer,
                   std::span<const uint8_t> hash) const {
  char *p = k256_sign_compact(buffer.data(), hash.data());
  if (!p)
    return std::nullopt;
  std::string s(p);
  k256_free_string(p);
  return s;
}

std::optional<std::string> K256::sign_der(std::span<const uint8_t> buffer,
                                          std::span<const uint8_t> hash) const {
  char *p = k256_sign_der(buffer.data(), hash.data());
  if (!p)
    return std::nullopt;
  std::string s(p);
  k256_free_string(p);
  return s;
}

std::optional<bool> K256::sign_verify(std::span<const uint8_t> buffer,
                                      std::span<const uint8_t> hash,
                                      std::span<const uint8_t> sign) const {
  return k256_sign_verify(buffer.data(), hash.data(), sign.data(), sign.size());
}

std::optional<std::string> K256::ecdh(std::span<const uint8_t> buffer,
                                      std::span<const uint8_t> pubkey) const {
  char *p = k256_ecdh(buffer.data(), pubkey.data());
  if (!p)
    return std::nullopt;
  std::string s(p);
  k256_free_string(p);
  return s;
}

} // namespace module
} // namespace bitcoinfuzz
