#include "module.h"
#include <pybridge.h>
#include <span>

namespace bitcoinfuzz {
namespace module {
Pybitcoinkernel::Pybitcoinkernel(void) : BaseModule("Pybitcoinkernel") {}

std::optional<std::string>
Pybitcoinkernel::kernel_transaction(std::span<const uint8_t> buffer) const {
  return CallPython("pybitcoinkernel", "transaction_parse", buffer);
}

std::optional<std::string>
Pybitcoinkernel::kernel_block(std::span<const uint8_t> buffer) const {
  return CallPython("pybitcoinkernel", "block_parse", buffer);
}

std::optional<std::string>
Pybitcoinkernel::kernel_block_check(std::span<const uint8_t> buffer) const {
  return CallPython("pybitcoinkernel", "block_check", buffer);
}
} // namespace module
} // namespace bitcoinfuzz
