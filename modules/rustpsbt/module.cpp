#include <span>

#include "module.h"
#include "rust_psbt_lib/rust_psbt_lib.h"

namespace bitcoinfuzz {
namespace module {
RustPsbt::RustPsbt(void) : BaseModule("RustPsbt") {}

std::optional<std::string>
RustPsbt::psbt_parse(std::span<const uint8_t> buffer) const {
  auto result_ptr = rust_psbt_psbt_parse(buffer.data(), buffer.size());
  if (result_ptr == nullptr)
    return std::nullopt;

  std::string result(result_ptr);
  rust_psbt_free_c_string(result_ptr);
  return result;
}
} // namespace module
} // namespace bitcoinfuzz
