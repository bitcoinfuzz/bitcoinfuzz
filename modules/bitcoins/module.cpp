#include "module.h"
#include <jnibridge.h>

namespace bitcoinfuzz {
namespace module {

BitcoinS::BitcoinS(void) : BaseModule("BitcoinS") {}

std::optional<std::string>
BitcoinS::bip32_master_keygen(std::span<const uint8_t> buffer) const {
  return CallJvm("BitcoinSWrapper", "createMasterKey", buffer);
}

std::optional<std::string> BitcoinS::bip32_deserialize_extended_key(
    std::span<const uint8_t> buffer) const {
  return CallJvm("BitcoinSWrapper", "deserializeExtendedKey", buffer);
}

std::optional<std::string>
BitcoinS::psbt_v0_parse(std::span<const uint8_t> buffer) const {
  return CallJvm("BitcoinSWrapper", "parsePSBT", buffer);
}

} // namespace module
} // namespace bitcoinfuzz
