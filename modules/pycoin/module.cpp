#include "module.h"
#include <pybridge.h>
#include <span>

namespace bitcoinfuzz {
namespace module {

Pycoin::Pycoin(void) : BaseModule("Pycoin") {}

std::optional<std::string>
Pycoin::bip32_master_keygen(std::span<const uint8_t> buffer) const {
  return CallPython("pycoin", "bip32_master_keygen", buffer);
}

std::optional<std::string>
Pycoin::bip32_deserialize_extended_key(std::span<const uint8_t> buffer) const {
  return CallPython("pycoin", "bip32_deserialize_extended_key", buffer);
}

} // namespace module
} // namespace bitcoinfuzz
