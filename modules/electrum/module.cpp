#include "module.h"
#include <pybridge.h>
#include <span>

namespace bitcoinfuzz {
namespace module {

Electrum::Electrum(void) : BaseModule("Electrum") {}

std::optional<std::string>
Electrum::bip32_master_keygen(std::span<const uint8_t> buffer) const {
  return CallPython("electrum", "bip32_master_keygen", buffer);
}

std::optional<std::string> Electrum::bip32_deserialize_extended_key(
    std::span<const uint8_t> buffer) const {
  return CallPython("electrum", "bip32_deserialize_extended_key", buffer);
}

} // namespace module
} // namespace bitcoinfuzz
