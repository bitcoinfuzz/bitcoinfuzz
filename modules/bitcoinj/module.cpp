#include "module.h"
#include <jnibridge.h>

namespace bitcoinfuzz {
namespace module {
BitcoinJ::BitcoinJ(void) : BaseModule("BitcoinJ") {}

std::optional<std::string>
BitcoinJ::bip32_master_keygen(std::span<const uint8_t> buffer) const {
  return CallJvm("bitcoinj/Wrapper", "createMasterKey", buffer);
}

std::optional<std::string> BitcoinJ::bip32_deserialize_extended_key(
    std::span<const uint8_t> buffer) const {
  return CallJvm("bitcoinj/Wrapper", "deserializeExtendedKey", buffer);
}
} // namespace module
} // namespace bitcoinfuzz
