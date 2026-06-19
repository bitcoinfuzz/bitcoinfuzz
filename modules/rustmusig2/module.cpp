#include "module.h"
#include "musig2_lib/musig2_lib.h"
#include <span>

namespace bitcoinfuzz {
namespace module {

RustMusig2::RustMusig2(void) : BaseModule("RustMusig2") {}

std::optional<std::string>
RustMusig2::musig2_key_agg(std::span<const uint8_t> seckeys) const {
  // Input is num_keys concatenated 32-byte private keys; the Rust side derives
  // the pubkeys and aggregates. Returns null on an invalid scalar (-> nullopt,
  // skipped) or the "AGG_FAIL" sentinel if aggregation itself is rejected.
  size_t num_keys = seckeys.size() / 32;

  char *result = ::musig2_key_agg(seckeys.data(), num_keys);
  if (!result)
    return std::nullopt;

  std::string s(result);
  ::musig2_free_string(result);
  return s;
}

} // namespace module
} // namespace bitcoinfuzz
