#include "module.h"
#include "musig2_lib/musig2_lib.h"
#include <span>

namespace bitcoinfuzz {
namespace module {

RustMusig2::RustMusig2(void) : BaseModule("RustMusig2") {}

std::optional<std::string>
RustMusig2::musig2_key_agg(std::span<const uint8_t> buffer) const {
  // Input format: [1 byte: num_keys] [33 bytes * num_keys: pubkeys]
  if (buffer.size() < 1)
    return std::nullopt;

  size_t num_keys = buffer[0];
  if (num_keys == 0 || num_keys > 100)
    return std::nullopt;

  size_t expected_size = 1 + (num_keys * 33);
  if (buffer.size() < expected_size)
    return std::nullopt;

  char *result = ::musig2_key_agg(buffer.data() + 1, num_keys);
  if (!result)
    return std::nullopt;

  std::string s(result);
  ::musig2_free_string(result);
  return s;
}

} // namespace module
} // namespace bitcoinfuzz
