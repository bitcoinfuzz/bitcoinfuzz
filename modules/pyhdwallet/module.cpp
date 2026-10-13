#include "module.h"
#include <pybridge.h>
#include <span>

namespace bitcoinfuzz {
namespace module {

Pyhdwallet::Pyhdwallet(void) : BaseModule("Pyhdwallet") {}

std::optional<std::string>
Pyhdwallet::bip32_master_keygen(std::span<const uint8_t> buffer) const {
  return CallPython("pyhdwallet", "bip32_master_keygen", buffer);
}

} // namespace module
} // namespace bitcoinfuzz
