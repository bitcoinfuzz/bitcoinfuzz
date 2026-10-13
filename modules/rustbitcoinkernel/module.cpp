#include <span>

#include "module.h"
#include "rustbitcoinkernel_lib/rustbitcoinkernel_lib.h"

namespace bitcoinfuzz {
namespace module {
Rustbitcoinkernel::Rustbitcoinkernel(void) : BaseModule("Rustbitcoinkernel") {}

std::optional<std::string>
Rustbitcoinkernel::kernel_transaction(std::span<const uint8_t> buffer) const {
  return TakeResult(
      rustbitcoinkernel_transaction(buffer.data(), buffer.size()));
}

std::optional<std::string>
Rustbitcoinkernel::kernel_block(std::span<const uint8_t> buffer) const {
  return TakeResult(rustbitcoinkernel_block(buffer.data(), buffer.size()));
}

std::optional<std::string>
Rustbitcoinkernel::kernel_block_check(std::span<const uint8_t> buffer) const {
  return TakeResult(
      rustbitcoinkernel_block_check(buffer.data(), buffer.size()));
}
} // namespace module
} // namespace bitcoinfuzz
