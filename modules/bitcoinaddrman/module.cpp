#include "module.h"
#include "bitcoin_addrman_lib/bitcoin_addrman_lib.h"
#include <cstdint>
#include <span>
#include <string>

namespace bitcoinfuzz {
namespace module {
Bitcoinaddrman::Bitcoinaddrman(void) : BaseModule("Bitcoinaddrman") {}

std::optional<std::string>
Bitcoinaddrman::addrman_serialize(std::span<const uint8_t> buffer) const {
  char *p = bitcoin_addrman_serialize(buffer.data(), buffer.size());
  if (!p)
    return std::nullopt;
  std::string result(p);
  bitcoin_addrman_free_string(p);
  return result;
}

} // namespace module
} // namespace bitcoinfuzz
