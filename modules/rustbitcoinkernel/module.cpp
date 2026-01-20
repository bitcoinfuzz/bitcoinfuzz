#include <span>

#include "module.h"
#include "rustbitcoinkernel_lib/rustbitcoinkernel_lib.h"

namespace bitcoinfuzz {
namespace module {
Rustbitcoinkernel::Rustbitcoinkernel(void) : BaseModule("Rustbitcoinkernel") {}

std::optional<std::string>
Rustbitcoinkernel::kernel_transaction(std::span<const uint8_t> buffer) const {
  auto result_ptr = rustbitcoinkernel_transaction(buffer.data(), buffer.size());
  if (result_ptr == nullptr)
    return std::nullopt;

  std::string result(result_ptr);
  free_c_string(result_ptr);
  return result;
}

std::optional<std::string>
Rustbitcoinkernel::kernel_block(std::span<const uint8_t> buffer) const {
  auto result_ptr = rustbitcoinkernel_block(buffer.data(), buffer.size());
  if (result_ptr == nullptr)
    return std::nullopt;

  std::string result(result_ptr);
  free_c_string(result_ptr);
  return result;
}
} // namespace module
} // namespace bitcoinfuzz
