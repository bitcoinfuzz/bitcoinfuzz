#include <span>

#include "module.h"
#include "rust_psbt_lib/rust_psbt_lib.h"

namespace bitcoinfuzz {
namespace module {
RustPsbt::RustPsbt(void) : BaseModule("RustPsbt") {}

std::optional<std::string>
RustPsbt::psbt_v0_parse(std::span<const uint8_t> buffer) const {
  return TakeResult(rust_psbt_psbt_v0_parse(buffer.data(), buffer.size()));
}

std::optional<std::string>
RustPsbt::psbt_v2_parse(std::span<const uint8_t> buffer) const {
  return TakeResult(rust_psbt_psbt_v2_parse(buffer.data(), buffer.size()));
}
} // namespace module
} // namespace bitcoinfuzz
