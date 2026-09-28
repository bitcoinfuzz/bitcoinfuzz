#include "module.h"
#include "aes_lib/aes_lib.h"
#include <span>

namespace bitcoinfuzz {
namespace module {

RustCryptoAes::RustCryptoAes(void) : BaseModule("RustCryptoAes") {}

std::optional<std::string>
RustCryptoAes::aes256_cbc(std::span<const uint8_t> key,
                          std::span<const uint8_t> iv, bool pad,
                          std::span<const uint8_t> data) const {
  return TakeResult(::rustcrypto_aes256_cbc(key.data(), iv.data(), pad,
                                            data.data(), data.size()));
}

} // namespace module
} // namespace bitcoinfuzz
